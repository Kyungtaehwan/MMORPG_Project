#pragma once
#include <string>
#include <iostream>
#include <mutex>

//  아래 토글 값을 바꾸고  전체 리빌드.
#define RW_LOCK_MUTEX       0   // std::mutex
#define RW_LOCK_SPIN        1   // 구현한 스핀락
#define RW_LOCK_SHARED      2   // std::shared_mutex

#define USE_RW_LOCK         RW_LOCK_MUTEX

// 아직 구현 전. 만들 때 이 플래그로 갈라 넣는다.
#define USE_MEMORY_POOL     0
#define USE_SECTOR_AOI      0


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

// 한 스코프에서 여러 번 잠글 수 있도록 변수 이름에 줄번호를 붙인다.
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
    std::cout << "  SECTOR_AOI  : " << (USE_SECTOR_AOI  ? "ON" : "off") << std::endl;
    std::cout << "=============================" << std::endl;
}
