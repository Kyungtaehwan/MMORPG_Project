#pragma once
#include "StaticObject.h"

// 수레(식료) - 정적 오브젝트(비상호작용).
class CWagonFood : public CStaticObject
{
public:
    CWagonFood() = default;
    virtual ~CWagonFood() = default;

public:
    virtual void Initialize() override;
};
