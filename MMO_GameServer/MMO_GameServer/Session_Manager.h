#pragma once
#include "Session.h"
#include <array>
#include <mutex>

constexpr int32_t MAX_SESSION = 20000;

// ================================================================
//  CSessionManager ººº« ΩΩ∑‘ «Æ (ΩÃ±€≈Ê)
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

    // ∫πªÁ πÊ¡ˆ
    CSession_Manager(const CSession_Manager&) = delete;
    CSession_Manager& operator=(const CSession_Manager&) = delete;

    int32_t    Assign();
    SessionRef Get_Session(int32_t nID);
    void       Release(int32_t nID);
    int32_t    GetCount() { return m_count.load(); }

private:
    static CSession_Manager* m_pInstance;

    std::array<SessionRef, MAX_SESSION> m_sessions;
    std::mutex                          m_lock;
    std::atomic<int32_t>                m_count = 0;
};