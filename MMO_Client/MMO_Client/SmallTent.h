#pragma once
#include "StaticObject.h"

class CSmallTent : public CStaticObject
{
public:
    CSmallTent() = default;
    virtual ~CSmallTent() = default;

public:
    virtual void Initialize() override;
};
