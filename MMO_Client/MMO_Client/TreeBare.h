#pragma once
#include "StaticObject.h"

// 나무(고목) - 정적 오브젝트(비상호작용).
class CTreeBare : public CStaticObject
{
public:
    CTreeBare() = default;
    virtual ~CTreeBare() = default;

public:
    virtual void Initialize() override;
};
