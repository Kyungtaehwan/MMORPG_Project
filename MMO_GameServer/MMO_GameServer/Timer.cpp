#include "pch.h"
#include "Timer.h"

std::priority_queue<FTimerEvent, std::vector<FTimerEvent>, std::greater<FTimerEvent>> g_timerQueue;

std::mutex g_timerLock;

void AddTimer(int32_t nID, EEventType eType, uint32_t nDelayMs)
{
    FTimerEvent ev;
    ev.nID = nID;
    ev.eType = eType;
    ev.wakeupTime = std::chrono::system_clock::now()
        + std::chrono::milliseconds(nDelayMs);

    std::lock_guard<std::mutex> lock(g_timerLock);
    g_timerQueue.push(ev);
}