#pragma once
#include "define.h"

// ================================================================
//  CUI_CloseButton  패널 우상단 닫기(X) 버튼 — 경량 헬퍼
//  - CUI_Manager 리스트에 올라가지 않고, 각 UI 패널이 멤버로 보유.
//  - 헤더 온리(별도 .cpp 없음 → vcxproj 등록 불필요).
//  - 사용법:
//      m_closeBtn.Set_ByPanelCorner(m_tRect.right, m_tRect.top);
//      if (m_closeBtn.Update(tMouse, bLeftClick)) Close();
//      m_closeBtn.Render(pRT);
// ================================================================
class CUI_CloseButton
{
public:
    // 패널 우상단 코너 기준 배치
    void Set_ByPanelCorner(float fPanelRight, float fPanelTop,
        float fSize = 24.f, float fMargin = 8.f)
    {
        m_fSize = fSize;
        m_fX = fPanelRight - fSize - fMargin;
        m_fY = fPanelTop + fMargin;
    }

    // 마우스 위치/좌클릭으로 갱신. 버튼이 클릭되면 true.
    bool Update(POINT tMouse, bool bLeftClick)
    {
        RECT r = Get_Rect();
        m_bHover = PtInRect(&r, tMouse) ? true : false;
        return (m_bHover && bLeftClick);
    }

    void Render(ID2D1RenderTarget* pRT)
    {
        RECT r = Get_Rect();
        ID2D1SolidColorBrush* pBrush = nullptr;

        // 배경 (호버 시 밝은 빨강)
        D2D1::ColorF bg = m_bHover
            ? D2D1::ColorF(0.85f, 0.2f, 0.2f)
            : D2D1::ColorF(0.55f, 0.13f, 0.13f);
        pRT->CreateSolidColorBrush(bg, &pBrush);
        pRT->FillRoundedRectangle(
            D2D1::RoundedRect(
                D2D1::RectF((float)r.left, (float)r.top, (float)r.right, (float)r.bottom),
                4.f, 4.f),
            pBrush);
        pBrush->Release();

        // X 표시
        pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f), &pBrush);
        float pad = m_fSize * 0.3f;
        pRT->DrawLine(
            D2D1::Point2F(r.left + pad, r.top + pad),
            D2D1::Point2F(r.right - pad, r.bottom - pad), pBrush, 2.f);
        pRT->DrawLine(
            D2D1::Point2F(r.right - pad, r.top + pad),
            D2D1::Point2F(r.left + pad, r.bottom - pad), pBrush, 2.f);
        pBrush->Release();
    }

private:
    RECT Get_Rect() const
    {
        return RECT{ (LONG)m_fX, (LONG)m_fY,
                     (LONG)(m_fX + m_fSize), (LONG)(m_fY + m_fSize) };
    }

private:
    float m_fX = 0.f;
    float m_fY = 0.f;
    float m_fSize = 24.f;
    bool  m_bHover = false;
};
