#pragma once
// ================================================================
//  StressMetrics — 부하 테스트용 서버 계측 (헤더 온리, atomics)
//
//  목적: 패킷 1건당 "핸들러 처리 시간"(락 대기·GetNearPlayers 포함)을
//        마이크로초 단위로 모아 p50/p99/max·평균·초당 처리량(pps)과
//        워커 스레드별 처리 건수를 디버그 콘솔에 출력한다.
//
//  사용:
//   - CPacket_Handler::Handle 진입/종료에서 Now()/Record(ElapsedUs()).
//   - DebugConsoleThread 가 주기적으로 SnapshotAndReset() 호출 -> 출력.
//
//  설계: 콜당 QueryPerformanceCounter 2회 + relaxed atomic 몇 번(수십 ns).
//        고정 크기 히스토그램(2의 거듭제곱 us 버킷)이라 할당·락 없음.
//        스냅샷 시 버킷을 exchange(0) 하므로 "직전 구간" 통계가 된다.
//        인라인 변수(C++17) 대신 함수 로컬 static 으로 상태를 공유한다.
// ================================================================
#include <atomic>
#include <cstdint>
#include <windows.h>

namespace StressMetrics
{
    constexpr int BUCKET_COUNT = 32;   // 버킷 i = [2^i, 2^(i+1)) us
    constexpr int MAX_WORKERS = 64;

    struct State
    {
        std::atomic<uint32_t> buckets[BUCKET_COUNT];
        std::atomic<uint64_t> sumUs;
        std::atomic<uint32_t> maxUs;
        std::atomic<uint64_t> worker[MAX_WORKERS];
        std::atomic<int>      workerNext;

        // ---- AOI(GetNearPlayers) 계측 ----
        std::atomic<uint64_t> aoiCalls;      // 호출 횟수
        std::atomic<uint64_t> aoiSumUs;      // 총 소요 us(락 대기+스캔)
        std::atomic<uint64_t> aoiSumScanned; // 총 순회한 플레이어 수(평균 N 산출용)
        std::atomic<uint32_t> aoiMaxUs;

        State() : sumUs(0), maxUs(0), workerNext(0),
                  aoiCalls(0), aoiSumUs(0), aoiSumScanned(0), aoiMaxUs(0)
        {
            for (int i = 0; i < BUCKET_COUNT; ++i) buckets[i].store(0);
            for (int i = 0; i < MAX_WORKERS; ++i)  worker[i].store(0);
        }
    };

    // 모든 TU 가 같은 인스턴스를 공유(inline 함수의 로컬 static)
    inline State& S()
    {
        static State s;
        return s;
    }

    // 이 스레드의 워커 인덱스(첫 패킷 처리 때 지연 할당)
    inline int WorkerIndex()
    {
        thread_local int idx = -1;
        if (idx < 0)
        {
            idx = S().workerNext.fetch_add(1, std::memory_order_relaxed);
            if (idx >= MAX_WORKERS) idx = MAX_WORKERS - 1;
        }
        return idx;
    }

    inline int BucketOf(uint32_t us)
    {
        int b = 0;
        while (us >= 2 && b < BUCKET_COUNT - 1) { us >>= 1; ++b; }
        return b;
    }

    // ---- QueryPerformanceCounter 헬퍼 ----
    inline int64_t QpcFreq()
    {
        static int64_t f = [] {
            LARGE_INTEGER li; QueryPerformanceFrequency(&li); return li.QuadPart;
        }();
        return f;
    }
    inline int64_t Now()
    {
        LARGE_INTEGER li; QueryPerformanceCounter(&li); return li.QuadPart;
    }
    inline uint32_t ElapsedUs(int64_t nStart)
    {
        int64_t d = Now() - nStart;
        if (d < 0) d = 0;
        return static_cast<uint32_t>(d * 1000000 / QpcFreq());
    }

