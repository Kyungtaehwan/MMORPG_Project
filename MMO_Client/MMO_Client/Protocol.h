#pragma once
#include <cstdint>

#pragma pack(push, 1)

struct PacketHeader
{
    uint16_t size;
    uint16_t id;
};

enum PacketID : uint16_t
{
    // Client - Server
    CS_LOGIN = 1000,
    CS_MOVE_DEST = 1001,  // 마우스 클릭  목적지 전송
    CS_MOVE_POS = 1002,  // 이동 중 타일 변경 시  현재 위치 전송
    CS_ATTACK_MONSTER = 1003,
    CS_RESPAWN = 1004,
    CS_PORTAL = 1005,   // 포탈 존 이동 요청
    CS_PICKUP = 1006,   // 아이템 줍기 요청
    CS_EQUIP = 1007,    // 인벤 슬롯 아이템 장착
    CS_UNEQUIP = 1008,  // 장비 슬롯 해제
    CS_USE_ITEM = 1009, // 인벤 슬롯 아이템 사용(포션 등)
    CS_MOVE_STOP = 1010, // UI 진입 등으로 이동 강제 정지(현재 위치 커밋)
    CS_BUY = 1011,      // 상점 구매 (itemCode를 count개)
    CS_SELL = 1012,     // 상점 판매 (인벤 슬롯의 아이템 count개)
    CS_AUCTION_LIST = 1013,      // 경매장 목록 요청
    CS_AUCTION_REGISTER = 1014,  // 경매장 등록 (인벤슬롯 count개, 개당가격)
    CS_AUCTION_BUY = 1015,       // 경매장 구매 (listingID count개)
    CS_AUCTION_COLLECT = 1016,   // 판매대금 수령 (listingID)
    CS_AUCTION_CANCEL = 1017,    // 등록 취소 (남은수량 반환 + 미수령골드 지급)

    // Server - Client
    //세션, 플레이어 관련
    SC_LOGIN_OK = 2000,
    SC_LOGIN_FAIL = 2001,
    SC_ENTER_GAME = 2002,
    SC_ADD_PLAYER = 2003,
    SC_REMOVE_PLAYER = 2004,
    SC_MOVE_PLAYER = 2005,  // 목적지 + 현재위치 포함 (보간용)
    SC_PLAYER_STATE = 2006,
    SC_PLAYER_HIT = 2007,
    SC_RESPAWN = 2008,
    SC_CHANGE_ZONE = 2009,  // 존 전환 확정 (클라가 맵 로드 + 위치 이동)
    SC_ADD_DROP = 2010,     // 월드에 아이템 드롭 생성
    SC_REMOVE_DROP = 2011,  // 드롭 제거 (획득/소멸)
    SC_INVEN_UPDATE = 2012, // 인벤토리 전체 스냅샷 (장비 포함)
    SC_PLAYER_HP = 2013,    // HP/MP 동기화 (회복 등, 피격 애니 없음)
    SC_BUFF = 2014,         // 버프 적용 알림 (클라가 자체 타이머 표시)
    SC_AUCTION_LIST = 2015, // 경매장 매물 전체 스냅샷
    //몬스터 용
    SC_ADD_MONSTER = 2100,
    SC_REMOVE_MONSTER = 2101,
    SC_MOVE_MONSTER = 2102,
    SC_MONSTER_STATE = 2103,
    SC_MONSTER_HIT = 2104,
};

// ---- C-S ----

struct CS_LOGIN_PACKET
{
    PacketHeader header;
    char         id[20];
    char         pw[20];
};

// 마우스 클릭 시 1번 전송
// 서버: 목적지 검증 + 브로드캐스트
struct CS_MOVE_DEST_PACKET
{
    PacketHeader header;
    float        fDestX;
    float        fDestZ;
    uint32_t     moveTime;
};

// 이동 중 타일이 바뀔 때마다 전송
// 서버: 현재 위치 업데이트 + 시야 재계산
struct CS_MOVE_POS_PACKET
{
    PacketHeader header;
    float        fCurX;    // 현재 위치
    float        fCurZ;
    uint32_t     moveTime;
};

// ---- S-C ----

struct SC_LOGIN_OK_PACKET
{
    PacketHeader header;
    uint32_t     playerID;
};

