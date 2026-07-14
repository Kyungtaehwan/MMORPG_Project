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

    // 타입별 스탯 (클라 서브클래스 Initialize의 값과 반드시 일치해야 함)
    // 특히 최대 체력이 다르면 클라 HP바(m_iHp>=m_iMaxHp면 숨김)가 어긋난다
    switch (eType)
    {
    case MONSTER_WING:
        pMonster->m_nHp = pMonster->m_nMaxHp = 80;   // 클라 CMonster_Wing과 동일
        pMonster->m_nExpReward = 25;                 // HP는 낮지만 직선 추적이라 성가심
        break;
    case MONSTER_ORC:
    default:
        pMonster->m_nHp = pMonster->m_nMaxHp = 100;
        pMonster->m_nExpReward = 20;                 // Lv1→2(100exp) = 5마리
        break;
    }

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