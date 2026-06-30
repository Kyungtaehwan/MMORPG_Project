#include "pch.h"
#include "Map_Manager.h"
#include "Object_Manager.h"
#include "NPC_Shop.h"
#include "Zone_Test.h"
#include "Zone_Town.h"
#include "Zone_Field_E.h"
#include "Zone_Field_S.h"
#include "Zone_Field_W.h"

CMap_Manager* CMap_Manager::m_pInstance = nullptr;
void CMap_Manager::Initialize()
{
    Release();
    m_vecZone.assign(ZONE_MAX, nullptr);
    Change_Zone(ZONE_TEST);
}

void CMap_Manager::Update()
{
    if (m_bLoading.load())  return;
    if (!m_pPendingZone)    return;

    std::lock_guard<std::mutex> lock(m_ZoneMutex);

    ZONE_ID ePrevID = Get_CurZoneID();
    if (m_pCurZone)
        m_pCurZone->Clear_Objects();

    m_pCurZone = m_pPendingZone;
    m_pPendingZone = nullptr;

    // 새 존의 오브젝트(NPC/포탈) 생성. 동기 Change_Zone은 이걸 직접 하지만
    // 비동기 경로는 swap 이후 여기서 해줘야 함.
    if (m_pCurZone)
        m_pCurZone->Spawn_Objects();

    if (ePrevID != ZONE_MAX)
        Unload_Zone(ePrevID);
}

CZone* CMap_Manager::Create_Zone(ZONE_ID eID)
{
    switch (eID)
    {
    case ZONE_TEST:    return new CZone_Test;
    case ZONE_TOWN:    return new CZone_Town;
    case ZONE_FIELD_E: return new CZone_Field_E;
    case ZONE_FIELD_S: return new CZone_Field_S;
    case ZONE_FIELD_W: return new CZone_Field_W;
    default:           return nullptr;
    }
}

void CMap_Manager::Load_Zone(ZONE_ID eID)
{
    if (m_vecZone[eID]) return;
    CZone* pZone = Create_Zone(eID);
    if (!pZone) return;
    pZone->Build();
    m_vecZone[eID] = pZone;
}

void CMap_Manager::Unload_Zone(ZONE_ID eID)
{
    if (eID < 0 || eID >= ZONE_MAX)   return;
    if (!m_vecZone[eID])              return;
    if (m_vecZone[eID] == m_pCurZone) return;
    delete m_vecZone[eID];
    m_vecZone[eID] = nullptr;
}

void CMap_Manager::Change_Zone(ZONE_ID eID)
{
    if (eID < 0 || eID >= ZONE_MAX) return;
    ZONE_ID ePrevID = Get_CurZoneID();
    Load_Zone(eID);
    if (m_pCurZone)
        m_pCurZone->Clear_Objects();
    m_pCurZone = m_vecZone[eID];
    m_pCurZone->Spawn_Objects();
    if (ePrevID != ZONE_MAX && ePrevID != eID)
        Unload_Zone(ePrevID);
}

void CMap_Manager::Change_Zone_Async(ZONE_ID eID)
{
    if (eID < 0 || eID >= ZONE_MAX) return;
    if (m_bLoading.load())          return;

    m_bLoading.store(true);
    m_fLoadProgress.store(0.f);

    std::thread([this, eID]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            m_fLoadProgress.store(0.2f);

            Load_Zone(eID);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            m_fLoadProgress.store(0.9f);
            {
                std::lock_guard<std::mutex> lock(m_ZoneMutex);
                m_pPendingZone = m_vecZone[eID];
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            m_fLoadProgress.store(1.f);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            m_bLoading.store(false);
        }).detach();
}


bool CMap_Manager::Is_Movable(int x, int z)
{
    if (!m_pCurZone) return false;
    return m_pCurZone->Is_Movable(x, z);
}

TILE_TYPE CMap_Manager::Get_TileType(int x, int z)
{
    if (!m_pCurZone) return TILE_OUTSIDE;
    return m_pCurZone->Get_TileType(x, z);
}

void CMap_Manager::Render(ID2D1RenderTarget* pRT)
{
    if (!m_pCurZone || m_bLoading.load()) return;
    m_pCurZone->Render(pRT);
}

ZONE_ID CMap_Manager::Get_CurZoneID() const
{
    if (!m_pCurZone) return ZONE_MAX;
    return m_pCurZone->Get_ZoneID();
}

void CMap_Manager::Release()
{
    for (auto& pZone : m_vecZone)
    {
        delete pZone;
        pZone = nullptr;
    }
    m_vecZone.clear();
    m_pCurZone = nullptr;
    m_pPendingZone = nullptr;
}