struct SC_LOGIN_FAIL_PACKET
{
    PacketHeader header;
    uint8_t      reason;
};

struct SC_ENTER_GAME_PACKET
{
    PacketHeader header;
    uint32_t     playerID;
    float        fCurX;
    float        fCurZ;
    int32_t      zoneID;
    char         name[20];   // 내 캐릭터 이름(계정 id) — 머리 위 표시용
};

struct SC_ADD_PLAYER_PACKET
{
    PacketHeader header;
    uint32_t     playerID;
    float        fCurX;
    float        fCurZ;
    float        fDestX;
    float        fDestZ;
    float        fSpeed;
    uint8_t      state;
    uint8_t      dir;
    char         name[20];
};

struct SC_REMOVE_PLAYER_PACKET
{
    PacketHeader header;
    uint32_t     playerID;
};

// 이동 브로드캐스트
// 현재위치 + 목적지 포함 - 클라이언트가 보간
struct SC_MOVE_PLAYER_PACKET
{
    PacketHeader header;
    uint32_t     playerID;
    float        fCurX;    // 현재 위치 (보간 시작점)
    float        fCurZ;
    float        fDestX;   // 목적지
    float        fDestZ;
    float        fSpeed;
    uint32_t     moveTime;
};

struct SC_ADD_MONSTER_PACKET
{
    PacketHeader  header;
    int32_t       monsterID;
    uint8_t       monsterType;
    uint8_t       state;
    float         fCurX;
    float         fCurZ;
    float         fDestX;
    float         fDestZ;
    float         fSpeed;
};

struct SC_REMOVE_MONSTER_PACKET
{
    PacketHeader  header;
    int32_t       monsterID;
};

struct SC_MOVE_MONSTER_PACKET
{
    PacketHeader  header;
    int32_t       monsterID;
    float         fCurX;
    float         fCurZ;
    float         fDestX;
    float         fDestZ;
    float         fSpeed;
    uint8_t       dir;
    uint32_t      moveTime;
};

struct SC_MONSTER_STATE_PACKET
{
    PacketHeader  header;
    int32_t       monsterID;
    uint8_t       state;
    uint8_t       dir;
    int32_t       targetID;
};

struct CS_ATTACK_MONSTER_PACKET
{
    PacketHeader header;
    int32_t      monsterID;
    float        fCurX;
    float        fCurZ;
};

struct SC_PLAYER_STATE_PACKET
{
    PacketHeader header;
    uint32_t     playerID;
    uint8_t      state;
};

struct SC_PLAYER_HIT_PACKET
{
    PacketHeader header;
    uint32_t     playerID;
    int32_t      nHp;
    int32_t      nMaxHp;
};

struct CS_RESPAWN_PACKET
{
    PacketHeader header;
};

struct SC_RESPAWN_PACKET
{
    PacketHeader header;
    float        fCurX;
    float        fCurZ;
    int32_t      nHp;
    int32_t      nMaxHp;
};

struct SC_MONSTER_HIT_PACKET
{
    PacketHeader header;
    int32_t      monsterID;
    int32_t      nHp;
    int32_t      nMaxHp;
    uint8_t      dir;
};

// ---- 포탈 / 존 전환 ----
struct CS_PORTAL_PACKET
{
    PacketHeader header;
    int32_t      targetZone;
    float        spawnX;
    float        spawnZ;
};

struct SC_CHANGE_ZONE_PACKET
{
    PacketHeader header;
    int32_t      zoneID;
    float        spawnX;
    float        spawnZ;
};

// ---- 아이템 드롭 / 인벤토리 ----
// itemCode = category*1000 + subType
//   1xxx 포션, 2xxx 스크롤, 3xxx 장비, 4xxx 기타, 9000 골드
struct CS_PICKUP_PACKET
{
    PacketHeader header;
    uint32_t     dropId;
};

struct SC_ADD_DROP_PACKET
{
    PacketHeader header;
    int32_t      dropId;
    int32_t      itemCode;
    int32_t      amount;   // 골드면 금액, 그 외 개수
    float        fX;
    float        fZ;
};

struct SC_REMOVE_DROP_PACKET
{
    PacketHeader header;
    int32_t      dropId;
};

