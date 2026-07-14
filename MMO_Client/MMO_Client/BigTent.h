#pragma once
#include "StaticObject.h"

class CBigTent : public CStaticObject
{
public:
    CBigTent() = default;
    virtual ~CBigTent() = default;

public:
    virtual void Initialize() override;
};
