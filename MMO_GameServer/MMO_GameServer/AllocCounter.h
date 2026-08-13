#pragma once
#include <cstdint>

#include "ServerConfig.h"

// ================================================================
//  힙 할당 카운터
// ================================================================

#if USE_ALLOC_COUNTER

namespace AllocMetrics
{
    struct Snapshot
    {
        uint64_t allocs = 0;   // 구간 내 operator new 호출 수
        uint64_t frees  = 0;   // 구간 내 operator delete 호출 수
        uint64_t bytes  = 0;   // 구간 내 요청된 바이트 합
    };

    // 직전 호출 이후의 누적치를 반환하고 0 으로 되돌린다.
    // StressMetrics::SnapshotAndReset 과 같은 방식이다.
    Snapshot SnapshotAndReset();
}

#endif
