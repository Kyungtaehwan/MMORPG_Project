#pragma once
#include "Protocol.h"

// ================================================================
//  패킷 큐 아이템
//  수신 스레드가 패킷을 람다로 감싸서 큐에 push
//  메인 스레드가 매 프레임 꺼내서 처리
//
//  이렇게 하는 이유:
//  D2D 렌더는 싱글스레드 모델 - 메인 스레드에서만 가능
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
//  1) Connect() - TCP 연결 + 수신 스레드 시작
//  2) 수신 스레드 - 패킷 수신 - 큐에 push
//  3) Dispatch() - 메인 스레드에서 매 프레임 호출
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
    void SendPortal(int32_t nTargetZone, float fSpawnX, float fSpawnZ);
    void SendPickup(uint32_t nDropId);
    void SendEquip(int32_t nInvenSlot);
    void SendUnEquip(int32_t nEquipSlot);
    void SendUseItem(int32_t nInvenSlot);
    void SendMoveStop(float fCurX, float fCurZ);     // UI 진입 등 이동 정지 통보
    void SendBuy(int32_t nItemCode, int32_t nCount); // 상점 구매
    void SendSell(int32_t nInvenSlot, int32_t nCount); // 상점 판매

    // 경매장
    void SendAuctionList(int32_t page, int32_t tab,
        const int32_t* searchCodes, int32_t searchCount);               // 목록 요청(페이지/탭/검색)
    void SendAuctionRegister(int32_t nInvenSlot, int32_t nCount, int32_t nUnitPrice);
    void SendAuctionBuy(int32_t nListingID, int32_t nCount);
    void SendAuctionCollect(int32_t nListingID);
    void SendAuctionCancel(int32_t nListingID);
    void SendQuickSlotSet(int32_t nSlot, int32_t nItemCode);   // 퀵슬롯 등록/해제(0=해제)
    // ---- 메인 스레드에서 매 프레임 호출 ----
    // 큐에 쌓인 패킷 핸들러를 전부 처리
    void Dispatch();

    // ---- 게임 데이터 ----
    uint32_t GetMyPlayerID() const { return m_nMyPlayerID; }

    // ---- 로그인 실패 표시 (로그인 박스가 폴링) ----
    bool Is_LoginFailed() const { return m_bLoginFailed; }
    void Clear_LoginFailed() { m_bLoginFailed = false; }

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
    void Handle_SC_CHANGE_ZONE(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_ADD_DROP(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_REMOVE_DROP(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_INVEN_UPDATE(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_PLAYER_HP(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_BUFF(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_PLAYER_EXP(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_QUICKSLOT_UPDATE(uint8_t* pBuffer, int32_t nSize);
    void Handle_SC_AUCTION_LIST(uint8_t* pBuffer, int32_t nSize);
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
    int32_t GetStartZone() const { return m_nStartZone; }   // 로그인 시작 존(서버 지정)
    const char* GetMyName() const { return m_szMyName; }    // 내 캐릭터 이름(머리 위 표시)

    // 경매장 스냅샷 접근 (경매장 UI가 매 프레임 참조)
    const FAuctionEntry* GetAuctionEntries() const { return m_auctionEntries; }
    int32_t              GetAuctionCount()   const { return m_auctionCount; }
    int32_t              GetAuctionPage()    const { return m_auctionPage; }
    bool                 GetAuctionHasNext() const { return m_auctionHasNext != 0; }
    uint32_t             GetAuctionVersion() const { return m_auctionVersion; } // 갱신 감지용

    // 퀵슬롯 스냅샷(로그인 시 서버가 보내줌). UI가 버전 변화를 보고 한 번 가져간다.
    const int32_t*       GetQuickCodes()   const { return m_quickCode; }
    uint32_t             GetQuickVersion() const { return m_quickVersion; }
    void  ClearSpawn()     { m_bSpawnReady = false; }

private:
    uint32_t             m_nMyPlayerID = 0;
    bool  m_bLoginFailed = false;
    bool  m_bSpawnReady = false;
    float m_fSpawnX = 0.f;
    float m_fSpawnZ = 0.f;
    int32_t m_nStartZone = 1;   // 기본 ZONE_TOWN(=1). SC_ENTER_GAME에서 서버 값으로 갱신
    char  m_szMyName[20] = {};  // SC_ENTER_GAME에서 서버가 준 내 이름

    // 경매장 스냅샷(SC_AUCTION_LIST 수신 시 갱신)
    FAuctionEntry m_auctionEntries[AUCTION_MAX] = {};
    int32_t       m_auctionCount = 0;
    int32_t       m_auctionPage = 0;      // 서버가 준 현재 페이지
    int32_t       m_auctionHasNext = 0;   // 다음 페이지 존재 여부
    uint32_t      m_auctionVersion = 0;   // 수신할 때마다 +1

    int32_t       m_quickCode[QUICK_SLOTS] = {};
    uint32_t      m_quickVersion = 0;     // 수신할 때마다 +1 (0 = 아직 못 받음)
};