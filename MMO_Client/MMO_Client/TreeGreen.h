#pragma once
#include "StaticObject.h"

// 나무(잎) - 정적 오브젝트(비상호작용).
class CTreeGreen : public CStaticObject
{
public:
    CTreeGreen() = default;
    virtual ~CTreeGreen() = default;

public:
    virtual void Initialize() override;
};
