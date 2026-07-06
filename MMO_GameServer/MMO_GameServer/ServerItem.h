#pragma once
#include <cstdint>
#include <cstdlib>

// ================================================================
//  아이템 고유 코드 규칙: code = category*1000 + subType
//   1xxx 포션, 2xxx 스크롤, 3xxx 장비, 4xxx 기타, 9000 골드
//  (서브타입 값은 클라 Item_define.h 의 enum 과 일치해야 함)
// ================================================================
constexpr int32_t ICODE_POTION = 1000;
constexpr int32_t ICODE_SCROLL = 2000;
constexpr int32_t ICODE_EQUIP  = 3000;
constexpr int32_t ICODE_ETC    = 4000;
constexpr int32_t ICODE_GOLD   = 9000;

// ================================================================
//  장비 스탯 테이블 (클라 ItemData_Equipment.cpp s_EquipTable 와 값 동일)
//  index = EQUIPMENT_TYPE 서브타입(0..38), slot = EQUIP_SLOT
//   (WEAPON=0, ARMOR=1, SHIELD=2, HELMET=3, PENDANT=4, RING=5)
// ================================================================
struct FEquipStat { int32_t atk; int32_t def; int32_t slot; };

static const FEquipStat g_EquipTable[39] =
{
    // Sword (WEAPON=0)
    {12,0,0},{15,0,0},{18,0,0},{22,0,0},{27,0,0},{33,0,0},{40,0,0},
    // Armor (ARMOR=1)
    {0,5,1},{0,8,1},{0,11,1},{0,14,1},{0,18,1},{0,23,1},{0,28,1},{0,34,1},
    // Helmet (HELMET=3)
    {0,3,3},{0,5,3},{0,7,3},{0,10,3},{0,13,3},{0,17,3},{0,22,3},
    // Shield (SHIELD=2)
    {0,4,2},{0,7,2},{0,10,2},{0,14,2},{0,19,2},{0,25,2},{0,32,2},
    // Ring (RING=5)
    {2,2,5},{4,4,5},{6,6,5},{9,9,5},{13,13,5},
    // Pendant (PENDANT=4)
    {3,0,4},{5,0,4},{8,0,4},{12,0,4},{17,0,4},
};

inline int32_t EquipAtk(int32_t code)
{
    int32_t s = code % 1000;
    return (s >= 0 && s < 39) ? g_EquipTable[s].atk : 0;
}
inline int32_t EquipDef(int32_t code)
{
    int32_t s = code % 1000;
    return (s >= 0 && s < 39) ? g_EquipTable[s].def : 0;
}
inline int32_t EquipSlotOf(int32_t code)
{
    int32_t s = code % 1000;
    return (s >= 0 && s < 39) ? g_EquipTable[s].slot : -1;
}

// ================================================================
//  포션 효과 테이블 (클라 ItemData_Potion.cpp s_PotionTable 배열 순서와 동일)
//   [0]HP30 [1]HP50 [2]MP30 [3]MP50 [4]ATK버프10 [5]무적
//  (POTION_TYPE enum 이름과 [4][5] 순서가 클라에서 뒤바뀌어 있으므로,
//   "플레이어가 보는 아이콘"과 효과가 일치하도록 배열 순서를 따른다)
// ================================================================
enum EPotionKind { PK_HP, PK_MP, PK_ATK, PK_INVINCIBLE };
struct FPotionEffect { EPotionKind kind; int32_t amount; };

static const FPotionEffect g_PotionTable[6] =
{
    { PK_HP, 30 }, { PK_HP, 50 },
    { PK_MP, 30 }, { PK_MP, 50 },
    { PK_ATK, 10 }, { PK_INVINCIBLE, 0 },
};

constexpr uint32_t ATK_BUFF_DURATION_MS   = 15000;
constexpr uint32_t INVINCIBLE_DURATION_MS = 8000;

// ================================================================
//  상점 가격 (포션). 인덱스 = code%1000 (아이콘/배열 순서 기준)
//   [0]HP30 [1]HP50 [2]MP30 [3]MP50 [4]ATK버프 [5]무적
//  클라 UI_Shop.cpp s_ShopPotions 의 price 와 값이 동일해야 함.
//  판매가 = 구매가 / 2.
// ================================================================
static const int32_t g_PotionBuyPrice[6] =
{
    30, 50,    // HP 중 / 대
    30, 50,    // MP 중 / 대
    120,       // 공격력 버프
    200,       // 무적
};

// 포션 구매가. 포션이 아니거나 범위 밖이면 0(구매 불가).
inline int32_t PotionBuyPrice(int32_t code)
{
    if (code / 1000 != 1) return 0;
    int s = code % 1000;
    return (s >= 0 && s < 6) ? g_PotionBuyPrice[s] : 0;
}

// 판매가(전 아이템). 클라 UI_Shop.cpp Shop_SellPrice 와 값이 반드시 동일해야 함.
//   포션 : 구매가/2
//   장비 : (공격+방어)*5   (g_EquipTable 스탯 기반)
//   스크롤: 20 고정
//   기타 : 5 고정
//   그 외(골드 등): 0 = 판매 불가
inline int32_t ItemSellPrice(int32_t code)
{
    switch (code / 1000)
    {
    case 1:  return PotionBuyPrice(code) / 2;
    case 2:  return 20;
    case 3:  return (EquipAtk(code) + EquipDef(code)) * 5;
    case 4:  return 5;
    default: return 0;
    }
}

struct FDropRoll
{
    bool    bDrop;
    int32_t code;
    int32_t amount;   // 골드면 금액, 그 외 개수
};

// 몬스터 사망 시 1회 추첨 (현재 ORC 공용 테이블)
inline FDropRoll RollDrop()
{
    FDropRoll r{ false, 0, 0 };

    // 30% 확률로 드롭 없음
    if (rand() % 100 < 30) return r;

    r.bDrop = true;
    int pick = rand() % 100;

    if (pick < 40)
    {
        r.code = ICODE_GOLD;            // 골드 5~30
        r.amount = 5 + rand() % 26;
    }
    else if (pick < 60)
    {
        r.code = ICODE_POTION + (rand() % 6);  // 포션 6종(HP/MP/공격/무적)
        r.amount = 1;
    }
    else if (pick < 90)
    {
        r.code = ICODE_ETC + (rand() % 10);    // 기타 잡템 0~9
        r.amount = 1;
    }
    else
    {
        r.code = ICODE_EQUIP + (rand() % 3);   // 소드 0~2
        r.amount = 1;
    }
    return r;
}