    // 패킷 처리 1건 기록
    inline void Record(uint32_t us)
    {
        State& st = S();
        st.buckets[BucketOf(us)].fetch_add(1, std::memory_order_relaxed);
        st.sumUs.fetch_add(us, std::memory_order_relaxed);
        st.worker[WorkerIndex()].fetch_add(1, std::memory_order_relaxed);
        uint32_t cur = st.maxUs.load(std::memory_order_relaxed);
        while (us > cur &&
               !st.maxUs.compare_exchange_weak(cur, us, std::memory_order_relaxed)) {}
    }

    // GetNearPlayers 1회 기록 (us = 락대기+스캔 시간, nScanned = 순회한 플레이어 수)
    inline void RecordAoi(uint32_t us, uint32_t nScanned)
    {
        State& st = S();
        st.aoiCalls.fetch_add(1, std::memory_order_relaxed);
        st.aoiSumUs.fetch_add(us, std::memory_order_relaxed);
        st.aoiSumScanned.fetch_add(nScanned, std::memory_order_relaxed);
        uint32_t cur = st.aoiMaxUs.load(std::memory_order_relaxed);
        while (us > cur &&
               !st.aoiMaxUs.compare_exchange_weak(cur, us, std::memory_order_relaxed)) {}
    }

    struct AoiSnapshot
    {
        uint64_t calls = 0;       // 직전 구간 호출 횟수
        uint64_t sumUs = 0;       // 총 소요 us
        uint64_t sumScanned = 0;  // 총 순회 플레이어 수
        uint32_t maxUs = 0;
    };
    inline AoiSnapshot SnapshotAoiAndReset()
    {
        State& st = S();
        AoiSnapshot s;
        s.calls      = st.aoiCalls.exchange(0, std::memory_order_relaxed);
        s.sumUs      = st.aoiSumUs.exchange(0, std::memory_order_relaxed);
        s.sumScanned = st.aoiSumScanned.exchange(0, std::memory_order_relaxed);
        s.maxUs      = st.aoiMaxUs.exchange(0, std::memory_order_relaxed);
        return s;
    }

    struct Snapshot
    {
        uint64_t count;      // 직전 구간 처리 패킷 수
        double   avgUs;
        uint32_t p50Us;      // 버킷 하한 근사값
        uint32_t p99Us;
        uint32_t maxUs;
        uint64_t worker[MAX_WORKERS];
        int      workerCount;

        Snapshot() : count(0), avgUs(0.0), p50Us(0), p99Us(0),
                     maxUs(0), workerCount(0)
        {
            for (int i = 0; i < MAX_WORKERS; ++i) worker[i] = 0;
        }
    };

    // 스냅샷 + 리셋. 콘솔 스레드가 주기적으로 호출.
    inline Snapshot SnapshotAndReset()
    {
        State& st = S();
        Snapshot s;
        uint32_t buckets[BUCKET_COUNT];
        uint64_t total = 0;
        for (int i = 0; i < BUCKET_COUNT; ++i)
        {
            buckets[i] = st.buckets[i].exchange(0, std::memory_order_relaxed);
            total += buckets[i];
        }
        s.count = total;
        uint64_t sum = st.sumUs.exchange(0, std::memory_order_relaxed);
        s.maxUs = st.maxUs.exchange(0, std::memory_order_relaxed);
        s.avgUs = total ? static_cast<double>(sum) / total : 0.0;

        auto pct = [&](double p) -> uint32_t {
            uint64_t target = static_cast<uint64_t>(total * p);
            if (target == 0) target = 1;
            uint64_t acc = 0;
            for (int i = 0; i < BUCKET_COUNT; ++i)
            {
                acc += buckets[i];
                if (acc >= target) return (i == 0) ? 1u : (1u << i);
            }
            return 0;
        };
        s.p50Us = total ? pct(0.50) : 0;
        s.p99Us = total ? pct(0.99) : 0;

        s.workerCount = st.workerNext.load(std::memory_order_relaxed);
        if (s.workerCount > MAX_WORKERS) s.workerCount = MAX_WORKERS;
        for (int i = 0; i < s.workerCount; ++i)
            s.worker[i] = st.worker[i].exchange(0, std::memory_order_relaxed);
        return s;
    }
}
