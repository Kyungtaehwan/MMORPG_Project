#include "pch.h"
#include "UI_QtyDialog.h"
#include "Img_Manager.h"
#include "Input_Manager.h"

// ===================== 레이아웃 =====================

RECT CUI_QtyDialog::Box() const
{
    float l = WINCX / 2.f - BOX_W / 2.f;
    float t = WINCY / 2.f - BOX_H / 2.f;
    return RECT{ (LONG)l, (LONG)t, (LONG)(l + BOX_W), (LONG)(t + BOX_H) };
}

RECT CUI_QtyDialog::Rc_Icon() const
{
    RECT b = Box();
    return RECT{ b.left + 28, b.top + 52, b.left + 88, b.top + 112 };
}

// 수량 조절 행 (스택 아이템만)
RECT CUI_QtyDialog::Rc_Minus() const
{
    RECT b = Box();  LONG y = b.top + 118;
    return RECT{ b.left + 40, y, b.left + 80, y + 36 };
}
RECT CUI_QtyDialog::Rc_QtyBox() const
{
    RECT b = Box();  LONG y = b.top + 118;
    return RECT{ b.left + 86, y, b.left + 156, y + 36 };
}
RECT CUI_QtyDialog::Rc_Plus() const
{
    RECT b = Box();  LONG y = b.top + 118;
    return RECT{ b.left + 162, y, b.left + 202, y + 36 };
}
RECT CUI_QtyDialog::Rc_All() const
{
    RECT b = Box();  LONG y = b.top + 118;
    return RECT{ b.left + 210, y, b.left + 280, y + 36 };
}

// 확인/취소
RECT CUI_QtyDialog::Rc_Confirm() const
{
    RECT b = Box();  LONG y = b.bottom - 42;
    return RECT{ b.left + 44, y, b.left + 154, y + 30 };
}
RECT CUI_QtyDialog::Rc_Cancel() const
{
    RECT b = Box();  LONG y = b.bottom - 42;
    return RECT{ b.left + 166, y, b.left + 276, y + 30 };
}

// ===================== 상태 =====================

void CUI_QtyDialog::Open(const TCHAR* szAction, const TCHAR* szIconKey,
    const TCHAR* szName, int iMaxQty)
{
    lstrcpyn(m_szAction, szAction ? szAction : L"", 32);
    lstrcpyn(m_szIconKey, szIconKey ? szIconKey : L"", 64);
    lstrcpyn(m_szName, szName ? szName : L"", 64);

    m_iMax = (iMaxQty < 1) ? 1 : iMaxQty;
    m_bStackable = (m_iMax > 1);
    m_iQty = 1;
    m_bTyped = false;
    m_bOpen = true;
}

void CUI_QtyDialog::Set_Qty(int v)
{
    if (v < 1) v = 1;
    if (v > m_iMax) v = m_iMax;
    m_iQty = v;
}

// ===================== 입력 =====================

int CUI_QtyDialog::Update()
{
    if (!m_bOpen) return RESULT_NONE;

    CInput_Manager* in = CInput_Manager::Get_Instance();
    in->Set_CursorMode(CURSOR_UI);

    POINT tMouse = in->Get_MousePos();
    bool  bClick = in->Key_Down(VK_LBUTTON);

    // 키보드 확정/취소
    if (in->Key_Down(VK_ESCAPE)) { m_bOpen = false; return RESULT_CANCEL; }
    if (in->Key_Down(VK_RETURN)) { m_bOpen = false; return RESULT_CONFIRM; }

    // 버튼 클릭
    if (bClick)
    {
        RECT rc;
        rc = Rc_Confirm(); if (PtInRect(&rc, tMouse)) { m_bOpen = false; return RESULT_CONFIRM; }
        rc = Rc_Cancel();  if (PtInRect(&rc, tMouse)) { m_bOpen = false; return RESULT_CANCEL; }

        if (m_bStackable)
        {
            rc = Rc_Minus(); if (PtInRect(&rc, tMouse)) { Set_Qty(m_iQty - 1); m_bTyped = false; }
            rc = Rc_Plus();  if (PtInRect(&rc, tMouse)) { Set_Qty(m_iQty + 1); m_bTyped = false; }
            rc = Rc_All();   if (PtInRect(&rc, tMouse)) { Set_Qty(m_iMax);     m_bTyped = false; }
        }
    }

    // 숫자키 입력 (상단 숫자 + 넘패드)
    if (m_bStackable)
    {
        for (int d = 0; d <= 9; ++d)
        {
            if (in->Key_Down('0' + d) || in->Key_Down(VK_NUMPAD0 + d))
            {
                int v = m_bTyped ? (m_iQty * 10 + d) : d;
                m_bTyped = true;
                Set_Qty(v);
            }
        }
        if (in->Key_Down(VK_BACK)) { Set_Qty(m_iQty / 10); }
    }

    return RESULT_NONE;
}

// ===================== 렌더 =====================

