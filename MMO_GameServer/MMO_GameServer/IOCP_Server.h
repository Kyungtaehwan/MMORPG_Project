#pragma once
#include "Session.h"
#include <iomanip>   // setw, setprecision
#include <vector>

class CIOCP_Server
{
public:
    CIOCP_Server();
    ~CIOCP_Server();

    bool Start(uint16_t nPort);
    void Run();

    HANDLE GetIOCPHandle() { return m_hIOCP; }

private:
    bool InitSocket(uint16_t nPort);
    bool InitIOCP();
    void StartAccept();
    void ReRegisterAccept(SessionRef pSession);

    void WorkerThread();

    void ProcessAccept(SessionRef pSession);
    void ProcessRecv(SessionRef pSession, int32_t nNumOfBytes);
    void ProcessSend(SessionRef pSession);

    LPFN_ACCEPTEX                m_fnAcceptEx = nullptr;
    LPFN_GETACCEPTEXSOCKADDRS    m_fnGetAcceptExSockaddrs = nullptr;

    HANDLE  m_hIOCP = INVALID_HANDLE_VALUE;
    SOCKET  m_listenSocket = INVALID_SOCKET;

    static constexpr int32_t ACCEPT_POOL_SIZE = 10;
    std::vector<SessionRef>  m_acceptSessions;
    std::vector<std::thread> m_workerThreads;

    void DebugConsoleThread();
    std::thread m_debugThread;

    void TimerThread();
    std::thread m_timerThread;
};