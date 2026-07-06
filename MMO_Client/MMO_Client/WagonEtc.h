#pragma once
#include "StaticObject.h"

// 수레(빈) - 정적 오브젝트(비상호작용).
class CWagonEtc : public CStaticObject
{
public:
    CWagonEtc() = default;
    virtual ~CWagonEtc() = default;

public:
    virtual void Initialize() override;
};
