#include "pch.h"
#include "UI_HUD.h"
#include "Player.h"
#include "Img_Manager.h"

void CUI_HUD::Initialize()
{
    CImg_Manager::Get_Instance()->Insert_Png(
        L"../Resource/UI/FrameFinal.png", L"HUD_FRAME");
    CImg_Manager::Get_Instance()->Insert_Png(
        L"../Resource/UI/Hp_Bar.png", L"HUD_HP");
    CImg_Manager::Get_Instance()->Insert_Png(
        L"../Resource/UI/Mp_Bar.png", L"HUD_MP");
    CImg_Manager::Get_Instance()->Insert_Png(
        L"../Resource/UI/Exp_Bar.png", L"HUD_EXP");

    // 좌측 하단 고정
    Set_Pos(PANEL_W * 0.5f, WINCY - PANEL_H * 0.5f);
    Set_Size(PANEL_W, PANEL_H);
    Update_Rect();
}

int CUI_HUD::Update(float dt) { return UI_NOEVENT; }
void CUI_HUD::Late_Update(float dt) {}
void CUI_HUD::Release() {}

void CUI_HUD::Render(ID2D1RenderTarget* pRT)
{
    if (!m_pPlayer) return;

    float fL = (float)m_tRect.left;
    float fT = (float)m_tRect.top;


    ID2D1Bitmap* pFrame = CImg_Manager::Get_Instance()->Find_Png(L"HUD_FRAME");
    if (pFrame)
        pRT->DrawBitmap(pFrame,
            D2D1::RectF(fL, fT, fL + PANEL_W, fT + PANEL_H),
            1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

    // EXP 바
    float fExpRatio = (m_pPlayer->Get_MaxExp() > 0)
        ? (float)m_pPlayer->Get_Exp() / m_pPlayer->Get_MaxExp() : 0.f;
    Render_Bar(pRT, L"HUD_EXP", fL + EXP_X, fT + EXP_Y, BAR_W, BAR_H, fExpRatio);

    // HP 바
    float fHpRatio = (m_pPlayer->Get_MaxHP() > 0)
        ? (float)m_pPlayer->Get_HP() / m_pPlayer->Get_MaxHP() : 0.f;
    Render_Bar(pRT, L"HUD_HP", fL + HP_X, fT + HP_Y, BAR_W, BAR_H, fHpRatio);

    // MP 바
    float fMpRatio = (m_pPlayer->Get_MaxMP() > 0)
        ? (float)m_pPlayer->Get_MP() / m_pPlayer->Get_MaxMP() : 0.f;
    Render_Bar(pRT, L"HUD_MP", fL + MP_X, fT + MP_Y, BAR_W, BAR_H, fMpRatio);

    // 버프 박스 (HP UI 위쪽)
    Render_Buffs(pRT, fL + 20.f, fT - 52.f);
}

void CUI_HUD::Render_Buffs(ID2D1RenderTarget* pRT, float fBaseX, float fBaseY)
{
    if (!m_pPlayer) return;

    const CPlayer::FBuff* pBuffs = m_pPlayer->Get_Buffs();
    int nMax = m_pPlayer->Get_MaxBuffs();
    DWORD now = (DWORD)GetTickCount64();

    const float fBox = 42.f;
    const float fGap = 6.f;
    int nDrawn = 0;

    for (int i = 0; i < nMax; ++i)
    {
        if (pBuffs[i].type < 0) continue;
        DWORD dwEnd = pBuffs[i].start + pBuffs[i].duration;
        if (now >= dwEnd) continue;   // 만료된 버프는 표시 안 함

        float fX = fBaseX + nDrawn * (fBox + fGap);
        float fY = fBaseY;

        // 1) 버프 아이콘 (버프포션 아이콘)
        const TCHAR* szKey = (pBuffs[i].type == 0) ? L"Potion_Atk" : L"Potion_Invincible";
        ID2D1Bitmap* pBmp = CImg_Manager::Get_Instance()->Find_Png(szKey);
        if (pBmp)
            pRT->DrawBitmap(pBmp, D2D1::RectF(fX, fY, fX + fBox, fY + fBox),
                1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

        ID2D1SolidColorBrush* pBrush = nullptr;

        // 2) 경과 비율만큼 위에서 검은색으로 덮기 (쿨타임 표시)
        float fRatio = (pBuffs[i].duration > 0)
            ? (float)(now - pBuffs[i].start) / (float)pBuffs[i].duration : 1.f;
        if (fRatio < 0.f) fRatio = 0.f;
        if (fRatio > 1.f) fRatio = 1.f;

        pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.6f), &pBrush);
        if (pBrush)
        {
            pRT->FillRectangle(
                D2D1::RectF(fX, fY, fX + fBox, fY + fBox * fRatio), pBrush);
            pBrush->Release();
        }

        // 3) 테두리
        pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f, 0.85f), &pBrush);
        if (pBrush)
        {
            pRT->DrawRectangle(D2D1::RectF(fX, fY, fX + fBox, fY + fBox), pBrush, 1.5f);
            pBrush->Release();
        }

        // 4) 남은 초
        int nRemain = (int)((dwEnd - now) / 1000) + 1;
        TCHAR szSec[8];
        swprintf_s(szSec, 8, L"%d", nRemain);
        pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 0.4f), &pBrush);
        if (pBrush)
        {
            pRT->DrawText(szSec, lstrlen(szSec),
                CImg_Manager::Get_Instance()->Get_DebugFont(),
                D2D1::RectF(fX + 2.f, fY + fBox - 18.f, fX + fBox, fY + fBox), pBrush);
            pBrush->Release();
        }

        ++nDrawn;
    }
}

void CUI_HUD::Render_Bar(ID2D1RenderTarget* pRT, const TCHAR* szKey,
    float fX, float fY, float fW, float fH, float fRatio)
{
    ID2D1Bitmap* pBitmap = CImg_Manager::Get_Instance()->Find_Png(szKey);
    if (!pBitmap) return;

    fRatio = max(0.f, min(1.f, fRatio));

    // 바를 비율만큼 잘라서 렌더
    float fBitmapW = pBitmap->GetSize().width;
    float fBitmapH = pBitmap->GetSize().height;

    pRT->DrawBitmap(pBitmap,
        D2D1::RectF(fX, fY, fX + fW * fRatio, fY + fH),
        1.0f,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
        D2D1::RectF(0.f, 0.f, fBitmapW * fRatio, fBitmapH));
}