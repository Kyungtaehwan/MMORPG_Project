#pragma once
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <intrin.h>


//  잠금 규칙이 깨졌을 때 즉시 터트림
#define RWLOCK_CRASH(cause)                                             \
{                                                                       \
    std::cout << "[RWLock] " << (cause) << std::endl;                   \
    uint32_t* pCrash = nullptr;                                         \
    __analysis_assume(pCrash != nullptr);                               \
    *pCrash = 0xDEADBEEF;                                               \
}

// ----------------------------------------------------------------
//  스레드마다 고유한 16비트 번호
//  Windows의 GetCurrentThreadId()는 16비트를 넘을 수 있어 직접 발급
// ----------------------------------------------------------------
inline uint32_t GetLockThreadId()
{
    static std::atomic<uint32_t> s_nextId{ 1 };
    thread_local uint32_t tlsId = (s_nextId.fetch_add(1) & 0xFFFF) << 16;
    return tlsId;
}

class FRWSpinLock
{
    enum : uint32_t
    {
        WRITE_THREAD_MASK = 0xFFFF0000,   // 상위 16비트 - 쓰기 스레드 번호
        READ_COUNT_MASK   = 0x0000FFFF,   // 하위 16비트 - 읽기 개수
        EMPTY_FLAG        = 0x00000000,   // 아무도 안 쓰는 상태


        MAX_SPIN_COUNT    = 4000,   //스핀락 카운트
        ACQUIRE_TIMEOUT_MS = 10000, //데드락 시간
    };

public:
    FRWSpinLock() = default;
    FRWSpinLock(const FRWSpinLock&) = delete;
    FRWSpinLock& operator=(const FRWSpinLock&) = delete;


    //  WriteLock - 나 혼자만 들어간다(Read 있으면 대기)
    void WriteLock()
    {
        const uint32_t myId = GetLockThreadId();

        // 이미 내가 쓰기 잠금을 쥐고 있으면 횟수만 올리고 통과한다(재귀 허용).
        if ((m_lockFlag.load(std::memory_order_relaxed) & WRITE_THREAD_MASK) == myId)
        {
            ++m_writeCount;
            return;
        }

        const auto beginTime = std::chrono::steady_clock::now();

        while (true)
        {
            for (uint32_t spin = 0; spin < MAX_SPIN_COUNT; ++spin)
            {
                // 완전히 비어 있을 때만(읽기도 0) 내 번호를 새긴다.
                // CAS는 실패하면 expected에 현재값을 덮어쓰므로 매번 초기화한다.
                uint32_t expected = EMPTY_FLAG;
                if (m_lockFlag.compare_exchange_strong(
                        expected, myId, std::memory_order_acquire))
                {
                    m_writeCount = 1;
                    return;
                }
                _mm_pause();   // 스핀 중이라고 CPU에 알려 경쟁을 줄인다
            }

            if (ElapsedMs(beginTime) >= ACQUIRE_TIMEOUT_MS)
                RWLOCK_CRASH("WriteLock 타임아웃 - 데드락으로 판단");

            std::this_thread::yield();
        }
    }

    void WriteUnlock()
    {
        // 읽기가 남아 있는데 쓰기를 푸는 것은 잠금 순서가 꼬였다는 뜻
        if ((m_lockFlag.load(std::memory_order_relaxed) & READ_COUNT_MASK) != 0)
            RWLOCK_CRASH("WriteUnlock 전에 ReadUnlock을 먼저 해야 한다");

        if (--m_writeCount == 0)
            m_lockFlag.store(EMPTY_FLAG, std::memory_order_release);
    }


    //  ReadLock - 쓰기만 없으면 몇 개든 동시에 통과
    void ReadLock()
    {
        const uint32_t myId = GetLockThreadId();

        // 내가 쓰기 잠금을 쥔 상태에서의 읽기는 그냥 통과시킨다.
        // (쓰기 구간 안에서 읽기 함수를 부르는 경우)
        if ((m_lockFlag.load(std::memory_order_relaxed) & WRITE_THREAD_MASK) == myId)
        {
            m_lockFlag.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        const auto beginTime = std::chrono::steady_clock::now();

        while (true)
        {
            for (uint32_t spin = 0; spin < MAX_SPIN_COUNT; ++spin)
            {
                // 쓰기 스레드가 없는 상태(상위 16비트가 0)에서만 읽기 수를 늘린다.
                // expected에서 상위 비트를 떼어내므로, 쓰기 중이면 CAS가 실패한다.
                uint32_t expected = m_lockFlag.load(std::memory_order_relaxed)
                                  & READ_COUNT_MASK;
                if (m_lockFlag.compare_exchange_strong(
                        expected, expected + 1, std::memory_order_acquire))
                    return;

                _mm_pause();
            }

            if (ElapsedMs(beginTime) >= ACQUIRE_TIMEOUT_MS)
                RWLOCK_CRASH("ReadLock 타임아웃 - 데드락으로 판단");

            std::this_thread::yield();
        }
    }

    void ReadUnlock()
    {
        // fetch_sub이 돌려주는 빼기 직전 값으로 검사.
        // 먼저 읽고 나서 빼면 그 사이에 다른 스레드가 끼어들 수 있으므로 반환값을 쓰는 쪽이 정확하다.
        if ((m_lockFlag.fetch_sub(1, std::memory_order_release) & READ_COUNT_MASK) == 0)
            RWLOCK_CRASH("잠그지 않은 읽기를 풀었다 - 읽기 카운트 언더플로");
    }

private:
    // 스핀을 시작한 뒤 지난 시간(ms).
    static uint64_t ElapsedMs(const std::chrono::steady_clock::time_point& begin)
    {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - begin).count());
    }

    std::atomic<uint32_t> m_lockFlag{ EMPTY_FLAG };

    // 쓰기 재귀 횟수. 쓰기 잠금을 쥔 스레드만 건드리므로 atomic이 아니어도 된다.
    uint16_t              m_writeCount = 0;
};

//  RAII
//  std::lock_guard와 사용법을 맞춰서 매크로가 세 방식 모두에 통하게 한다.
// ----------------------------------------------------------------
class FSpinReadGuard
{
public:
    explicit FSpinReadGuard(FRWSpinLock& lock) : m_lock(lock) { m_lock.ReadLock(); }
    ~FSpinReadGuard() { m_lock.ReadUnlock(); }

    FSpinReadGuard(const FSpinReadGuard&) = delete;
    FSpinReadGuard& operator=(const FSpinReadGuard&) = delete;

private:
    FRWSpinLock& m_lock;
};

class FSpinWriteGuard
{
public:
    explicit FSpinWriteGuard(FRWSpinLock& lock) : m_lock(lock) { m_lock.WriteLock(); }
    ~FSpinWriteGuard() { m_lock.WriteUnlock(); }

    FSpinWriteGuard(const FSpinWriteGuard&) = delete;
    FSpinWriteGuard& operator=(const FSpinWriteGuard&) = delete;

private:
    FRWSpinLock& m_lock;
};
