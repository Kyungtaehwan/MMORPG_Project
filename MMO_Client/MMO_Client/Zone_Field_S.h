#pragma once
#include "Zone.h"

// 남쪽 몬스터 필드.
class CZone_Field_S : public CZone
{
public:
    CZone_Field_S() = default;
    virtual ~CZone_Field_S() = default;

    virtual void    Build()         override;
    virtual ZONE_ID Get_ZoneID()    const override { return ZONE_FIELD_S; }
    virtual void    Spawn_Objects() override;
    virtual void    Clear_Objects() override;
};
