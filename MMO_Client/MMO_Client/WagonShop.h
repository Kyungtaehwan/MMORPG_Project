#pragma once
#include "StaticObject.h"


class CWagonShop : public CStaticObject
{
public:
    CWagonShop() = default;
    virtual ~CWagonShop() = default;

public:
    virtual void Initialize() override;
};
