#pragma once
#include "Zone.h"

// 북쪽 몬스터 필드.
class CZone_Test : public CZone
{
public:
    CZone_Test() = default;
    virtual ~CZone_Test() = default;

    virtual void    Build()         override;
    virtual ZONE_ID Get_ZoneID()    const override { return ZONE_TEST; }
    virtual void    Spawn_Objects() override;
    virtual void    Clear_Objects() override;
};