#pragma once
#include <cstdint>

// ================================================================
//  FSaveSnapshot — 저장용 플레이어 상태 스냅샷
//
//  주기 저장(타이머)은 "살아있는 플레이어"의 데이터를 다른 스레드에서
//  읽어야 한다. 그 순간 게임 스레드가 인벤을 수정 중이면 반쪽 상태가
//  저장될 수 있으므로(torn snapshot), CPlayer::TakeSnapshot()이 짧은
//  락으로 이 구조체에 값을 통째로 복사한 뒤, 느린 DB I/O는 락 밖에서
//  이 복사본으로 수행한다. (락 보유 시간 최소화)
// ================================================================
struct FSaveSnapshot
{
    static constexpr int INVEN = 40;   // CPlayer::INVEN_SIZE 와 일치
    static constexpr int EQUIP = 6;    // CPlayer::EQUIP_SLOTS 와 일치

    char    id[20]  = {};   // account_id (= CPlayer::m_szName)
    int32_t zoneID  = 0;
    float   x       = 0.f;
    float   z       = 0.f;
    int32_t gold    = 0;
    int32_t invenCode[INVEN]  = {};
    int32_t invenCount[INVEN] = {};
    int32_t equipCode[EQUIP]  = {};
};
