#pragma once
#include "StaticObject.h"

class CTreeBare : public CStaticObject
{
public:
    CTreeBare() = default;
    virtual ~CTreeBare() = default;

public:
    virtual void Initialize() override;
};