void CUI_QtyDialog::Draw_Button(ID2D1RenderTarget* pRT, const RECT& r,
    const TCHAR* szLabel, POINT tMouse, bool bAccent)
{
    ID2D1SolidColorBrush* pBrush = nullptr;

    bool bHover = PtInRect(&r, tMouse) ? true : false;
    D2D1_RECT_F rc = D2D1::RectF((float)r.left, (float)r.top, (float)r.right, (float)r.bottom);

    // 배경
    D2D1::ColorF bg = bAccent
        ? (bHover ? D2D1::ColorF(0.20f, 0.55f, 0.25f) : D2D1::ColorF(0.14f, 0.42f, 0.18f))
        : (bHover ? D2D1::ColorF(0.40f, 0.40f, 0.45f) : D2D1::ColorF(0.24f, 0.24f, 0.28f));
    pRT->CreateSolidColorBrush(bg, &pBrush);
    pRT->FillRoundedRectangle(D2D1::RoundedRect(rc, 6.f, 6.f), pBrush);
    pBrush->Release();

    // 라벨(rect 정중앙)
    CImg_Manager::Get_Instance()->Draw_Text_Center(pRT, szLabel, rc,
        D2D1::ColorF(1.f, 1.f, 1.f));
}

void CUI_QtyDialog::Render(ID2D1RenderTarget* pRT)
{
    if (!m_bOpen) return;

    CImg_Manager* pImg = CImg_Manager::Get_Instance();
    IDWriteTextFormat* pFont = pImg->Get_DebugFont();
    ID2D1SolidColorBrush* pBrush = nullptr;

    POINT tMouse = CInput_Manager::Get_Instance()->Get_MousePos();

    // 화면 전체 어둡게(모달)
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.45f), &pBrush);
    pRT->FillRectangle(D2D1::RectF(0.f, 0.f, (float)WINCX, (float)WINCY), pBrush);
    pBrush->Release();

    RECT b = Box();
    D2D1_RECT_F rcBox = D2D1::RectF((float)b.left, (float)b.top, (float)b.right, (float)b.bottom);

    // 박스 배경 + 테두리
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.92f), &pBrush);
    pRT->FillRoundedRectangle(D2D1::RoundedRect(rcBox, 10.f, 10.f), pBrush);
    pBrush->Release();
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.55f, 0.6f, 0.9f), &pBrush);
    pRT->DrawRoundedRectangle(D2D1::RoundedRect(rcBox, 10.f, 10.f), pBrush, 1.5f);
    pBrush->Release();

    // 제목 ("판매" / "구매") — 박스 상단 중앙
    pImg->Draw_Text_Center(pRT, m_szAction,
        D2D1::RectF((float)b.left, (float)b.top + 12.f, (float)b.right, (float)b.top + 40.f),
        D2D1::ColorF(1.f, 0.95f, 0.6f));

    // 아이콘
    RECT ri = Rc_Icon();
    ID2D1Bitmap* pIcon = pImg->Find_Png(m_szIconKey);
    if (pIcon)
        pRT->DrawBitmap(pIcon,
            D2D1::RectF((float)ri.left, (float)ri.top, (float)ri.right, (float)ri.bottom),
            1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

    // 이름
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f), &pBrush);
    pRT->DrawText(m_szName, (UINT32)wcslen(m_szName), pFont,
        D2D1::RectF((float)ri.right + 12.f, (float)ri.top + 6.f, (float)b.right - 16.f, (float)ri.top + 30.f),
        pBrush);
    pBrush->Release();

    if (m_bStackable)
    {
        // 안내 문구
        TCHAR szMsg[64];
        swprintf_s(szMsg, 64, L"몇 개를 %s하시겠습니까?", m_szAction);
        pRT->CreateSolidColorBrush(D2D1::ColorF(0.8f, 0.85f, 0.9f), &pBrush);
        pRT->DrawText(szMsg, (UINT32)wcslen(szMsg), pFont,
            D2D1::RectF((float)ri.right + 12.f, (float)ri.top + 34.f, (float)b.right - 16.f, (float)ri.top + 56.f),
            pBrush);
        pBrush->Release();

        // 수량 조절 버튼 + 수량 박스
        Draw_Button(pRT, Rc_Minus(), L"-", tMouse, false);
        Draw_Button(pRT, Rc_Plus(), L"+", tMouse, false);
        Draw_Button(pRT, Rc_All(), L"전체", tMouse, false);

        RECT rq = Rc_QtyBox();
        D2D1_RECT_F rcq = D2D1::RectF((float)rq.left, (float)rq.top, (float)rq.right, (float)rq.bottom);
        pRT->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.15f), &pBrush);
        pRT->FillRoundedRectangle(D2D1::RoundedRect(rcq, 5.f, 5.f), pBrush);
        pBrush->Release();

        TCHAR szQty[16];
        swprintf_s(szQty, 16, L"%d", m_iQty);
        pImg->Draw_Text_Center(pRT, szQty, rcq, D2D1::ColorF(1.f, 1.f, 1.f));
    }
    else
    {
        // 스택 불가: 단순 확인
        TCHAR szMsg[64];
        swprintf_s(szMsg, 64, L"%s하시겠습니까?", m_szAction);
        pRT->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.9f, 0.95f), &pBrush);
        pRT->DrawText(szMsg, (UINT32)wcslen(szMsg), pFont,
            D2D1::RectF((float)ri.right + 12.f, (float)ri.top + 34.f, (float)b.right - 16.f, (float)ri.top + 58.f),
            pBrush);
        pBrush->Release();
    }

    // 확인 / 취소
    Draw_Button(pRT, Rc_Confirm(), L"확인", tMouse, true);
    Draw_Button(pRT, Rc_Cancel(), L"취소", tMouse, false);
}
