#pragma once
#include "GameObject.h"

class CDropItem : public CGameObject
{
public:
    CDropItem() = default;
    virtual ~CDropItem() = default;

    void Init_Drop(int32_t nDropId, int32_t nItemCode, int32_t nAmount,
        float fX, float fZ);

    virtual void Initialize()                       override;
    virtual int  Update(float dt)                   override;
    virtual void Late_Update(float dt)              override;
    virtual void Render(ID2D1RenderTarget* pRT)     override;
    virtual void Release()                          override;

    int32_t Get_DropID()   const { return m_nDropId; }
    int32_t Get_ItemCode() const { return m_nItemCode; }

private:
    void    Render_Hitbox(ID2D1RenderTarget* pRT); 
    void    Render_HoverName(ID2D1RenderTarget* pRT, float fIconX, float fIconY);

private:
    int32_t m_nDropId   = 0;
    int32_t m_nItemCode = 0;
    int32_t m_nAmount   = 0;
    TCHAR   m_szIconKey[64] = {};
    TCHAR   m_szName[64] = {};
};
