#pragma once
#include "AccountDB.h"
#include "SaveData.h" 
#include "GameLog.h"  
#include "Protocol.h" 
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

// ================================================================
//  CDB_Manager — MySQL(ODBC/nanodbc) 접속 담당 싱글턴
//
//  - 저장 프로시저 sp_login 을 CALL 해서 로그인 인증 + 계정 데이터 로드.
//  - 호출마다 connect-쿼리-close (로그인은 드문 이벤트라 충분). 연결 풀로 승격 예정
// 
// ================================================================
class CDB_Manager
{
private:
    CDB_Manager() = default;
    ~CDB_Manager() = default;

public:
    static CDB_Manager* Get_Instance()
    {
        if (!m_pInstance)
            m_pInstance = new CDB_Manager;
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

    CDB_Manager(const CDB_Manager&) = delete;
    CDB_Manager& operator=(const CDB_Manager&) = delete;

    // 서버 시작 시 1회 호출. 연결 문자열 세팅 + 접속 테스트.
    bool Init();

    // id/pw 로 sp_login 을 호출. 인증 성공 시 out 을 채우고 true.
    bool Login(const char* id, const char* pw, FAccountData& out);


    // 플레이어 상태 스냅샷(존/위치/골드/인벤/장비)을 DB에 저장.
    bool Save(const FSaveSnapshot& snap);



    // ---------------- 저장 전용 스레드 (비동기 저장) ----------------
    //  IOCP 워커가 Save()에서 블로킹되지 않도록, 저장 요청을 큐에 넣고
    //  전용 스레드 1개가 영속 커넥션(재사용)으로 순차 처리한다.

    //서버 시작 시 1회. 스레드 기동.
    void StartSaveThread();
    
    //종료 시.큐를 비우고 조인.
    void StopSaveThread();

    //워커에서 논블로킹으로 저장 요청(스냅샷 복사본을 큐에).
    void EnqueueSave(const FSaveSnapshot& snap);

    // ---------------- 로그 전용 스레드 (거래 로그) ----------------
    //  mmorpg_log
    //  로그는 저장과 달리 건수가 훨씬 많고 순서만 맞으면 되므로 배치로 쓴다 

    //서버 시작 시 1회
    void StartLogThread();
    //종료 시 큐에 남은 로그를 전부 쓰고 조인
    void StopLogThread();
    //워커에서 논블로킹으로 로그 적재(복사본을 큐에)
    void EnqueueLog(const FGameLog& log);

    // 디버그 콘솔 표시용 누적치 (기록 성공 건수 / 큐 넘쳐서 버린 건수)
    uint64_t GetLogWritten() const { return m_logWritten.load(); }
    uint64_t GetLogDropped() const { return m_logDropped.load(); }

    // ---------------- 경매장 (DB 정본, write-through) ----------------
    // 한 페이지 조회(최신 등록순)
    int32_t Auction_GetPage(int32_t page, int32_t tab, const char* myName,
        const int32_t* searchCodes, int32_t searchCount,
        FAuctionEntry* outEntries, bool& outHasNext);

    // 등록: INSERT (listing_id는 DB가 자동 발급). 성공 시 true.
    bool Auction_Register(const char* seller, int32_t itemCode, int32_t count, int32_t unitPrice);

    // 구매 사전조회(비변경): 코드/개당가 반환. 매물 없음/본인매물이면 false.
    //  outSeller : 판매자명(선택). AUCTION_SOLD 로그의 actor 로 쓴다.
    //  DB 그대로의 UTF-8 바이트
    bool Auction_PeekBuy(int32_t listingID, const char* buyer,
        int32_t& outCode, int32_t& outUnitPrice, std::string* outSeller = nullptr);

    // 구매 확정: 원자적 조건부 UPDATE(count>=qty AND seller<>buyer). 1행 반영 시 true.
    // (동시 구매 시 DB 행 잠금으로 직렬화 - 오버셀 방지)
    bool Auction_CommitBuy(int32_t listingID, int32_t qty, int32_t total, const char* buyer);

    // 판매대금 수령: seller 본인 pending_gold 회수-0, count<=0이면 매물 삭제. 성공 시 outGold.
    bool Auction_Collect(int32_t listingID, const char* seller, int32_t& outGold);

    // 취소 사전조회(비변경): seller 본인 매물의 코드/남은수량/미수령골드. 아니면 false.
    // (취소는 DELETE 후 지급이라, 지급 실패로 아이템이 사라지지 않게 미리 인벤 확인용)
    bool Auction_PeekCancel(int32_t listingID, const char* seller,
        int32_t& outCode, int32_t& outCount, int32_t& outGold);

    // 등록취소: seller 본인 매물 삭제 + 남은수량/코드/미수령골드 반환.
    bool Auction_Cancel(int32_t listingID, const char* seller,
        int32_t& outCode, int32_t& outCount, int32_t& outGold);

private:
    // 저장 전용 스레드 루프. 큐에서 스냅샷을 꺼내 영속 커넥션으로 저장.
    void SaveThreadFunc();

    // 로그 전용 스레드 루프. 큐에서 로그를 모아 배치 INSERT.
    void LogThreadFunc();

    static CDB_Manager* m_pInstance;

    std::string m_connStr;      // ODBC 연결 문자열 (Database=mmorpg)
    std::string m_logConnStr;   // 로그 DB 연결 문자열 (Database=mmorpg_log)

    // ---- 저장 전용 스레드/큐 ----
    std::thread               m_saveThread;
    std::mutex                m_queueLock;
    std::condition_variable   m_queueCv;
    std::queue<FSaveSnapshot> m_saveQueue;      // 대기 중인 저장 요청(스냅샷 복사본)
    std::atomic<bool>         m_saveRunning{ false };

    // ---- 로그 전용 스레드/큐 ----
    std::thread               m_logThread;
    std::mutex                m_logQueueLock;
    std::condition_variable   m_logQueueCv;
    std::queue<FGameLog>      m_logQueue;
    std::atomic<bool>         m_logRunning{ false };
    std::atomic<uint64_t>     m_logWritten{ 0 };
    std::atomic<uint64_t>     m_logDropped{ 0 };
};
