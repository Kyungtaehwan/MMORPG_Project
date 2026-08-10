#include "pch.h"
#include "IOCP_Server.h"
#include "Session_Manager.h"
#include "Player_Manager.h"
#include "Timer.h"
#include "Monster_Manager.h" 
#include "Zone_Manager.h"
#include "Protocol.h"
#include "StressMetrics.h"
#include "AllocCounter.h"
#include <iostream>
#include <sstream>
#include <deque>
#include <string>
#include <iomanip>
#include <cstring>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")   // GetProcessMemoryInfo

// 주기 저장 틱 간격(ms). 한 틱에 온라인 1명씩 라운드로빈 저장.
static constexpr uint32_t AUTOSAVE_INTERVAL_MS = 5000;

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
    // 확장 함수인 AcceptEx는 MS 권장대로 함수 주소를 받아 씀
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

    // 주기 저장 타이머 최초 등록 (이후 워커가 매 틱 재등록)
    AddTimer(0, EEventType::PlayerAutoSave, AUTOSAVE_INTERVAL_MS);
    for (int32_t i = 0; i < nThreadCount; ++i)
        m_workerThreads.emplace_back(&CIOCP_Server::WorkerThread, this);

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

    // 개발 단계에서 서버 재시작시 OS가 포트를 붙잡아 bind 거부되는 상황을 막기위해 붙잡는 상태에서도 Bind 허용 옵션 설정
    BOOL bReuseAddr = TRUE;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&bReuseAddr), sizeof(bReuseAddr));

    //  리슨 소켓을 IOCP에 등록
    //  key=0으로 등록 (Accept 완료는 key가 아닌 IOEvent로 구분)
    CreateIoCompletionPort(
        reinterpret_cast<HANDLE>(m_listenSocket), m_hIOCP, 0, 0);

    SOCKADDR_IN addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(nPort);
    addr.sin_addr.s_addr = INADDR_ANY;

    //바인딩
    if (bind(m_listenSocket,
        reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        std::cout << "[CIOCPServer] bind 실패: " << WSAGetLastError() << std::endl;
        return false;
    }
    
    //  접속 받는 소켓으로 전환
    //  SOMAXCONN은 OS가 허용하는 합리적 최대치
    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cout << "[CIOCPServer] listen 실패: " << WSAGetLastError() << std::endl;
        return false;
    }

    return true;
}


//  세션 슬롯 ACCEPT_POOL_SIZE개 미리 준비
//  Accept 완료 후 재등록 시 새 세션 슬롯을 Assign해서 사용
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

        ReRegisterAccept(pSession);
    }
}



//  IOCP 등록 - AcceptEx 순서
//  pSession->GetAcceptBuf() = m_acceptEvent.m_acceptBuf(CIOEvent 안에 포함된 버퍼)
    
void CIOCP_Server::ReRegisterAccept(SessionRef pSession)
{

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

}

