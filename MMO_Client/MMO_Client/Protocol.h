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

    // Server → Client
    //세션, 플레이어 관련
    SC_LOGIN_OK = 2000,
    SC_LOGIN_FAIL = 2001,
    SC_ENTER_GAME = 2002,
    SC_ADD_PLAYER = 2003,
    SC_REMOVE_PLAYER = 2004,
    SC_MOVE_PLAYER = 2005,  // 목적지 + 현재위치 포함 (보간용)

    //몬스터 용
    SC_ADD_MONSTER = 2100,
    SC_REMOVE_MONSTER = 2101,
    SC_MOVE_MONSTER = 2102,
    SC_MONSTER_STATE = 2103,

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

#pragma pack(pop)