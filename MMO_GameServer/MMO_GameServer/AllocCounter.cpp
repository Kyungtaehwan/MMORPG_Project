#include "pch.h"
#include "AllocCounter.h"

#if USE_ALLOC_COUNTER

#include <atomic>
#include <cstdlib>
#include <new>

// ================================================================
//  전역 operator 
// / delete 교체
//
//  순서 보장 필요 X  => memory_order_relaxed
//  delete 를 네 종류 다 정의해야 한다
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
