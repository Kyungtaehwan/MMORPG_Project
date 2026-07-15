#pragma once
#include "Zone.h"

// 북쪽 몬스터 필드.
// (예전 이름 CZone_Test — 대형 레이드/부하테스트 맵을 새로 만들면서 정식 명칭으로 변경)
class CZone_Field_N : public CZone
{
public:
    CZone_Field_N() = default;
    virtual ~CZone_Field_N() = default;

    virtual void    Build()         override;
    virtual ZONE_ID Get_ZoneID()    const override { return ZONE_FIELD_N; }
    virtual void    Spawn_Objects() override;
    virtual void    Clear_Objects() override;
};
