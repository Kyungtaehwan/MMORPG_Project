#pragma once
#include "StaticObject.h"

// 큰 텐트(병사 텐트) - 정적 오브젝트(비상호작용).
class CBigTent : public CStaticObject
{
public:
    CBigTent() = default;
    virtual ~CBigTent() = default;

public:
    virtual void Initialize() override;
};