//  워커 쓰레드
//  GQCS -> IOType으로 분기
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
        else if (pIOEvent->m_type == IOType::MonsterRespawn)
        {
            int32_t nMonsterID = static_cast<int32_t>(ulKey);

            MonsterRef pMonster = CMonster_Manager::Get_Instance()
                ->Get_Monster(nMonsterID);
            if (pMonster)
            {
                CZone* pZone = CZone_Manager::Get_Instance()
                    ->GetZone(pMonster->m_nZoneID);
                if (pZone)
                    pZone->OnMonsterRespawn(nMonsterID);
            }

            delete pIOEvent;
            continue;
        }
        else if (pIOEvent->m_type == IOType::MonsterAttackHit)
        {
            int32_t nMonsterID = static_cast<int32_t>(ulKey);

            MonsterRef pMonster = CMonster_Manager::Get_Instance()
                ->Get_Monster(nMonsterID);
            if (pMonster)
            {
                CZone* pZone = CZone_Manager::Get_Instance()
                    ->GetZone(pMonster->m_nZoneID);
                if (pZone)
                    pZone->OnMonsterAttackHit(nMonsterID);
            }

            delete pIOEvent;
            continue;
        }
        else if (pIOEvent->m_type == IOType::PlayerAutoSave)
        {
            // 주기 저장: 온라인 1명 저장(라운드로빈) 후 다음 틱 재등록
            CPlayer_Manager::Get_Instance()->AutoSaveNext();
            AddTimer(0, EEventType::PlayerAutoSave, AUTOSAVE_INTERVAL_MS);

            delete pIOEvent;
            continue;
        }

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
//  차이점: SO_UPDATE_ACCEPT_CONTEXT 후처리 필수 (AcceptEx 사용 시)
// ================================================================
void CIOCP_Server::ProcessAccept(SessionRef pSession)
{

    int nResult = setsockopt(pSession->GetSocket(),
        SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
        reinterpret_cast<const char*>(&m_listenSocket),
        sizeof(m_listenSocket));

    // Nagle 알고리즘 끄기. 게임 패킷은 전부 수십 바이트짜리 작은 패킷이라
    // Nagle 이 켜져 있으면 밀릴 수 있다.

    BOOL bNoDelay = TRUE;
    setsockopt(pSession->GetSocket(), IPPROTO_TCP, TCP_NODELAY,
        reinterpret_cast<const char*>(&bNoDelay), sizeof(bNoDelay));

    pSession->SetConnected(true);
    CSession_Manager::Get_Instance()->OnConnected();
    pSession->RegisterRecv();

    // 새 슬롯 보충
    int32_t nNewID = CSession_Manager::Get_Instance()->Assign();
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

// 콘솔 전체를 공백으로 덮고 커서를 좌상단으로 되돌린다.
// 기동 로그(존 생성, 몬스터 스폰 등)를 한 번에 치우는 용도.
static void ClearConsoleAll(HANDLE hConsole)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;

    const DWORD cells = static_cast<DWORD>(csbi.dwSize.X) * csbi.dwSize.Y;
    const COORD home = { 0, 0 };
    DWORD written = 0;

    FillConsoleOutputCharacterA(hConsole, ' ', cells, home, &written);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, cells, home, &written);
    SetConsoleCursorPosition(hConsole, home);
}

static int GetConsoleWidth(HANDLE hConsole)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return 80;

    const int w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    return (w < 50) ? 50 : w;
}

// ================================================================
//  프로세스 CPU / 메모리 샘플링
//
//  GetProcessTimes 는 이 프로세스가 커널+유저 모드에서 쓴 누적 CPU 시간을 준다.
//  직전 샘플과의 차이를 구간 시간으로 나누면 "코어 몇 개어치를 쓰고 있나"가 나온다.
//   - 코어 1개를 100% 쓰면 1.0 코어. 16코어를 다 쓰면 16.0 코어
//
//  이 값이 있어야 "같은 부하에서 CPU를 얼마나 덜 쓰게 됐나"를 비교할 수 있다.
//  특히 패킷 1건당 CPU(us)는 봇과 서버가 코어를 나눠 쓰는 상황에서도
//  비율이라 왜곡이 적어, 최적화 전후 비교에 가장 믿을 만한 지표다.
// ================================================================
struct FProcStat
{
    double cpuCores     = 0.0;   // 코어 환산 사용량 (16코어 중 7.2개 쓰면 7.2)
    double cpuPercent   = 0.0;   // 전체 코어 대비 % (0~100)
    double cpuUsDelta   = 0.0;   // 직전 창에서 쓴 CPU 시간(us)
    double cpuUsPerPkt  = 0.0;   // 패킷 1건당 CPU(us) - 최적화 전후 비교의 핵심
    double workingSetMB = 0.0;   // 실제 물리 메모리 점유
};

// CPU 집계 창(초). 화면은 0.5초마다 갱신하지만 CPU 만 이 주기로 다시 잰다.
//   GetProcessTimes 의 해상도가 스케줄러 틱(약 15.6ms)이라, 0.5초 구간에서
//   가벼운 부하는 통째로 0으로 뭉개진다(측정값이 0.00 과 0.70 을 오간다).
//   창을 늘리면 틱 단위 양자화 오차가 그만큼 희석된다.
static constexpr double CPU_WINDOW_SEC = 4.0;

