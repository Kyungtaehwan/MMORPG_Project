#pragma once
#include "Protocol.h"

// ================================================================
//  패킷 큐 아이템
//  수신 스레드가 패킷을 람다로 감싸서 큐에 push
//  메인 스레드가 매 프레임 꺼내서 처리
//
//  이렇게 하는 이유:
//  D2D 렌더는 싱글스레드 모델 → 메인 스레드에서만 가능
//  수신 스레드에서 직접 UI 건드리면 크래시
//  큐를 거치면 항상 메인 스레드에서 처리 보장
// ================================================================
struct FPacketTask
{
    std::function<void()> m_handler;
};

// ================================================================
//  CNetwork_Manager  클라이언트 네트워크 싱글톤
//
//  구조:
//  1) Connect() → TCP 연결 + 수신 스레드 시작
//  2) 수신 스레드 → 패킷 수신 → 큐에 push
//  3) Dispatch() → 메인 스레드에서 매 프레임 호출
//                  큐에서 꺼내서 처리
// ================================================================
class CNetwork_Manager
{
private:
    CNetwork_Manager() = default;
    ~CNetwork_Manager() { Disconnect(); }

public:
    static CNetwork_Manager* Get_Instance()
    {
        if (!m_pInstance)
            m_pInstance = new CNetwork_Manager;
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

    CNetwork_Manager(const CNetwork_Manager&) = delete;
    CNetwork_Manager& operator=(const CNetwork_Manager&) = delete;

    // ---- 연결 ----
    bool Connect(const wchar_t* pszIP, uint16_t nPort);
    void Disconnect();
    bool IsConnected() const { return m_bConnected; }

    // ---- 송신 ----
    void SendLogin(const char* pszID, const char* pszPW);
    void SendMoveDest(float fDestX, float fDestZ, uint32_t nMoveTime);
    void SendMovePos(float fCurX, float fCurZ, uint32_t nMoveTime);
    void SendAttackMonster(int32_t nMonsterID, float fCurX, float fCurZ);
    void SendRespawn();
    // ---- 메인 스레드에서 매 프레임 호출 ----
    // 큐에 쌓인 패킷 핸들러를 전부 처리
    void Dispatch();

    // ---- 게임 데이터 ----
    uint32_t GetMyPlayerID() const { return m_nMyPlayerID; }

private:
    // ---- 수신 스레드 ----
    void RecvThread();

    // ---- 패킷 파싱 + 큐 push ----
    void ProcessPacket(uint8_t* pBuffer, int32_t nSize);

    // ---- 패킷별 핸들러 ----
    void Handle_SC_LOGIN_OK(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_LOGIN_FAIL(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_ENTER_GAME(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_ADD_PLAYER(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_REMOVE_PLAYER(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_MOVE_PLAYER(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_PLAYER_STATE(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_PLAYER_HIT(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_RESPAWN(uint8_t* pBuffer, int32_t nSize);
public:
    

private:
    // --몬스터--
    void Handle_SC_ADD_MONSTER(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_MONSTER_HIT(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_REMOVE_MONSTER(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_MOVE_MONSTER(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_MONSTER_STATE(uint8_t* pBuffer, int32_t nSize);

    // ---- 송신 헬퍼 ----
    bool SendRaw(const void* pData, int32_t nSize);

    // ---- 큐 push (수신 스레드에서 호출) ----
    void PushTask(std::function<void()> handler)
    {
        std::lock_guard<std::mutex> lock(m_queueLock);
        m_taskQueue.push({ handler });
    }

private:
    static CNetwork_Manager* m_pInstance;

    SOCKET               m_socket = INVALID_SOCKET;
    std::atomic<bool>    m_bConnected = false;

    // 수신 스레드
    std::thread          m_recvThread;

    // 패킷 큐  수신 스레드가 push, 메인 스레드가 pop
    std::queue<FPacketTask> m_taskQueue;
    std::mutex              m_queueLock;

    // recv 버퍼 불완전 패킷 처리
    static constexpr int32_t RECV_BUF_SIZE = 4096;
    uint8_t  m_recvBuf[RECV_BUF_SIZE] = {};
    int32_t  m_nPrevRemain = 0;

public:
    bool  IsSpawnReady() const { return m_bSpawnReady; }
    float GetSpawnX()    const { return m_fSpawnX; }
    float GetSpawnZ()    const { return m_fSpawnZ; }
    void  ClearSpawn()     { m_bSpawnReady = false; }

private:
    uint32_t             m_nMyPlayerID = 0;
    bool  m_bSpawnReady = false;
    float m_fSpawnX = 0.f;
    float m_fSpawnZ = 0.f;
};