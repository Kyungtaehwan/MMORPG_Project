#pragma once
#include "StaticObject.h"

// 대장간 - 정적 오브젝트(비상호작용).
class CSmithy : public CStaticObject
{
public:
    CSmithy() = default;
    virtual ~CSmithy() = default;

public:
    virtual void Initialize() override;
};
