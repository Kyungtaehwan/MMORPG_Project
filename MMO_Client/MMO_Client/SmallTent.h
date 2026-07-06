#pragma once
#include "StaticObject.h"

// 작은 텐트(지휘관 텐트) - 정적 오브젝트(비상호작용).
class CSmallTent : public CStaticObject
{
public:
    CSmallTent() = default;
    virtual ~CSmallTent() = default;

public:
    virtual void Initialize() override;
};
