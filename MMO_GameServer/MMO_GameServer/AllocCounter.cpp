#include "pch.h"
#include "AllocCounter.h"

#if USE_ALLOC_COUNTER

#include <atomic>
#include <cstdlib>
#include <new>

// ================================================================
//  전역 operator new / delete 교체
//
//  ★ 카운터가 전역 원자 변수인 이유
//    할당은 워커 16 개가 동시에 한다. 보통 변수로 두면 값이 깨진다.
//    다만 정확한 순서는 필요 없고 "몇 번" 만 알면 되므로
//    memory_order_relaxed 로 둔다. 순서 보장을 요구하면 그 자체가
//    측정 대상보다 비싸진다.
//
//  ★ 정적 초기화 순서 문제가 없다
//    std::atomic<uint64_t> 의 값 초기화는 상수 초기화라
//    프로그램 로드 시점에 이미 0 이다. main 보다 먼저 일어나는
//    CRT 내부 할당도 안전하게 세진다.
//
//  ★ delete 를 네 종류 다 정의해야 한다
//    C++14 부터 컴파일러가 크기를 아는 자리에서는 sized delete 를 부른다.
//    하나라도 빼먹으면 그 경로는 기본 구현으로 가고, malloc 으로 잡은
//    메모리를 다른 방식으로 해제하려다 깨진다.
// ================================================================

namespace
{
    std::atomic<uint64_t> g_allocs{ 0 };
    std::atomic<uint64_t> g_frees{ 0 };
    std::atomic<uint64_t> g_bytes{ 0 };
}

namespace AllocMetrics
{
    Snapshot SnapshotAndReset()
    {
        Snapshot s;
        s.allocs = g_allocs.exchange(0, std::memory_order_relaxed);
        s.frees  = g_frees.exchange(0, std::memory_order_relaxed);
        s.bytes  = g_bytes.exchange(0, std::memory_order_relaxed);
        return s;
    }
}

void* operator new(size_t nSize)
{
    g_allocs.fetch_add(1, std::memory_order_relaxed);
    g_bytes.fetch_add(nSize, std::memory_order_relaxed);

    // malloc(0) 은 구현에 따라 nullptr 을 줄 수 있어 최소 1 로 올린다.
    void* p = std::malloc(nSize ? nSize : 1);
    if (!p) throw std::bad_alloc();
    return p;
}

void* operator new[](size_t nSize)
{
    return operator new(nSize);
}

void* operator new(size_t nSize, const std::nothrow_t&) noexcept
{
    g_allocs.fetch_add(1, std::memory_order_relaxed);
    g_bytes.fetch_add(nSize, std::memory_order_relaxed);
    return std::malloc(nSize ? nSize : 1);
}

void* operator new[](size_t nSize, const std::nothrow_t& tag) noexcept
{
    return operator new(nSize, tag);
}

void operator delete(void* p) noexcept
{
    if (p) g_frees.fetch_add(1, std::memory_order_relaxed);
    std::free(p);
}

void operator delete[](void* p) noexcept              { operator delete(p); }
void operator delete(void* p, size_t) noexcept        { operator delete(p); }
void operator delete[](void* p, size_t) noexcept      { operator delete(p); }

void operator delete(void* p, const std::nothrow_t&) noexcept   { operator delete(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { operator delete(p); }

#endif  // USE_ALLOC_COUNTER
