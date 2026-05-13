#include "pch.h"
#include "IOCP_Server.h"
#include "Session_Manager.h"
#include "Player_Manager.h"
#include "Timer.h"
#include "Monster_Manager.h" 
#include "Zone_Manager.h"
#include "Protocol.h"
#include <iostream>
#include <cstring>

CIOCP_Server::CIOCP_Server() {}

CIOCP_Server::~CIOCP_Server()
{
    if (m_listenSocket != INVALID_SOCKET)
        closesocket(m_listenSocket);
    if (m_hIOCP != INVALID_HANDLE_VALUE)
        CloseHandle(m_hIOCP);
    WSACleanup();
}

bool CIOCP_Server::Start(uint16_t nPort)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cout << "[CIOCPServer] WSAStartup 실패" << std::endl;
        return false;
    }

    if (!InitIOCP())        return false;
    if (!InitSocket(nPort)) return false;

    // AcceptEx 함수 포인터 획득
    // 교수님 코드는 전역 AcceptEx를 바로 쓰지만
    // 우리는 런타임에 포인터를 받아야 함
    GUID guidAcceptEx = WSAID_ACCEPTEX;
    DWORD dwBytes = 0;
    WSAIoctl(m_listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidAcceptEx, sizeof(guidAcceptEx),
        &m_fnAcceptEx, sizeof(m_fnAcceptEx),
        &dwBytes, nullptr, nullptr);

    if (m_fnAcceptEx == nullptr)
    {
        std::cout << "[CIOCPServer] AcceptEx 포인터 획득 실패" << std::endl;
        return false;
    }

    StartAccept();

    int32_t nThreadCount = static_cast<int32_t>(
        std::thread::hardware_concurrency());
    m_debugThread = std::thread(&CIOCP_Server::DebugConsoleThread, this);
    m_debugThread.detach();

    m_timerThread = std::thread(&CIOCP_Server::TimerThread, this);
    m_timerThread.detach();
    for (int32_t i = 0; i < nThreadCount; ++i)
        m_workerThreads.emplace_back(&CIOCP_Server::WorkerThread, this);

    //std::cout << "[CIOCPServer] 시작. 포트=" << nPort
    //    << " Worker스레드=" << nThreadCount << std::endl;
    return true;
}

void CIOCP_Server::Run()
{


    for (auto& t : m_workerThreads)
        t.join();
}

bool CIOCP_Server::InitIOCP()
{
    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (m_hIOCP == INVALID_HANDLE_VALUE)
    {
        std::cout << "[CIOCPServer] IOCP 생성 실패" << std::endl;
        return false;
    }
    return true;
}

bool CIOCP_Server::InitSocket(uint16_t nPort)
{
    m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
        nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (m_listenSocket == INVALID_SOCKET)
    {
        std::cout << "[CIOCPServer] 소켓 생성 실패" << std::endl;
        return false;
    }

    BOOL bReuseAddr = TRUE;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&bReuseAddr), sizeof(bReuseAddr));

    // 리슨 소켓을 IOCP에 등록
    // 교수님 코드: CreateIoCompletionPort(g_s_socket, h_iocp, 9999, 0)
    // key=0으로 등록 (Accept 완료는 key가 아닌 IOEvent로 구분)
    CreateIoCompletionPort(
        reinterpret_cast<HANDLE>(m_listenSocket), m_hIOCP, 0, 0);

    SOCKADDR_IN addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(nPort);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(m_listenSocket,
        reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        std::cout << "[CIOCPServer] bind 실패: " << WSAGetLastError() << std::endl;
        return false;
    }

    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cout << "[CIOCPServer] listen 실패: " << WSAGetLastError() << std::endl;
        return false;
    }

    //std::cout << "[CIOCPServer] 리슨 소켓 준비 완료" << std::endl;
    return true;
}

// ================================================================
//  StartAccept
//
//  교수님 코드와 비교:
//    교수님: g_c_socket 하나 + g_a_over 하나
//    우리:   세션 슬롯 ACCEPT_POOL_SIZE개 미리 준비
//
//  Accept 완료 후 재등록 시 새 세션 슬롯을 Assign해서 사용
// ================================================================
void CIOCP_Server::StartAccept()
{
    for (int32_t i = 0; i < ACCEPT_POOL_SIZE; ++i)
    {
        int32_t nID = CSession_Manager::Get_Instance()->Assign();
        if (nID == -1) break;

        SessionRef pSession = CSession_Manager::Get_Instance()->Get_Session(nID);

        SOCKET clientSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
            nullptr, 0, WSA_FLAG_OVERLAPPED);
        pSession->SetSocket(clientSocket);

        // 여기서 IOCP 등록 (소켓 생성 직후 한 번만)
        CreateIoCompletionPort(
            reinterpret_cast<HANDLE>(clientSocket),
            m_hIOCP,
            static_cast<ULONG_PTR>(nID), 0);

        m_acceptSessions.push_back(pSession);
        ReRegisterAccept(pSession);
    }
}

