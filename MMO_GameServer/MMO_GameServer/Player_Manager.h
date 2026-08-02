#pragma once
#include "Player.h"
#include <array>
#include <mutex>

constexpr int32_t MAX_PLAYER = 20000;

class CPlayer_Manager
{
private:
    CPlayer_Manager() = default;
    ~CPlayer_Manager() = default;

public:
    static CPlayer_Manager* Get_Instance()
    {
        if (!m_pInstance)
            m_pInstance = new CPlayer_Manager;
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

    CPlayer_Manager(const CPlayer_Manager&) = delete;
    CPlayer_Manager& operator=(const CPlayer_Manager&) = delete;

    // 플레이어 생성 \ 세션 ID와 동일한 슬롯에 배치
    PlayerRef Create(int32_t nSessionID);

    // 조회
    PlayerRef Get_Player(int32_t nPlayerID);

    // 제거 (로그아웃)
    void      Remove(int32_t nPlayerID);

    // 주기 저장(라운드로빈): 다음 온라인 플레이어 1명을 골라 스냅샷-DB 저장.
    // 타이머(PlayerAutoSave)가 매 틱 호출. 한 틱에 1명씩 분산 - 워커 블로킹 최소화.
    void      AutoSaveNext();

private:
    static CPlayer_Manager* m_pInstance;

    std::array<PlayerRef, MAX_PLAYER> m_players;

    // 읽기/쓰기 락. 실제 타입은 ServerConfig.h의 USE_RW_LOCK이 정한다.
    // - Get_Player(읽기)는 브로드캐스트 루프에서 존 인원수만큼 불린다
    // - Create/Remove(쓰기)는 접속과 종료 때 한 번씩뿐이다
    FRWLock                           m_lock;

    int32_t                           m_saveCursor = 0;   // 라운드로빈 커서
};