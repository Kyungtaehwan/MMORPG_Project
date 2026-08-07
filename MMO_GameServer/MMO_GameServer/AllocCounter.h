#pragma once
#include <cstdint>

#include "ServerConfig.h"

// ================================================================
//  힙 할당 카운터
//
//  ★ 왜 필요한가
//    "메모리 풀을 넣을까" 를 묻기 전에 "지금 할당을 몇 번 하나" 를
//    먼저 알아야 한다. 에피소드 10 에서 재지 않고 스핀락부터 만들었다가
//    종단 지표가 안 움직인 적이 있다. 같은 실수를 반복하지 않기 위한 도구다.
//
//  ★ new 라는 글자를 세는 게 아니다
//    코드에서 new 를 찾으면 IOCP_Server.cpp 한 줄뿐이다. 그런데
//    std::vector, std::unordered_set, std::make_shared 는 전부 안에서
//    operator new 를 부른다. 특히 unordered_set 은 원소 하나마다
//    노드를 따로 할당한다(40 개 넣으면 40 회).
//    그래서 전역 operator new 를 갈아끼워 "실제로 힙을 몇 번 두들기나" 를 센다.
//    이렇게 하면 표준 컨테이너든 명시적 new 든 하나도 안 놓친다.
//
//  ★ 이 계측 자체가 측정을 오염시킨다 (에피소드 8)
//    모든 할당마다 원자적 증가가 하나 붙는다. 할당이 초당 수십만 번이면
//    그 증가만으로 수치가 흔들린다.
//    그래서 이 플래그를 켠 상태의 값은 "성능 측정치" 가 아니라
//    "할당 횟수 조사 결과" 로만 쓴다. 성능 표를 채울 때는 반드시 끈다.
//    켜져 있으면 태그에 ALLOC 이 붙어서 표에 잘못 적는 사고를 막는다.
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
