#pragma once
#include <cstdint>
#include <cstring>
#include "Zone_Manager.h"   // ZONE_ID

// ================================================================
//  AccountDB — DB 대체용 하드코딩 계정 저장소 (임시)
//
//  실제 DB를 붙일 때는 이 파일의 FindAccount()만 DB 조회로 교체하면 됨.
//  게임 로직(Packet_Handler)은 FAccountData 구조체 + FindAccount 인터페이스에만
//  의존하도록 짜여 있어, 데이터 출처(하드코딩 → DB)만 갈아끼우면 된다.
//
//  DB가 없으므로 매 로그인마다 아래 값 그대로(같은 인벤/골드/위치) 시작한다.
// ================================================================

struct FSaveItem { int32_t code; int32_t count; };   // code==0 이면 목록 끝

struct FAccountData
{
    const char* id;
    const char* pw;
    int32_t     zoneID;      // 로그인 시작 존 (ZONE_ID)
    float       spawnX;
    float       spawnZ;
    int32_t     gold;
    int32_t     level;       // 레벨 (1 시작)
    int32_t     exp;         // 현재 레벨에서 쌓은 경험치
    FSaveItem   inven[16];   // 인벤 아이템 (code==0에서 종료)
    int32_t     equip[6];    // 슬롯별 장착 코드 (0=없음), 클라 EQUIP_SLOT 순서
    int32_t     quick[8];    // 퀵슬롯 등록 코드 (0=빈칸), 클라 CUI_QuickSlot 순서
};

// 하드코딩 계정 2개
static const FAccountData g_Accounts[] =
{
    {
        "test1", "1234",
        ZONE_TOWN, 12.f, 20.f,          // 마을, 상점 NPC(12,17) 근처
        1000,
        1, 0,                           // level, exp
        { {1000,5}, {1001,2}, {1004,1}, {3001,1}, {0,0} },  // HP중5,HP대2,공격력물약1,소드1(인벤)
        { 3000, 0, 0, 0, 0, 0 },                             // 무기 슬롯에 소드0 장착
        { 1000, 1001, 0, 0, 0, 0, 0, 0 }                     // 퀵슬롯 1,2번에 HP포션
    },
    {
        "test2", "1234",
        ZONE_FIELD_N, 10.f, 10.f,          // 북쪽 필드
        300,
        1, 0,                           // level, exp
        { {1002,5}, {1005,1}, {0,0} },                      // MP중5, 무적물약1
        { 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 }
    },
};

// id/pw 일치 계정 반환. 없으면 nullptr. (추후 DB 조회로 교체 지점)
inline const FAccountData* FindAccount(const char* id, const char* pw)
{
    for (const FAccountData& a : g_Accounts)
        if (strcmp(a.id, id) == 0 && strcmp(a.pw, pw) == 0)
            return &a;
    return nullptr;
}