// ================================================================
//  ReRegisterAccept
//
//  교수님 코드와 비교:
//    교수님:
//      CreateIoCompletionPort(g_c_socket, h_iocp, client_id, 0)  // Accept 후
//      AcceptEx(g_s_socket, g_c_socket, g_a_over._send_buf, ...)
//
//    우리:
//      IOCP 등록 → AcceptEx 순서
//      g_a_over._send_buf 역할 = pSession->GetAcceptBuf()
//                              = m_acceptEvent.m_acceptBuf
//                              (CIOEvent 안에 포함된 버퍼)
// ================================================================
void CIOCP_Server::ReRegisterAccept(SessionRef pSession)
{
    //std::cout << "[ReRegisterAccept] SessionID=" << pSession->GetID()
    //    << " Socket=" << pSession->GetSocket() << std::endl;

    pSession->GetAcceptEvent()->Reset();
    pSession->Initialize();

    DWORD dwBytesReceived = 0;
    int nAddrSize = sizeof(SOCKADDR_IN);

    BOOL bResult = m_fnAcceptEx(
        m_listenSocket,
        pSession->GetSocket(),
        pSession->GetAcceptBuf(),
        0,
        nAddrSize + 16,
        nAddrSize + 16,
        &dwBytesReceived,
        &pSession->GetAcceptEvent()->m_overlapped
    );

    if (bResult == FALSE)
    {
        int nErr = WSAGetLastError();
        if (nErr != WSA_IO_PENDING)
            std::cout << "[ReRegisterAccept] AcceptEx 실패: " << nErr
            << " Socket=" << pSession->GetSocket() << std::endl;
    }
    else
    {
        //std::cout << "[ReRegisterAccept] AcceptEx 성공 즉시완료" << std::endl;
    }
}

// ================================================================
//  WorkerThread
//
//  교수님 코드의 worker_thread()와 동일한 구조
//  GQCS → IOType으로 분기
// ================================================================
void CIOCP_Server::WorkerThread()
{
    while (true)
    {
        DWORD       dwNumOfBytes = 0;
        ULONG_PTR   ulKey = 0;
        OVERLAPPED* pOver = nullptr;

        BOOL bResult = GetQueuedCompletionStatus(
            m_hIOCP, &dwNumOfBytes, &ulKey, &pOver, INFINITE);

        CIOEvent* pIOEvent = reinterpret_cast<CIOEvent*>(pOver);
        if (pIOEvent == nullptr) continue;

        if (pIOEvent->m_type == IOType::MonsterAI)
        {
            int32_t nMonsterID = static_cast<int32_t>(ulKey);

            MonsterRef pMonster = CMonster_Manager::Get_Instance()
                ->Get_Monster(nMonsterID);
            if (pMonster)
            {
                CZone* pZone = CZone_Manager::Get_Instance()
                    ->GetZone(pMonster->m_nZoneID);
                if (pZone)
                    pZone->OnMonsterAI(nMonsterID);
            }

            delete pIOEvent;
            continue;  // 소켓 처리로 내려가지 않음
        }
        // 로그 추가
        //std::cout << "[GQCS] bResult=" << bResult
        //    << " bytes=" << dwNumOfBytes
        //    << " key=" << ulKey
        //    << " type=" << (int)pIOEvent->m_type << std::endl;

        CSession* rawSession = pIOEvent->m_owner;
        if (!rawSession) continue;

        SessionRef pSession =
            CSession_Manager::Get_Instance()->Get_Session(rawSession->GetID());

        if (pIOEvent->m_type == IOType::Accept)
        {
            // Accept는 bResult TRUE면 무조건 성공
            // dwNumOfBytes == 0은 정상 (데이터 없이 연결만)
            if (bResult == TRUE)
            {
                if (pSession) ProcessAccept(pSession);
            }
            else
            {
                // 진짜 에러 소켓이 유효할 때만 재등록
                if (pSession && pSession->GetSocket() != INVALID_SOCKET)
                    ReRegisterAccept(pSession);
            }
            continue;
        }

        // Recv / Send 처리
        if (bResult == FALSE || dwNumOfBytes == 0)
        {
            if (pSession) pSession->Disconnect();
            continue;
        }

        switch (pIOEvent->m_type)
        {
        case IOType::Recv:
            if (pSession) ProcessRecv(pSession, static_cast<int32_t>(dwNumOfBytes));
            break;
        case IOType::Send:
            if (pSession) ProcessSend(pSession);
            break;
        }
    }
}

