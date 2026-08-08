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
//        고정 크기 히스토그램이라 할당·락 없음.
//        스냅샷 시 버킷을 exchange(0) 하므로 "직전 구간" 통계가 된다.
//        인라인 변수(C++17) 대신 함수 로컬 static 으로 상태를 공유한다.
//
//  히스토그램 해상도 (2026-08-08 교체, 중요):
//   예전에는 버킷 i = [2^i, 2^(i+1)) us 인 순수 로그 버킷(32칸)이었다.
//   칸 하나가 2배 범위를 삼키므로 p99 오차가 최대 100% 였고, 실제로
//   봇 1000/1500/2000 세 부하에서 p99 가 전부 "8.2ms"로 똑같이 나왔다.
//   8192us = 2^13 -> 8.2ms 와 16.4ms 가 같은 칸이라 변화를 못 본 것이다.
//   (측정값이 아니라 칸 라벨을 읽고 있었다.)
//
//   순수 선형 1us 배열로 가면 5초 범위에 500만 칸(20MB)이라 캐시가 깨진다.
//   그래서 HdrHistogram 과 같은 방식을 쓴다: 2배 구간(옥타브)은 유지하되
//   각 옥타브를 다시 8칸으로 선형 분할한다. 오차 100% -> 12.5%,
//   칸 수 32 -> 240(1KB 미만)이라 여전히 캐시에 들어간다.
// ================================================================
#include <atomic>
#include <cstdint>
#include <intrin.h>
#include <windows.h>

namespace StressMetrics
{
    // 옥타브(2배 구간) 하나를 2^SUB_BITS 칸으로 나눈다. 3 -> 8칸 -> 오차 12.5%
    constexpr int SUB_BITS    = 3;
    constexpr int SUB_COUNT   = 1 << SUB_BITS;   // 8
    constexpr int BUCKET_COUNT = 256;            // uint32 전 범위(최대 인덱스 239)

    struct State
    {
        std::atomic<uint32_t> buckets[BUCKET_COUNT];
        std::atomic<uint64_t> sumUs;
        std::atomic<uint32_t> maxUs;

        // ---- AOI(GetNearPlayers) 계측 ----
        std::atomic<uint64_t> aoiCalls;      // 호출 횟수
        std::atomic<uint64_t> aoiSumUs;      // 총 소요 us(락 대기+스캔)
        std::atomic<uint64_t> aoiSumScanned; // 총 순회한 플레이어 수(평균 N 산출용)
        std::atomic<uint32_t> aoiMaxUs;

        State() : sumUs(0), maxUs(0),
                  aoiCalls(0), aoiSumUs(0), aoiSumScanned(0), aoiMaxUs(0)
        {
            for (int i = 0; i < BUCKET_COUNT; ++i) buckets[i].store(0);
        }
    };

    // 모든 TU 가 같은 인스턴스를 공유(inline 함수의 로컬 static)
    inline State& S()
    {
        static State s;
        return s;
    }

    // ---- HdrHistogram 방식 버킷 ----
    //  us < 8            : 그대로 0..7 칸 (1us 해상도)
    //  us >= 8           : 옥타브를 8등분. 같은 칸에 들어가는 값의 폭은
    //                      항상 그 값의 1/8 이하 -> 상대오차 12.5% 보장.
    //  인덱스가 값에 대해 단조증가하므로 백분위 계산(누적합)이 그대로 성립한다.
    inline int BucketOf(uint32_t us)
    {
        if (us < SUB_COUNT) return static_cast<int>(us);

        unsigned long oct;                       // floor(log2(us))
        _BitScanReverse(&oct, us);
        const int shift = static_cast<int>(oct) - SUB_BITS;
        const int sub   = static_cast<int>((us >> shift) & (SUB_COUNT - 1));
        return (shift + 1) * SUB_COUNT + sub;
    }

    // 그 칸에 들어갈 수 있는 "가장 큰 값".
    //  p99 를 이 값으로 보고해야 "99%가 이 값 이하였다"가 참이 된다.
    //  (예전처럼 칸의 하한을 보고하면 그 문장이 거짓이 된다.)
    inline uint32_t BucketUpperUs(int i)
    {
        if (i < SUB_COUNT) return static_cast<uint32_t>(i);
        const int shift = i / SUB_COUNT - 1;
        const int sub   = i % SUB_COUNT;
        return static_cast<uint32_t>(((SUB_COUNT + sub + 1) << shift) - 1);
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
        double   avgUs;      // 화면에는 안 띄운다(평균은 꼬리를 가림). 검산용으로만 유지.
        uint32_t p50Us;      // 그 값이 들어간 칸의 상한(오차 <= 12.5%)
        uint32_t p99Us;
        uint32_t maxUs;
        uint64_t sumUs;      // 이 구간의 총 소요 us (긴 창을 합산할 때 필요)

        // 원본 칸 개수. 호출자가 여러 구간을 더해 "긴 창"의 백분위를 낼 수 있게 넘긴다.
        //  백분위수는 평균낼 수 없으므로, 0.5초짜리 p99 를 여러 개 평균내면 안 되고
        //  반드시 이 칸 개수를 더한 뒤 한 번만 계산해야 한다.
        uint32_t buckets[BUCKET_COUNT];

        Snapshot() : count(0), avgUs(0.0), p50Us(0), p99Us(0), maxUs(0), sumUs(0)
        {
            for (int i = 0; i < BUCKET_COUNT; ++i) buckets[i] = 0;
        }
    };

    // 이미 모아둔 칸 개수 배열에서 백분위를 뽑는다(긴 창 요약용).
    //  누적은 호출자가 하고, 계산은 창 끝에서 한 번만 한다.
    template <typename T>
    inline uint32_t PercentileOf(const T* buckets, uint64_t total, double p)
    {
        if (total == 0) return 0;
        uint64_t target = static_cast<uint64_t>(total * p);
        if (target == 0) target = 1;
        uint64_t acc = 0;
        for (int i = 0; i < BUCKET_COUNT; ++i)
        {
            acc += buckets[i];
            if (acc >= target) return BucketUpperUs(i);
        }
        return 0;
    }

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
        s.sumUs = sum;
        s.avgUs = total ? static_cast<double>(sum) / total : 0.0;
        for (int i = 0; i < BUCKET_COUNT; ++i) s.buckets[i] = buckets[i];

        // 낮은 값부터 개수를 더해가다 전체의 p 비율을 넘기는 칸의 "상한"을 돌려준다.
        auto pct = [&](double p) -> uint32_t {
            uint64_t target = static_cast<uint64_t>(total * p);
            if (target == 0) target = 1;
            uint64_t acc = 0;
            for (int i = 0; i < BUCKET_COUNT; ++i)
            {
                acc += buckets[i];
                if (acc >= target) return BucketUpperUs(i);
            }
            return 0;
        };
        s.p50Us = total ? pct(0.50) : 0;
        s.p99Us = total ? pct(0.99) : 0;
        return s;
    }
}