static FProcStat SampleProcStat(double /*dtSec*/, uint64_t nPacketsInWindow)
{
    // 창이 찰 때까지는 직전 계산값을 그대로 보여준다(화면이 0으로 깜빡이지 않게).
    static FProcStat s_last;
    static uint64_t  s_prevCpu100ns = 0;
    static int64_t   s_prevTick     = 0;
    static uint64_t  s_pktAccum     = 0;
    static bool      s_bFirst       = true;

    s_pktAccum += nPacketsInWindow;

    // 메모리는 순간값이라 매번 갱신해도 된다.
    PROCESS_MEMORY_COUNTERS pmc = {};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        s_last.workingSetMB = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);

    const int64_t nNow = StressMetrics::Now();
    const double  elapsed = s_bFirst ? 0.0
        : static_cast<double>(nNow - s_prevTick) / static_cast<double>(StressMetrics::QpcFreq());

    FILETIME ftCreate, ftExit, ftKernel, ftUser;
    if (!GetProcessTimes(GetCurrentProcess(), &ftCreate, &ftExit, &ftKernel, &ftUser))
        return s_last;

    // FILETIME 은 100ns 단위. ULARGE_INTEGER 로 합쳐야 64비트로 다룰 수 있다.
    ULARGE_INTEGER k, u;
    k.LowPart = ftKernel.dwLowDateTime;  k.HighPart = ftKernel.dwHighDateTime;
    u.LowPart = ftUser.dwLowDateTime;    u.HighPart = ftUser.dwHighDateTime;
    const uint64_t nowCpu = k.QuadPart + u.QuadPart;

    if (s_bFirst)
    {
        s_prevCpu100ns = nowCpu;
        s_prevTick     = nNow;
        s_pktAccum     = 0;
        s_bFirst       = false;
        return s_last;
    }

    if (elapsed < CPU_WINDOW_SEC)
        return s_last;      // 아직 창이 안 찼다

    s_last.cpuUsDelta = static_cast<double>(nowCpu - s_prevCpu100ns) / 10.0;  // 100ns -> us
    s_last.cpuCores   = s_last.cpuUsDelta / (elapsed * 1000000.0);

    unsigned nCores = std::thread::hardware_concurrency();
    if (nCores == 0) nCores = 1;
    s_last.cpuPercent  = s_last.cpuCores / nCores * 100.0;

    // 패킷 1건당 CPU. 같은 창에서 센 패킷 수로 나눠야 짝이 맞는다.
    s_last.cpuUsPerPkt = s_pktAccum ? s_last.cpuUsDelta / s_pktAccum : 0.0;

    s_prevCpu100ns = nowCpu;
    s_prevTick     = nNow;
    s_pktAccum     = 0;
    return s_last;
}

