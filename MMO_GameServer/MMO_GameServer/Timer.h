#pragma once
#include <cstdint>
#include <chrono>
#include <queue>
#include <vector>
#include <mutex>

enum class EEventType : uint8_t
{
    MonsterAI = 0,
    MonsterRespawn = 1,
    MonsterAttackHit = 2,
};

struct FTimerEvent
{
    int32_t    nID;
    EEventType eType;
    std::chrono::system_clock::time_point wakeupTime;

    bool operator>(const FTimerEvent& other) const
    {
        return wakeupTime > other.wakeupTime;
    }
};

extern std::priority_queue<FTimerEvent, std::vector<FTimerEvent>, std::greater<FTimerEvent>> g_timerQueue;
extern std::mutex g_timerLock;

void AddTimer(int32_t nID, EEventType eType, uint32_t nDelayMs);