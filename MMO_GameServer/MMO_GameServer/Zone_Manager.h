#pragma once
#include "Zone.h"
#include <unordered_map>
#include <mutex>

enum ZONE_ID : int32_t
{
    ZONE_TEST = 0,   // 북쪽 몬스터 필드 (기존)
    ZONE_TOWN = 1,
    ZONE_FIELD_E = 2,   // 동쪽 필드
    ZONE_FIELD_S = 3,   // 남쪽 필드
    ZONE_FIELD_W = 4,   // 서쪽 필드
};

class CZone_Manager
{
private:
    CZone_Manager();
    ~CZone_Manager();

public:
    static CZone_Manager* Get_Instance()
    {
        if (!m_pInstance)
            m_pInstance = new CZone_Manager;
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

    CZone_Manager(const CZone_Manager&) = delete;
    CZone_Manager& operator=(const CZone_Manager&) = delete;

    CZone* GetZone(int32_t nZoneID);

private:
    static CZone_Manager* m_pInstance;

    // 존 ID → CZone 포인터
    std::unordered_map<int32_t, CZone*> m_zones;
};