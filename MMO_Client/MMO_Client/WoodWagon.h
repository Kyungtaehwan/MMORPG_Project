#pragma once
#include "StaticObject.h"

class CWoodWagon : public CStaticObject
{
public:
    CWoodWagon() = default;
    virtual ~CWoodWagon() = default;

public:
    virtual void Initialize() override;
};
