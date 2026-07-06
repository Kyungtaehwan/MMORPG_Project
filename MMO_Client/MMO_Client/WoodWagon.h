#pragma once
#include "StaticObject.h"

// 수레(장작) - 정적 오브젝트(비상호작용).
class CWoodWagon : public CStaticObject
{
public:
    CWoodWagon() = default;
    virtual ~CWoodWagon() = default;

public:
    virtual void Initialize() override;
};