void CIOCP_Server::DebugConsoleThread()
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // 커서 깜빡임 제거
    CONSOLE_CURSOR_INFO cursorInfo = { 1, FALSE };
    SetConsoleCursorInfo(hConsole, &cursorInfo);

    // 준비가 끝났으니 기동 로그를 싹 지우고 대시보드로 전환한다.
    ClearConsoleAll(hConsole);

    auto* pSM = CSession_Manager::Get_Instance();
    auto* pPM = CPlayer_Manager::Get_Instance();

    // 목록에 보여줄 최대 인원. 이 수를 넘으면 숫자만 표시한다.
    constexpr int LIST_MAX = 8;

    // ================================================================
    //  측정창 요약 (성능 표에 적는 숫자는 여기서 나온 것만 쓴다)
    //
    //  화면의 0.5초 지표는 "지금 상태"를 보는 용도지 기록용이 아니다.
    //  0.5초 창에는 패킷이 수백 개뿐이라 p99 가 상위 6~9개 표본으로 계산돼
    //  같은 부하에서도 프레임마다 2배씩 흔들린다(900봇 실측 5.63~11.26ms).
    //  그래서 WINDOW_SEC 동안 칸 개수를 그대로 누적한 뒤 창 끝에서 한 번만
    //  백분위를 계산한다. 백분위수는 평균낼 수 없으므로 이 방법뿐이다.
    //  누적은 콘솔 스레드가 하므로 패킷 처리 경로에는 비용이 전혀 늘지 않는다.
    // ================================================================
    constexpr double WINDOW_SEC = 60.0;

    uint64_t winBuckets[StressMetrics::BUCKET_COUNT] = {};
    uint64_t winCount = 0, winSumUs = 0;
    uint32_t winMaxUs = 0;
    uint64_t winAoiCalls = 0, winAoiSumUs = 0, winAoiScanned = 0, winAoiLockUs = 0;
    double   winElapsed = 0.0;
    int      winIndex = 0;

    // 마지막 요약 3개를 화면에 남겨둔다(측정 끝나고 그대로 받아적게).
    std::deque<std::string> summaries;

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // ---- 부하 계측 스냅샷(직전 구간) ----
        static int64_t s_prevTick = StressMetrics::Now();
        int64_t nNowTick = StressMetrics::Now();
        double  dtSec = static_cast<double>(nNowTick - s_prevTick)
            / static_cast<double>(StressMetrics::QpcFreq());
        s_prevTick = nNowTick;
        if (dtSec < 0.001) dtSec = 0.001;

        StressMetrics::Snapshot ms = StressMetrics::SnapshotAndReset();
        double pps = static_cast<double>(ms.count) / dtSec;

        // ---- AOI(GetNearPlayers) 계측 : 섹터 AOI 개선 전/후 비교 핵심 지표 ----
        StressMetrics::AoiSnapshot aoi = StressMetrics::SnapshotAoiAndReset();
        double aoiCps   = aoi.calls / dtSec;                                   // 초당 호출
        double aoiAvgUs = aoi.calls ? (double)aoi.sumUs / aoi.calls : 0.0;     // 1회당 us
        double aoiScan  = aoi.calls ? (double)aoi.sumScanned / aoi.calls : 0.0;// 1회당 순회 N
        double aoiMsPs  = (aoi.sumUs / 1000.0) / dtSec;                        // 초당 총 소요 ms

        // 접속 수는 카운터라 O(1)이다. 배열을 훑지 않는다.
        const int nConnected = pSM->GetConnectedCount();

        // ---- 측정창 누적 ----
        for (int i = 0; i < StressMetrics::BUCKET_COUNT; ++i)
            winBuckets[i] += ms.buckets[i];
        winCount  += ms.count;
        winSumUs  += ms.sumUs;
        if (ms.maxUs > winMaxUs) winMaxUs = ms.maxUs;
        winAoiCalls   += aoi.calls;
        winAoiSumUs   += aoi.sumUs;
        winAoiScanned += aoi.sumScanned;
        winAoiLockUs  += aoi.lockUs;
        winElapsed    += dtSec;

        if (winElapsed >= WINDOW_SEC)
        {
            // 두 줄로 나눈다. Line() 의 자르기는 바이트 기준이라 한글이 섞인
            // 긴 줄은 화면에서 잘려 max·AOI 가 안 보인다(실제로 겪음).
            ++winIndex;
            std::ostringstream head, body;
            head << std::fixed << std::setprecision(0)
                 << " [요약 " << winIndex << "] " << winElapsed << "s"
                 << "  접속 " << nConnected << "  표본 " << winCount;

            if (winCount == 0)
            {
                body << "   (처리한 패킷 없음)";
            }
            else
            {
                const uint32_t p50  = StressMetrics::PercentileOf(winBuckets, winCount, 0.50);
                const uint32_t p99  = StressMetrics::PercentileOf(winBuckets, winCount, 0.99);
                const uint32_t p999 = StressMetrics::PercentileOf(winBuckets, winCount, 0.999);
                const double aoiScanW = winAoiCalls
                    ? static_cast<double>(winAoiScanned) / winAoiCalls : 0.0;
                const double lockCoreW = (static_cast<double>(winAoiLockUs) / 1000000.0)
                    / winElapsed * 100.0;
                const double lockPctW  = winAoiSumUs
                    ? 100.0 * winAoiLockUs / winAoiSumUs : 0.0;
                const double aoiCoreW = (static_cast<double>(winAoiSumUs) / 1000000.0)
                    / winElapsed * 100.0;

                body << std::fixed << std::setprecision(2)
                     << "   p50 " << (p50 / 1000.0)
                     << "  p99 " << (p99 / 1000.0)
                     << "  p99.9 " << (p999 / 1000.0)
                     << "  max " << (winMaxUs / 1000.0) << " ms"
                     << std::setprecision(1)
                     << " | AOI N " << aoiScanW << "  " << aoiCoreW << "% core"
                     << "  (lock " << lockCoreW << "% = " << lockPctW << "%)";
            }
            summaries.push_back(head.str());
            summaries.push_back(body.str());
            while (summaries.size() > 6) summaries.pop_front();   // 요약 3개(=6줄)

            for (int i = 0; i < StressMetrics::BUCKET_COUNT; ++i) winBuckets[i] = 0;
            winCount = winSumUs = 0; winMaxUs = 0;
            winAoiCalls = winAoiSumUs = winAoiScanned = winAoiLockUs = 0;
            winElapsed = 0.0;
        }

        // ---- CPU / 메모리 : 최적화 전후 "비용" 비교의 핵심 지표 ----
        const FProcStat proc = SampleProcStat(dtSec, ms.count);

        // ---- 프레임을 문자열로 다 만든 뒤 한 번에 출력한다 ----
        //  줄마다 콘솔 폭까지 공백을 채우므로 이전 화면의 잔상이 남지 않는다.
        //  (예전처럼 줄 끝에 공백을 손으로 붙일 필요가 없다)
        const int width = GetConsoleWidth(hConsole);
        const int barLen = (width - 1 < 66) ? width - 1 : 66;
        const std::string barDouble(barLen, '=');
        const std::string barSingle(barLen, '-');

        std::ostringstream frame;
        auto Line = [&](const std::string& s)
        {
            std::string t = s;
            if (static_cast<int>(t.size()) > width - 1)
                t.resize(width - 1);
            t.append((width - 1) - t.size(), ' ');
            frame << t << '\n';
        };
        auto Num = [](double v, int prec)
        {
            std::ostringstream o;
            o << std::fixed << std::setprecision(prec) << v;
            return o.str();
        };

        Line(barDouble);
        Line(" MMO GameServer  [" + GetServerConfigTag() + "]"
             + "   포트 7777   워커 " + std::to_string(m_workerThreads.size()));
        //  1) 서버 처리 p50/p99  - 서버가 느린가 (원인 진단)
        //  2) CPU 코어           - 서버가 포화인가 (봇 탓/서버 탓 판별)
        //  3) AOI                - 어느 함수가 범인인가 (섹터 분할 근거)
        //  4) 메모리             - 누수 감지 (보험)
        Line(barDouble);
        Line(" 접속 " + std::to_string(nConnected) + " 명");
        Line(barSingle);
        Line(" 패킷  " + Num(pps, 0) + " pkt/s"
             + "   (구간 " + std::to_string(ms.count) + " 건 / " + Num(dtSec, 2) + "s)");
        Line(" 지연  p50 " + Num(ms.p50Us / 1000.0, 2) + "ms"
             + "   p99 " + Num(ms.p99Us / 1000.0, 2) + "ms"
             + "   max " + Num(ms.maxUs / 1000.0, 2) + "ms");
        Line(barSingle);
        Line(" CPU   " + Num(proc.cpuCores, 2) + "코어/"
             + std::to_string(std::thread::hardware_concurrency())
             + "   (" + Num(proc.cpuPercent, 1) + "%, "
             + Num(CPU_WINDOW_SEC, 0) + "초 평균)");
        Line(" 메모리 " + Num(proc.workingSetMB, 1) + " MB");
        Line(barSingle);
        Line(" AOI   " + Num(aoiCps, 0) + " 회/s"
             + "   평균N " + Num(aoiScan, 1)
             + "   1회 " + Num(aoiAvgUs, 1) + "us"
             + "   " + Num(aoiMsPs / 10.0, 1) + "%코어");
        // AOI 시간 중 몇 %가 락을 "기다린" 시간인가.
        //  이 값이 높으면 병목은 스캔량이 아니라 전역 락 직렬화다.
        Line("       락대기 " + Num((aoi.lockUs / 1000.0) / 10.0, 1) + "%코어"
             + "  (AOI 시간의 " + Num(aoi.sumUs ? (100.0 * aoi.lockUs / aoi.sumUs) : 0.0, 1) + "%)"
             + "   1회 " + Num(aoi.calls ? (double)aoi.lockUs / aoi.calls : 0.0, 1) + "us");
        Line(barSingle);


        {
            const int leftSec = static_cast<int>(WINDOW_SEC - winElapsed + 0.5);
            Line(" 측정창 " + Num(WINDOW_SEC, 0) + "초  (다음 요약까지 "
                 + std::to_string(leftSec < 0 ? 0 : leftSec) + "초)"
                 );
            if (summaries.empty())
                Line("   (아직 없음)");
            else
                for (const std::string& s : summaries) Line(s);
            Line(barSingle);
        }

