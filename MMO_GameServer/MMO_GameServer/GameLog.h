#pragma once
#include <cstdint>
#include <cstring>

// ================================================================
//  FGameLog - 거래 로그 1건
//
//  재화의 기록.
//  게임 DB(mmorpg)가 아니라 별도 DB(mmorpg_log)에 쌓는다.
//
//  IOCP 워커가 이 구조체를 만들어 CDB_Manager::EnqueueLog() 로 던지면,
//  로그 전용 스레드가 100건씩 묶어 다중행 INSERT 한 방으로 쓴다.
//
//  created_at 은 서버가 아니라 DB 가 찍는어 서버 여러 대의 시계가 어긋나도 기록 순서가 보장
// ================================================================

// 로그 타입 문자열. game_log.log_type 에 그대로 들어간다.
// setup.sql 의 '로그 타입 목록' 주석과 반드시 일치해야 한다
// (탐지/보상 쿼리가 이 문자열로 필터링하므로 오타 하나면 그 경로가 통째로 빠진다).
namespace LogType
{
    // 경매
    inline constexpr const char* AUCTION_LIST    = "AUCTION_LIST";      // 등록      아이템 -
    inline constexpr const char* AUCTION_BUY     = "AUCTION_BUY";       // 구매      아이템 +, 골드 -
    inline constexpr const char* AUCTION_SOLD    = "AUCTION_SOLD";      // 판매성사  (판매자 기준)
    inline constexpr const char* AUCTION_COLLECT = "AUCTION_COLLECT";   // 대금수령  골드 +
    inline constexpr const char* AUCTION_CANCEL  = "AUCTION_CANCEL";    // 취소      아이템 +

    // 재화 생성 (경제에 새로 유입)
    inline constexpr const char* DROP_GAIN       = "DROP_GAIN";         // 몬스터 드롭 획득
    inline constexpr const char* QUEST_REWARD    = "QUEST_REWARD";      // 퀘스트 보상

    // 재화 소멸 (경제에서 빠져나감)
    inline constexpr const char* SHOP_BUY        = "SHOP_BUY";          // 상점 구매 골드 -
    inline constexpr const char* SHOP_SELL       = "SHOP_SELL";         // 상점 판매 골드 +
    inline constexpr const char* ITEM_USE        = "ITEM_USE";          // 포션/스크롤 소비 아이템 -

    // 기타
    inline constexpr const char* LEVEL_UP        = "LEVEL_UP";
}

struct FGameLog
{
    char    type[24]    = {};   // LogType
    char    actor[20]   = {};   // 오너 아이디
    char    target[20]  = {};   // 거래 상대(없으면 비워둠)
    int32_t itemCode    = 0;    // 아이템 코드
    int32_t quantity    = 0;    // 아이템 증감
    int32_t gold        = 0;    // 골드 증감
    int32_t goldBalance = 0;    // 변동 후 보유 골드
    char    detail[128] = {};   // 사람이 읽을 부가 정보(드랍 몬스터 ID 등)
};

// 고정 길이 배열에 안전하게 복사(넘치면 자르고, 항상 널 종료).
// strncpy 는  넘칠 때 널 종료를 보장하지 않아 직접 복사
inline void GameLog_SetStr(char* dst, size_t cap, const char* src)
{
    if (!dst || cap == 0) return;
    size_t n = 0;
    if (src)
    {
        while (n + 1 < cap && src[n] != '\0')
        {
            dst[n] = src[n];
            ++n;
        }
    }
    dst[n] = '\0';
}

// 호출부(경매/드롭/상점 핸들러)에서 한 줄로 로그를 만들기 위한 헬퍼.
// target/detail 은 없으면 생략 가능.
inline FGameLog MakeGameLog(
    const char* type,
    const char* actor,
    int32_t     itemCode,
    int32_t     quantity,
    int32_t     gold,
    int32_t     goldBalance,
    const char* target = nullptr,
    const char* detail = nullptr)
{
    FGameLog log;
    GameLog_SetStr(log.type,   sizeof(log.type),   type);
    GameLog_SetStr(log.actor,  sizeof(log.actor),  actor);
    GameLog_SetStr(log.target, sizeof(log.target), target);
    GameLog_SetStr(log.detail, sizeof(log.detail), detail);
    log.itemCode    = itemCode;
    log.quantity    = quantity;
    log.gold        = gold;
    log.goldBalance = goldBalance;
    return log;
}
