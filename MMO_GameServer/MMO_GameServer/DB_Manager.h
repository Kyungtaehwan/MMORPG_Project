#pragma once
#include "AccountDB.h"   // FAccountData, FSaveItem
#include "SaveData.h"    // FSaveSnapshot
#include "Protocol.h"    // FAuctionEntry
#include <string>

// ================================================================
//  CDB_Manager — MySQL(ODBC/nanodbc) 접속 담당 싱글턴
//
//  - 저장 프로시저 sp_login 을 CALL 해서 로그인 인증 + 계정 데이터 로드.
//  - 연결 모델 A: 호출마다 connect-쿼리-close (로그인은 드문 이벤트라 충분).
//    나중에 세이브가 잦아지면 연결 풀로 승격 예정.
//  - 게임 로직(Packet_Handler)은 예전 FindAccount() 대신 Login() 을 부르되,
//    결과를 그대로 FAccountData 로 받으므로 이후 처리는 바뀌지 않는다.
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
    // 실패하면 false (드라이버 미설치/계정 오류/서버 미기동 등).
    bool Init();

    // id/pw 로 sp_login 을 호출. 인증 성공 시 out 을 채우고 true.
    // 실패(인증 불일치/DB 오류) 시 false.
    bool Login(const char* id, const char* pw, FAccountData& out);

    // 플레이어 상태 스냅샷(존/위치/골드/인벤/장비)을 DB에 저장.
    // character UPDATE + inventory/equipment DELETE-INSERT 를 한 트랜잭션으로 실행:
    // 전부 성공하면 commit, 중간 실패 시 롤백(부분 저장으로 DB가 깨지지 않음).
    // 스냅샷은 CPlayer::TakeSnapshot()으로 락 잡고 미리 복사해온 것.
    // 계정 식별은 snap.id(=account_id). 성공 true / 실패 false.
    bool Save(const FSaveSnapshot& snap);

    // ---------------- 경매장 (DB 정본, write-through) ----------------
    // 한 페이지 조회(최신 등록순). outEntries[AUCTION_PAGE_SIZE]에 채우고 개수 반환.
    //  tab=0 구매(seller<>me AND count>0), tab=1 내판매(seller=me, 완판 포함).
    //  searchCount>0 이면 item_code IN(searchCodes) 로 추가 필터(구매 탭 검색).
    //  outHasNext = 다음 페이지 존재 여부. page는 0-base.
    int32_t Auction_GetPage(int32_t page, int32_t tab, const char* myName,
        const int32_t* searchCodes, int32_t searchCount,
        FAuctionEntry* outEntries, bool& outHasNext);

    // 등록: INSERT (listing_id는 DB가 자동 발급). 성공 시 true.
    bool Auction_Register(const char* seller, int32_t itemCode, int32_t count, int32_t unitPrice);

    // 구매 사전조회(비변경): 코드/개당가 반환. 매물 없음/본인매물이면 false.
    bool Auction_PeekBuy(int32_t listingID, const char* buyer,
        int32_t& outCode, int32_t& outUnitPrice);

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
    static CDB_Manager* m_pInstance;

    std::string m_connStr;   // ODBC 연결 문자열
};
