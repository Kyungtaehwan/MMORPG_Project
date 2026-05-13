#include "pch.h"
#include "Monster_Manager.h"

CMonster_Manager* CMonster_Manager::m_pInstance = nullptr;

MonsterRef CMonster_Manager::Create(int32_t nID, MONSTER_TYPE eType,
    float fX, float fZ, int32_t nZoneID)
{
    if (nID < 0 || nID >= MAX_MONSTER) return nullptr;

    std::lock_guard<std::mutex> lock(m_lock);

    // 이미 존재하면 실패
    if (m_monsters[nID]) return nullptr;

    auto pMonster = std::make_shared<CMonster>();
    pMonster->m_nMonsterID = nID;
    pMonster->m_eType = eType;
    pMonster->m_nZoneID = nZoneID;

    // 현재 위치 = 목적지 = 스폰 위치로 초기화
    pMonster->m_fCurX = fX;
    pMonster->m_fCurZ = fZ;
    pMonster->m_fDestX = fX;
    pMonster->m_fDestZ = fZ;
    pMonster->m_fSpawnX = fX;
    pMonster->m_fSpawnZ = fZ;

    pMonster->UpdateTilePos();

    m_monsters[nID] = pMonster;

    std::cout << "[Monster_Manager] 몬스터 생성. ID=" << nID
        << " Type=" << (int)eType
        << " pos=(" << fX << ", " << fZ << ")" << std::endl;

    return pMonster;
}

MonsterRef CMonster_Manager::Get_Monster(int32_t nID)
{
    if (nID < 0 || nID >= MAX_MONSTER) return nullptr;

    std::lock_guard<std::mutex> lock(m_lock);
    return m_monsters[nID];
}

void CMonster_Manager::Remove(int32_t nID)
{
    if (nID < 0 || nID >= MAX_MONSTER) return;

    std::lock_guard<std::mutex> lock(m_lock);
    m_monsters[nID] = nullptr;

    std::cout << "[Monster_Manager] 몬스터 제거. ID=" << nID << std::endl;
}