#pragma once
#include "Zone.h"

// ================================================================
//  대형 맵 150x150 — 레이드 필드 + 부하/길찾기 측정용
//
//  두 클래스는 장애물 유무만 다르다. 크기·스폰·타일셋이 전부 같아야
//  "장애물이 A* 비용에 얼마나 영향을 주는가"를 단독 변수로 비교할 수 있다.
//  블록맵은 RaidMap.h 가 고정 시드로 생성하며, 서버 Zone_Manager 도
//  같은 함수를 호출하므로 두 맵이 반드시 일치한다.
// ================================================================

// 장애물 있는 대형 맵 (A* 길찾기 부하 발생)
class CZone_Raid : public CZone
{
public:
    CZone_Raid() = default;
    virtual ~CZone_Raid() = default;

    virtual void    Build()         override;
    virtual ZONE_ID Get_ZoneID()    const override { return ZONE_RAID; }
    virtual void    Spawn_Objects() override;
    virtual void    Clear_Objects() override;
};

// 장애물 없는 평지 대형 맵 (대조군)
class CZone_Raid_Flat : public CZone
{
public:
    CZone_Raid_Flat() = default;
    virtual ~CZone_Raid_Flat() = default;

    virtual void    Build()         override;
    virtual ZONE_ID Get_ZoneID()    const override { return ZONE_RAID_FLAT; }
    virtual void    Spawn_Objects() override;
    virtual void    Clear_Objects() override;
};
