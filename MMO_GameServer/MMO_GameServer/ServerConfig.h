#pragma once
#include <string>
#include <iostream>
#include <mutex>
#include <type_traits>

#define DISABLE             0
#define ENABLE              1

//  아래 토글 값을 바꾸고  전체 리빌드.
#define RW_LOCK_MUTEX       0   // std::mutex
#define RW_LOCK_SPIN        1   // 구현한 스핀락
#define RW_LOCK_SHARED      2   // std::shared_mutex

#define USE_RW_LOCK         RW_LOCK_SPIN

// 아직 구현 전. 만들 때 이 플래그로 갈라 넣는다.
#define USE_MEMORY_POOL     DISABLE
#define USE_SECTOR_AOI      ENABLE

// 브로드캐스트 시 같은 패킷을 수신자 수만큼 만들지 않고
// 한 번만 직렬화해 shared_ptr 로 참조만 넘긴다. (SendBuffer.h)프리텐다드 Regular
// 전송 횟수는 그대로다 - 그건 다음 단계인 송신 배칭의 몫.
#define USE_SEND_BUFFER     ENABLE

// 힙 할당 횟수
// 성능 측정시에는 병목 방지로 끄기
#define USE_ALLOC_COUNTER   ENABLE

// 섹터 한 변의 타일 수. 반드시 VIEW_RANGE 이상이어야 3x3 섹터가 시야를 덮음
#define SECTOR_SIZE         6


//  락 타입
#if USE_RW_LOCK == RW_LOCK_SPIN
#include "RWLock.h"
using FRWLock     = FRWSpinLock;
using FReadGuard  = FSpinReadGuard;
using FWriteGuard = FSpinWriteGuard;

#elif USE_RW_LOCK == RW_LOCK_SHARED
#include <shared_mutex>
using FRWLock     = std::shared_mutex;
using FReadGuard  = std::shared_lock<std::shared_mutex>;
using FWriteGuard = std::unique_lock<std::shared_mutex>;

#else   // RW_LOCK_MUTEX
using FRWLock     = std::mutex;
using FReadGuard  = std::lock_guard<std::mutex>;
using FWriteGuard = std::lock_guard<std::mutex>;
#endif

// 플래그와 실제 타입이 맞는지 컴파일 타임에 한 번 더 확인
#if   USE_RW_LOCK == RW_LOCK_SPIN
static_assert(std::is_same<FRWLock, FRWSpinLock>::value,
              "USE_RW_LOCK이 SPIN인데 FRWLock이 스핀락이 아니다");
#elif USE_RW_LOCK == RW_LOCK_SHARED
static_assert(std::is_same<FRWLock, std::shared_mutex>::value,
              "USE_RW_LOCK이 SHARED인데 FRWLock이 shared_mutex가 아니다");
#else
static_assert(std::is_same<FRWLock, std::mutex>::value,
              "USE_RW_LOCK이 MUTEX인데 FRWLock이 mutex가 아니다");
#endif

// 한 스코프에서 여러 번 잠글 수 있도록 변수 이름에 줄번호를 붙인다.
//  __LINE__ 이 숫자로 바뀌기 전에 붙어버리는걸 방지하려고 한겹 더 붙여서 전개
#define SC_CONCAT_IMPL(a, b) a##b
#define SC_CONCAT(a, b)      SC_CONCAT_IMPL(a, b)

#define READ_LOCK(mtx)   FReadGuard  SC_CONCAT(scReadLock_,  __LINE__)(mtx)
#define WRITE_LOCK(mtx)  FWriteGuard SC_CONCAT(scWriteLock_, __LINE__)(mtx)


//  시작 시 현재 설정 출력
inline std::string GetServerConfigTag()
{
    std::string tag;
#if   USE_RW_LOCK == RW_LOCK_SPIN
    tag += "RWSPIN+";
#elif USE_RW_LOCK == RW_LOCK_SHARED
    tag += "RWSHARED+";
#endif
#if USE_MEMORY_POOL
    tag += "POOL+";
#endif
#if USE_SECTOR_AOI
    tag += "SECTOR+";
#endif
#if USE_SEND_BUFFER
    tag += "SENDBUF+";
#endif
    // 계측이 켜진 빌드의 수치를 성능 표에 잘못 적는 사고를 막는다.
#if USE_ALLOC_COUNTER
    tag += "ALLOC+";
#endif

    if (tag.empty())
        return "BASE";

    tag.pop_back();     // 끝의 + 제거
    return tag;
}

inline void PrintServerConfig()
{
    const char* lockName =
#if   USE_RW_LOCK == RW_LOCK_SPIN
        "FRWSpinLock (직접 구현)";
#elif USE_RW_LOCK == RW_LOCK_SHARED
        "std::shared_mutex";
#else
        "std::mutex (기준선)";
#endif

    std::cout << "=== 최적화 [" << GetServerConfigTag() << "] ===" << std::endl;
    std::cout << "  RW_LOCK     : " << lockName << std::endl;
    std::cout << "  MEMORY_POOL : " << (USE_MEMORY_POOL ? "ON" : "off") << std::endl;
    std::cout << "  SECTOR_AOI  : " << (USE_SECTOR_AOI  ? "ON" : "off");
#if USE_SECTOR_AOI
    std::cout << "  (SECTOR_SIZE " << SECTOR_SIZE << ")";
#endif
    std::cout << std::endl;
    std::cout << "  SEND_BUFFER : " << (USE_SEND_BUFFER ? "ON" : "off")
              << "  (브로드캐스트 1회 직렬화)" << std::endl;
    std::cout << "  ALLOC_COUNT : " << (USE_ALLOC_COUNTER ? "ON" : "off");
#if USE_ALLOC_COUNTER
    std::cout << "  <- 계측 켜짐. 이 빌드의 수치는 성능 표에 쓰지 말 것";
#endif
    std::cout << std::endl;
    std::cout << "=============================" << std::endl;
}
