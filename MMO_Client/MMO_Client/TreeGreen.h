#pragma once
#include "StaticObject.h"

class CTreeGreen : public CStaticObject
{
public:
    CTreeGreen() = default;
    virtual ~CTreeGreen() = default;

public:
    virtual void Initialize() override;
};
