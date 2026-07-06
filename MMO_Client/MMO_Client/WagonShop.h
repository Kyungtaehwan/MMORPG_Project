#pragma once
#include "StaticObject.h"

// 수레(잡화) - 정적 오브젝트(비상호작용).
class CWagonShop : public CStaticObject
{
public:
    CWagonShop() = default;
    virtual ~CWagonShop() = default;

public:
    virtual void Initialize() override;
};
