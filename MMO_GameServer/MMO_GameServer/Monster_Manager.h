#pragma once
#include "Monster.h"
#include <array>
#include <mutex>
#include <memory>

constexpr int32_t MAX_MONSTER = 500;

class CMonster_Manager
{
private:
    CMonster_Manager() = default;
    ~CMonster_Manager() = default;

public:
    static CMonster_Manager* Get_Instance()
    {
        if (!m_pInstance)
            m_pInstance = new CMonster_Manager;
        return m_pInstance;
    }

    static void Destroy_Instance()
    {
        delete m_pInstance;
        m_pInstance = nullptr;
    }

    // 몬스터 생성
    // nID     : 몬스터 고유 ID (0 ~ MAX_MONSTER-1)
    // eType   : 몬스터 종류
    // fX, fZ  : 스폰 위치
    // nZoneID : 소속 존
    MonsterRef Create(int32_t nID, MONSTER_TYPE eType,
        float fX, float fZ, int32_t nZoneID);

    // ID로 몬스터 참조 반환
    MonsterRef Get_Monster(int32_t nID);

    // 몬스터 제거
    void Remove(int32_t nID);

private:
    static CMonster_Manager* m_pInstance;

    // 고정 배열로 관리 (동적 할당 없음)
    std::array<MonsterRef, MAX_MONSTER> m_monsters;
    std::mutex                          m_lock;
};