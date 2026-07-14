#pragma once
#include "StaticObject.h"


class CWagonFood : public CStaticObject
{
public:
    CWagonFood() = default;
    virtual ~CWagonFood() = default;

public:
    virtual void Initialize() override;
};
