#pragma once
#include "StaticObject.h"

class CSmithy : public CStaticObject
{
public:
    CSmithy() = default;
    virtual ~CSmithy() = default;

public:
    virtual void Initialize() override;
};
