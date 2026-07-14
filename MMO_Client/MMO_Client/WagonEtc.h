#pragma once
#include "StaticObject.h"


class CWagonEtc : public CStaticObject
{
public:
    CWagonEtc() = default;
    virtual ~CWagonEtc() = default;

public:
    virtual void Initialize() override;
};
