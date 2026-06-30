#pragma once
#include "Zone.h"

// 동쪽 몬스터 필드.
class CZone_Field_E : public CZone
{
public:
    CZone_Field_E() = default;
    virtual ~CZone_Field_E() = default;

    virtual void    Build()         override;
    virtual ZONE_ID Get_ZoneID()    const override { return ZONE_FIELD_E; }
    virtual void    Spawn_Objects() override;
    virtual void    Clear_Objects() override;
};
