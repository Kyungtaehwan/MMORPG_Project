#pragma once
#include "StaticObject.h"


class CWagonDrink : public CStaticObject
{
public:
    CWagonDrink() = default;
    virtual ~CWagonDrink() = default;

public:
    virtual void Initialize() override;
};