// 인벤토리 전체 스냅샷 (40 = 클라 INVEN_SIZE, 6 = SLOT_END)
struct SC_INVEN_UPDATE_PACKET
{
    PacketHeader header;
    int32_t      gold;
    int32_t      codes[40];
    int32_t      counts[40];
    int32_t      equip[6];   // 장비 슬롯별 itemCode (0=빈칸)
};

// ---- 장비 / 사용 / 스탯 ----
struct CS_EQUIP_PACKET
{
    PacketHeader header;
    int32_t      invenSlot;
};

struct CS_UNEQUIP_PACKET
{
    PacketHeader header;
    int32_t      equipSlot;
};

struct CS_USE_ITEM_PACKET
{
    PacketHeader header;
    int32_t      invenSlot;
};

// UI 진입 등으로 이동 중단. 서버: 현재 위치 커밋 + m_bMoving=false.
struct CS_MOVE_STOP_PACKET
{
    PacketHeader header;
    float        fCurX;
    float        fCurZ;
};

// 상점 구매. 서버가 골드 검증 후 인벤 추가 - SC_INVEN_UPDATE 응답.
struct CS_BUY_PACKET
{
    PacketHeader header;
    int32_t      itemCode;
    int32_t      count;
};

// 상점 판매. 서버가 인벤 차감 후 골드 지급 - SC_INVEN_UPDATE 응답.
struct CS_SELL_PACKET
{
    PacketHeader header;
    int32_t      invenSlot;
    int32_t      count;
};

struct SC_PLAYER_HP_PACKET
{
    PacketHeader header;
    uint32_t     playerID;
    int32_t      nHp;
    int32_t      nMaxHp;
    int32_t      nMp;
    int32_t      nMaxMp;
};

// 버프 적용 알림. buffType: 0=공격력, 1=무적. durationMs 동안 지속.
struct SC_BUFF_PACKET
{
    PacketHeader header;
    uint32_t     playerID;
    int32_t      buffType;
    int32_t      durationMs;
};

// ================= 경매장 =================
constexpr int32_t AUCTION_MAX = 64;        // 패킷 entries 배열 크기(클라 버퍼와 공유)
constexpr int32_t AUCTION_PAGE_SIZE = 5;   // 한 페이지 매물 수(클라 UI 행수와 일치)
constexpr int32_t AUCTION_SEARCH_MAX = 32; // 검색어 일치 아이템 코드 최대 개수

// 매물 1건. 인스턴스ID 없이 (코드,개수) 모델. 개당 가격/부분 구매.
struct FAuctionEntry
{
    int32_t listingID;
    int32_t itemCode;
    int32_t count;        // 남은 수량
    int32_t unitPrice;    // 개당 가격
    int32_t pendingGold;  // 판매되어 미수령한 골드(소유자에게만 의미)
    char    sellerName[20];
};

// 경매 목록 요청. tab=0 구매(남의 매물), 1 내판매. searchCount>0 이면 item_code IN(searchCodes) 필터.
struct CS_AUCTION_LIST_PACKET
{
    PacketHeader header;
    int32_t page;        // 0-base 페이지 번호
    int32_t tab;         // 0=구매, 1=내판매
    int32_t searchCount; // 검색 코드 개수(0=검색없음)
    int32_t searchCodes[AUCTION_SEARCH_MAX];
};
struct CS_AUCTION_REGISTER_PACKET { PacketHeader header; int32_t invenSlot; int32_t count; int32_t unitPrice; };
struct CS_AUCTION_BUY_PACKET      { PacketHeader header; int32_t listingID; int32_t count; };
struct CS_AUCTION_COLLECT_PACKET  { PacketHeader header; int32_t listingID; };
struct CS_AUCTION_CANCEL_PACKET   { PacketHeader header; int32_t listingID; };

// 경매장 한 페이지 스냅샷(최신 등록순). 등록/구매/수령/취소 후에도 현재 페이지 재전송.
struct SC_AUCTION_LIST_PACKET
{
    PacketHeader  header;
    int32_t       page;                // 이 응답의 페이지 번호(0-base)
    int32_t       count;               // 이 페이지의 유효 매물 수 (<= AUCTION_PAGE_SIZE)
    int32_t       hasNext;             // 다음 페이지 존재 여부(1/0)
    FAuctionEntry entries[AUCTION_MAX];
};
#pragma pack(pop)