#pragma once
#include "Session.h"
#include <array>
#include <mutex>

constexpr int32_t MAX_SESSION = 20000;

// ================================================================
//  CSessionManager 세션 슬롯 풀
// ================================================================
class CSession_Manager
{
private:
    CSession_Manager();
    ~CSession_Manager() = default;

public:
    static CSession_Manager* Get_Instance()
    {
        if (!m_pInstance)
            m_pInstance = new CSession_Manager;
        return m_pInstance;
    }

    static void Destroy_Instance()
    {
        if (m_pInstance)
        {
            delete m_pInstance;
            m_pInstance = nullptr;
        }
    }

    // 복사 방지
    CSession_Manager(const CSession_Manager&) = delete;
    CSession_Manager& operator=(const CSession_Manager&) = delete;

    int32_t    Assign();
    SessionRef Get_Session(int32_t nID);
    void       Release(int32_t nID);
    int32_t    GetCount() { return m_count.load(); }


    // - 증가: ProcessAccept가 SetConnected(true) 직후
    // - 감소: CSession::Disconnect의 exchange를 통과한 스레드가 한 번만
    void       OnConnected() { m_connectedCount.fetch_add(1); }
    void       OnDisconnected() { m_connectedCount.fetch_sub(1); }
    int32_t    GetConnectedCount() { return m_connectedCount.load(); }

private:
    static CSession_Manager* m_pInstance;

    std::array<SessionRef, MAX_SESSION> m_sessions;


    FRWLock                             m_lock;    // 읽기/쓰기 락

    std::atomic<int32_t>                m_count = 0;            // 할당된 슬롯 수(대기 + 접속)
    std::atomic<int32_t>                m_connectedCount = 0;   // 그중 실제 접속 중인 수
};