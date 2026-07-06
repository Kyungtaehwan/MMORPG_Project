#pragma once
#include "StaticObject.h"

// 수레(음료) - 정적 오브젝트(비상호작용).
class CWagonDrink : public CStaticObject
{
public:
    CWagonDrink() = default;
    virtual ~CWagonDrink() = default;

public:
    virtual void Initialize() override;
};
