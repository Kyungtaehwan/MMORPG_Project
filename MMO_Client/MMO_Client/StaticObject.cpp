#include "pch.h"
#include "StaticObject.h"
#include "Img_Manager.h"
#include "Camera.h"

void CStaticObject::Set_Sprite(const TCHAR* key, float cx, float cy,
    float fHeight, float fSortOff)
{
    m_pImgKey = key;
    m_tIsoInfo.fCX = cx;
    m_tIsoInfo.fCY = cy;
    m_fHeightOffset = fHeight;
    m_fScale = 1.0f;
    Set_SortOffset(fSortOff);
}

void CStaticObject::Initialize()
{
    m_bDead = false;
    m_fSpeed = 0.f;
}

int CStaticObject::Update(float dt)
{
    return OBJ_NOEVENT;
}

void CStaticObject::Late_Update(float dt) {}

void CStaticObject::Render(ID2D1RenderTarget* pRT)
{
    ID2D1Bitmap* pBmp = CImg_Manager::Get_Instance()->Find_Png(m_pImgKey);
    if (!pBmp) return;

    POINT tScreen = CCamera::Get_Instance()->IsoWorldToScreen(
        m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ);

    float fW = m_tIsoInfo.fCX * m_fScale;
    float fH = m_tIsoInfo.fCY * m_fScale;

    // 스프라이트 바닥-중앙을 타일 중심에 맞춤 (NPC/몬스터와 동일 규칙)
    float fDrawX = tScreen.x - fW / 2.f;
    float fDrawY = tScreen.y - fH - m_fHeightOffset + TILE_HALF_H;

    pRT->DrawBitmap(pBmp,
        D2D1::RectF(fDrawX, fDrawY, fDrawX + fW, fDrawY + fH),
        1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

void CStaticObject::Release() {}
