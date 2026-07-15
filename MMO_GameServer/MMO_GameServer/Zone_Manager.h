#pragma once
#include "Zone.h"
#include <unordered_map>
#include <mutex>

// 클라 define.h 의 ZONE_ID 와 숫자까지 일치해야 함.
enum ZONE_ID : int32_t
{
    ZONE_FIELD_N = 0,   // 북쪽 필드
    ZONE_TOWN = 1,
    ZONE_FIELD_E = 2,   // 동쪽 필드
    ZONE_FIELD_S = 3,   // 남쪽 필드
    ZONE_FIELD_W = 4,   // 서쪽 필드

    // ---- 대형 맵 150x150 (레이드 + 부하/길찾기 측정용) ----
    // 두 맵은 장애물 유무만 다르고 크기/스폰/몬스터 수가 같다.
    // 그래야 "장애물이 A* 비용에 얼마나 영향을 주는가"를 단독 변수로 비교할 수 있다.
    ZONE_RAID = 5,        // 장애물 있음 (A* 길찾기 부하)
    ZONE_RAID_FLAT = 6,   // 장애물 없는 평지 (대조군)
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