#if USE_ALLOC_COUNTER
        // 힙 할당 조사. "1패킷당" 이 핵심 숫자다 - 부하가 달라져도 비교가 된다.
        {
            AllocMetrics::Snapshot al = AllocMetrics::SnapshotAndReset();
            const double allocPs = static_cast<double>(al.allocs) / dtSec;
            const double freePs  = static_cast<double>(al.frees)  / dtSec;
            const double mbPs    = (static_cast<double>(al.bytes) / (1024.0 * 1024.0)) / dtSec;
            const double perPkt  = ms.count
                ? static_cast<double>(al.allocs) / static_cast<double>(ms.count) : 0.0;

            Line(" 할당  " + Num(allocPs, 0) + " 회/s"
                 + "   해제 " + Num(freePs, 0) + " 회/s"
                 + "   " + Num(mbPs, 1) + " MB/s"
                 + "   1패킷당 " + Num(perPkt, 1) + " 회");
            Line(barSingle);
        }
#endif

        // ---- 접속 플레이어 목록: 소수일 때만(부하 중엔 생략) ----
        int nListed = 0;
        if (nConnected == 0)
        {
            Line(" 접속 중인 플레이어 없음");
        }
        else if (nConnected <= LIST_MAX)
        {
            Line(" 접속 중 플레이어");
            // 접속 수를 이미 알고 있으므로 다 찾으면 바로 멈춘다.
            for (int i = 0; i < MAX_SESSION && nListed < nConnected; ++i)
            {
                SessionRef pSession = pSM->Get_Session(i);
                if (!pSession || !pSession->IsConnected()) continue;

                PlayerRef pPlayer = pPM->Get_Player(i);
                std::ostringstream row;
                row << "   ID " << std::setw(4) << i << "  "
                    << std::setw(14) << std::left
                    << (pPlayer ? pPlayer->m_szName : "?")
                    << "  Zone " << (pPlayer ? pPlayer->m_nZoneID : -1)
                    << "  (" << std::fixed << std::setprecision(1)
                    << (pPlayer ? pPlayer->m_fCurX : 0.f) << ", "
                    << (pPlayer ? pPlayer->m_fCurZ : 0.f) << ")";
                Line(row.str());
                nListed++;
            }
        }
        else
        {
            Line(" 접속 중 플레이어 " + std::to_string(nConnected) + " 명 (목록 생략)");
        }

        // 줄 수를 항상 같게 맞춰야 아래쪽에 이전 프레임이 남지 않는다.
        for (int i = nListed; i < LIST_MAX; ++i)
            Line("");

        Line(barDouble);

        // 커서를 좌상단으로 되돌리고 한 번에 찍는다(깜빡임 최소화).
        SetConsoleCursorPosition(hConsole, COORD{ 0, 0 });
        std::cout << frame.str() << std::flush;
    }
}

void CIOCP_Server::TimerThread()
{
    using namespace std::chrono;

    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(g_timerLock);
            if (g_timerQueue.empty())
            {
                lock.unlock();
                std::this_thread::sleep_for(milliseconds(1));
                continue;
            }

            FTimerEvent ev = g_timerQueue.top();
            if (ev.wakeupTime > system_clock::now())
            {
                lock.unlock();
                std::this_thread::sleep_for(milliseconds(1));
                continue;
            }

            g_timerQueue.pop();
            lock.unlock();


            IOType eIOType;
            switch (ev.eType)
            {
            case EEventType::MonsterAI:
                eIOType = IOType::MonsterAI;
                break;
            case EEventType::MonsterRespawn:
                eIOType = IOType::MonsterRespawn;
                break;
            case EEventType::MonsterAttackHit:
                eIOType = IOType::MonsterAttackHit;
                break;
            case EEventType::PlayerAutoSave:
                eIOType = IOType::PlayerAutoSave;
                break;
            default:
                break;

            }

            CIOEvent* pEvent = new CIOEvent(eIOType);
            PostQueuedCompletionStatus(
                m_hIOCP, 0,
                static_cast<ULONG_PTR>(ev.nID),
                &pEvent->m_overlapped);
        }
    }
}