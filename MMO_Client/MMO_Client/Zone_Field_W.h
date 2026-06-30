#pragma once
#include "Zone.h"

// 서쪽 몬스터 필드.
class CZone_Field_W : public CZone
{
public:
    CZone_Field_W() = default;
    virtual ~CZone_Field_W() = default;

    virtual void    Build()         override;
    virtual ZONE_ID Get_ZoneID()    const override { return ZONE_FIELD_W; }
    virtual void    Spawn_Objects() override;
    virtual void    Clear_Objects() override;
};
