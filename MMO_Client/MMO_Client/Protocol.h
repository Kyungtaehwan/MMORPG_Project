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
    // Client → Server
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

    // Server → Client
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
    //몬스터 용
    SC_ADD_MONSTER = 2100,
    SC_REMOVE_MONSTER = 2101,
    SC_MOVE_MONSTER = 2102,
    SC_MONSTER_STATE = 2103,
    SC_MONSTER_HIT = 2104,
};

// ---- C→S ----

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

// ---- S→C ----

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
// 현재위치 + 목적지 포함 → 클라이언트가 보간
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
#pragma pack(pop)