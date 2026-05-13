#include "pch.h"
#include "Monster.h"
#include "Camera.h"
#include "Img_Manager.h"
#include "Input_Manager.h"

void CMonster::Initialize()
{
    m_bDead = false;
    m_fSpeed = 0.f;
    m_iHp = m_iMaxHp;
    m_eDir = DIR_B;
    m_fDestWorldX = m_tIsoInfo.fWorldX;
    m_fDestWorldZ = m_tIsoInfo.fWorldZ;
    m_bMoving = false;
}

int CMonster::Update(float dt)
{
    if (m_bDead) return OBJ_DEAD;

    __super::Update_Rect();
    __super::Move_Frame();
    Update_Cursor();

    return OBJ_NOEVENT;
}

void CMonster::Late_Update(float dt) {}

void CMonster::Render(ID2D1RenderTarget* pRT)
{
    Render_HpBar(pRT);
    Render_NameTag(pRT);
}

void CMonster::Release() {}

void CMonster::Direction_Change(DIRECTION eDir)
{
    m_eDir = eDir;
    m_tFrame.iFrameStart = 0;
}

void CMonster::Render_Sprite(ID2D1RenderTarget* pRT, ID2D1Bitmap* pBitmap)
{
    if (!pBitmap) return;

    POINT tScreen = CCamera::Get_Instance()->IsoWorldToScreen(
        m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ);

    float fWidth = m_tIsoInfo.fCX * m_fScale;
    float fHeight = m_tIsoInfo.fCY * m_fScale;
    float fDrawX = tScreen.x - fWidth / 2.f;
    float fDrawY = tScreen.y - fHeight - m_tIsoInfo.fHeight + TILE_HALF_H;

    float fSrcX = m_tIsoInfo.fCX * m_tFrame.iFrameStart;
    float fSrcY = m_tIsoInfo.fCY * (int)m_eDir;

    pRT->DrawBitmap(pBitmap,
        D2D1::RectF(fDrawX, fDrawY, fDrawX + fWidth, fDrawY + fHeight),
        1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
        D2D1::RectF(fSrcX, fSrcY, fSrcX + m_tIsoInfo.fCX, fSrcY + m_tIsoInfo.fCY));
}

void CMonster::Render_HpBar(ID2D1RenderTarget* pRT)
{
    if (m_iHp <= 0 || m_iHp >= m_iMaxHp) return;

    POINT tScreen = CCamera::Get_Instance()->IsoWorldToScreen(
        m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ);

    float fBarW = 60.f;
    float fBarH = 6.f;
    float fBarX = tScreen.x - fBarW / 2.f;
    float fBarY = tScreen.y - m_tIsoInfo.fCY - m_tIsoInfo.fHeight - 10.f + TILE_HALF_H;
    float fRatio = (float)m_iHp / (float)m_iMaxHp;

    ID2D1SolidColorBrush* pBrush = nullptr;

    pRT->CreateSolidColorBrush(D2D1::ColorF(0.6f, 0.f, 0.f), &pBrush);
    pRT->FillRectangle(
        D2D1::RectF(fBarX, fBarY, fBarX + fBarW, fBarY + fBarH), pBrush);
    pBrush->Release();

    pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.8f, 0.f), &pBrush);
    pRT->FillRectangle(
        D2D1::RectF(fBarX, fBarY, fBarX + fBarW * fRatio, fBarY + fBarH), pBrush);
    pBrush->Release();
}

void CMonster::Render_NameTag(ID2D1RenderTarget* pRT)
{
    if (m_szName[0] == '\0') return;

    POINT tScreen = CCamera::Get_Instance()->IsoWorldToScreen(
        m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ);

    float fNameY = tScreen.y - m_tIsoInfo.fCY
        - m_tIsoInfo.fHeight - 20.f + TILE_HALF_H;

    ID2D1SolidColorBrush* pBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 0.3f, 0.3f), &pBrush);
    pRT->DrawText(m_szName, lstrlen(m_szName),
        CImg_Manager::Get_Instance()->Get_DebugFont(),
        D2D1::RectF(
            (float)tScreen.x - 40.f, fNameY,
            (float)tScreen.x + 40.f, fNameY + 20.f),
        pBrush);
    pBrush->Release();
}

void CMonster::Update_Cursor()
{
    if (!CInput_Manager::Get_Instance()->Is_GameMode()) return;

    POINT tMouse = CInput_Manager::Get_Instance()->Get_MousePos();
    if (Is_MouseCollide(tMouse))
        CInput_Manager::Get_Instance()->Set_CursorMode(m_eHoverCursor);
}

#ifdef GAME_DEBUG
void CMonster::Debug_Render(ID2D1RenderTarget* pRT)
{
    Debug_DrawCollider(pRT);
    Debug_DrawMouseCollider(pRT);
}

void CMonster::Debug_DrawCollider(ID2D1RenderTarget* pRT)
{
    float fCX = Get_ColliderX();
    float fCZ = Get_ColliderZ();
    float fRX = m_tCollider.fRadiusX;
    float fRZ = m_tCollider.fRadiusZ;

    POINT tTL = CCamera::Get_Instance()->IsoWorldToScreen(fCX - fRX, fCZ - fRZ);
    POINT tTR = CCamera::Get_Instance()->IsoWorldToScreen(fCX + fRX, fCZ - fRZ);
    POINT tBR = CCamera::Get_Instance()->IsoWorldToScreen(fCX + fRX, fCZ + fRZ);
    POINT tBL = CCamera::Get_Instance()->IsoWorldToScreen(fCX - fRX, fCZ + fRZ);

    ID2D1SolidColorBrush* pBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 1.f, 1.f), &pBrush);

    auto P = [](POINT p) { return D2D1::Point2F((float)p.x, (float)p.y); };

    pRT->DrawLine(P(tTL), P(tTR), pBrush, 2.f);
    pRT->DrawLine(P(tTR), P(tBR), pBrush, 2.f);
    pRT->DrawLine(P(tBR), P(tBL), pBrush, 2.f);
    pRT->DrawLine(P(tBL), P(tTL), pBrush, 2.f);

    pBrush->Release();
}

void CMonster::Debug_DrawMouseCollider(ID2D1RenderTarget* pRT)
{
    ID2D1SolidColorBrush* pBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 0.f), &pBrush);
    pRT->DrawRectangle(
        D2D1::RectF(
            (float)m_tMouseRect.left,
            (float)m_tMouseRect.top,
            (float)m_tMouseRect.right,
            (float)m_tMouseRect.bottom),
        pBrush, 2.f);
    pBrush->Release();
}

void CMonster::Debug_DrawText(ID2D1RenderTarget* pRT) {}
#endif