// ================================================================
//  ProcessAccept
//
//  교수님 코드의 OP_ACCEPT 처리와 동일
//  차이점: SO_UPDATE_ACCEPT_CONTEXT 후처리 필수 (AcceptEx 사용 시)
// ================================================================
void CIOCP_Server::ProcessAccept(SessionRef pSession)
{
    //std::cout << "[ProcessAccept] SessionID=" << pSession->GetID()
    //    << " Socket=" << pSession->GetSocket()
    //    << " ListenSocket=" << m_listenSocket << std::endl;

    int nResult = setsockopt(pSession->GetSocket(),
        SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
        reinterpret_cast<const char*>(&m_listenSocket),
        sizeof(m_listenSocket));

    //std::cout << "[ProcessAccept] setsockopt 결과=" << nResult
    //    << " err=" << WSAGetLastError() << std::endl;
    // AcceptEx 필수 후처리 실패하면 소켓 연결이 안 된 상태

    pSession->SetConnected(true);
    pSession->RegisterRecv();

    // 새 슬롯 보충
    int32_t nNewID = CSession_Manager::Get_Instance()->Assign();
    //std::cout << "[ProcessAccept] 새 슬롯 nNewID=" << nNewID << std::endl;
    if (nNewID != -1)
    {
        SessionRef pNewSession = CSession_Manager::Get_Instance()->Get_Session(nNewID);
        SOCKET newSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
            nullptr, 0, WSA_FLAG_OVERLAPPED);
        pNewSession->SetSocket(newSocket);

        CreateIoCompletionPort(
            reinterpret_cast<HANDLE>(newSocket),
            m_hIOCP,
            static_cast<ULONG_PTR>(nNewID), 0);

        ReRegisterAccept(pNewSession);
    }
}

void CIOCP_Server::ProcessRecv(SessionRef pSession, int32_t nNumOfBytes)
{
    pSession->OnRecvComplete(nNumOfBytes);
}

void CIOCP_Server::ProcessSend(SessionRef pSession)
{
    pSession->OnSendComplete();
}

void CIOCP_Server::DebugConsoleThread()
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // 커서 깜빡임 제거
    CONSOLE_CURSOR_INFO cursorInfo = { 1, FALSE };
    SetConsoleCursorInfo(hConsole, &cursorInfo);

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        COORD pos = { 0, 0 };
        SetConsoleCursorPosition(hConsole, pos);

        auto* pSM = CSession_Manager::Get_Instance();
        auto* pPM = CPlayer_Manager::Get_Instance();

        int nConnected = 0;
        int nWaiting = (int)m_acceptSessions.size();

        std::cout << "=== MMO Server Debug =====================\n";
        std::cout << "포트: 7777  워커: " << m_workerThreads.size() << "\n";
        std::cout << "------------------------------------------\n";

        // Accept 대기 슬롯 한줄 출력
        std::cout << "[Accept 대기] ";
        for (auto& pSession : m_acceptSessions)
        {
            if (pSession && !pSession->IsConnected())
                std::cout << pSession->GetID() << " ";
        }
        std::cout << "          \n";  // 이전 내용 덮어쓰기용 공백

        // 접속 중인 플레이어
        std::cout << "------------------------------------------\n";
        std::cout << "[접속 중 플레이어]\n";

        for (int i = 0; i < MAX_SESSION; ++i)
        {
            SessionRef pSession = pSM->Get_Session(i);
            if (!pSession || !pSession->IsConnected()) continue;

            PlayerRef pPlayer = pPM->Get_Player(i);
            std::string name = pPlayer ? pPlayer->m_szName : "?";

            std::cout << "  ID=" << std::setw(3) << i
                << "  name=" << std::setw(12) << std::left << name
                << "  pos=(" << std::fixed << std::setprecision(1)
                << (pPlayer ? pPlayer->m_fCurX : 0.f) << ", "
                << (pPlayer ? pPlayer->m_fCurZ : 0.f) << ")   \n";
            nConnected++;
        }

        // 빈 줄로 이전 내용 덮어쓰기
        for (int i = nConnected; i < 5; ++i)
            std::cout << "                                          \n";

        std::cout << "------------------------------------------\n";
        std::cout << "접속: " << nConnected
            << "  대기: " << nWaiting
            << "  총 슬롯: " << (nConnected + nWaiting) << "   \n";
        std::cout << "==========================================\n";
    }
}

void CIOCP_Server::TimerThread()
{
    using namespace std::chrono;

    while (true)
    {
        // 큐가 비어있으면 잠깐 대기
        {
            std::unique_lock<std::mutex> lock(g_timerLock);
            if (g_timerQueue.empty())
            {
                lock.unlock();
                std::this_thread::sleep_for(milliseconds(1));
                continue;
            }

            // 아직 시간 안됐으면 대기
            FTimerEvent ev = g_timerQueue.top();
            if (ev.wakeupTime > system_clock::now())
            {
                lock.unlock();
                std::this_thread::sleep_for(milliseconds(1));
                continue;
            }

            g_timerQueue.pop();
            lock.unlock();

            // Worker Thread에 넘김
            // key = 몬스터 ID
            // IOType::MonsterAI로 구분
            CIOEvent* pEvent = new CIOEvent(IOType::MonsterAI);
            PostQueuedCompletionStatus(
                m_hIOCP,
                0,
                static_cast<ULONG_PTR>(ev.nID),
                &pEvent->m_overlapped);
        }
    }
}