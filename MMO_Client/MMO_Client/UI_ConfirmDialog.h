#pragma once
#include "define.h"
#include "Img_Manager.h"
#include "Input_Manager.h"

// ================================================================
//  CUI_ConfirmDialog  예/아니오 확인  — 재사용 위젯(헤더 온리)
//   - 제목 + 메시지(여러 줄 가능) + [확인]/[취소].
//   - 호스트가 Open→Update(결과 확인)→Render. 매니저 리스트 밖(멤버 보유).
// ================================================================
class CUI_ConfirmDialog
{
public:
    enum RESULT { RESULT_NONE, RESULT_CONFIRM, RESULT_CANCEL };

    void Open(const TCHAR* szTitle, const TCHAR* szMsg)
    {
        lstrcpyn(m_szTitle, szTitle ? szTitle : L"", 48);
        lstrcpyn(m_szMsg, szMsg ? szMsg : L"", 256);
        m_bOpen = true;
    }
    void Close() { m_bOpen = false; }
    bool Is_Open() const { return m_bOpen; }

    int Update()
    {
        if (!m_bOpen) return RESULT_NONE;
        CInput_Manager* in = CInput_Manager::Get_Instance();
        in->Set_CursorMode(CURSOR_UI);

        POINT m = in->Get_MousePos();
        bool  bClick = in->Key_Down(VK_LBUTTON);

        if (in->Key_Down(VK_ESCAPE)) { m_bOpen = false; return RESULT_CANCEL; }
        if (in->Key_Down(VK_RETURN)) { m_bOpen = false; return RESULT_CONFIRM; }

        if (bClick)
        {
            RECT rc = Rc_Confirm(); if (PtInRect(&rc, m)) { m_bOpen = false; return RESULT_CONFIRM; }
            rc = Rc_Cancel();       if (PtInRect(&rc, m)) { m_bOpen = false; return RESULT_CANCEL; }
        }
        return RESULT_NONE;
    }

    void Render(ID2D1RenderTarget* pRT)
    {
        if (!m_bOpen) return;
        CImg_Manager* pImg = CImg_Manager::Get_Instance();
        POINT m = CInput_Manager::Get_Instance()->Get_MousePos();
        ID2D1SolidColorBrush* b = nullptr;

        // 모달 배경 어둡게
        pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.45f), &b);
        pRT->FillRectangle(D2D1::RectF(0.f, 0.f, (float)WINCX, (float)WINCY), b); b->Release();

        RECT box = Box();
        D2D1_RECT_F rb = D2D1::RectF((float)box.left, (float)box.top, (float)box.right, (float)box.bottom);
        pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.93f), &b);
        pRT->FillRoundedRectangle(D2D1::RoundedRect(rb, 10, 10), b); b->Release();
        pRT->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.55f, 0.6f, 0.9f), &b);
        pRT->DrawRoundedRectangle(D2D1::RoundedRect(rb, 10, 10), b, 1.5f); b->Release();

        // 제목
        pImg->Draw_Text_Center(pRT, m_szTitle,
            D2D1::RectF((float)box.left, (float)box.top + 14.f, (float)box.right, (float)box.top + 40.f),
            D2D1::ColorF(1.f, 0.95f, 0.6f));

        // 메시지 (여러 줄)
        ID2D1SolidColorBrush* pt = nullptr;
        pRT->CreateSolidColorBrush(D2D1::ColorF(0.9f, 0.92f, 0.95f), &pt);
        pRT->DrawText(m_szMsg, (UINT32)lstrlen(m_szMsg), pImg->Get_DebugFont(),
            D2D1::RectF((float)box.left + 22.f, (float)box.top + 50.f,
                (float)box.right - 22.f, (float)box.bottom - 52.f), pt);
        pt->Release();

        Draw_Btn(pRT, Rc_Confirm(), L"확인", m, true);
        Draw_Btn(pRT, Rc_Cancel(), L"취소", m, false);
    }

private:
    RECT Box() const
    {
        float l = WINCX / 2.f - BOX_W / 2.f;
        float t = WINCY / 2.f - BOX_H / 2.f;
        return RECT{ (LONG)l, (LONG)t, (LONG)(l + BOX_W), (LONG)(t + BOX_H) };
    }
    RECT Rc_Confirm() const
    {
        RECT b = Box(); LONG y = b.bottom - 44;
        return RECT{ b.left + 44, y, b.left + 168, y + 32 };
    }
    RECT Rc_Cancel() const
    {
        RECT b = Box(); LONG y = b.bottom - 44;
        return RECT{ b.right - 168, y, b.right - 44, y + 32 };
    }
    void Draw_Btn(ID2D1RenderTarget* pRT, const RECT& r, const TCHAR* label, POINT m, bool bAccent)
    {
        ID2D1SolidColorBrush* b = nullptr;
        bool hover = PtInRect(&r, m) ? true : false;
        D2D1_RECT_F rc = D2D1::RectF((float)r.left, (float)r.top, (float)r.right, (float)r.bottom);
        D2D1::ColorF bg = bAccent
            ? (hover ? D2D1::ColorF(0.20f, 0.55f, 0.25f) : D2D1::ColorF(0.14f, 0.42f, 0.18f))
            : (hover ? D2D1::ColorF(0.40f, 0.40f, 0.45f) : D2D1::ColorF(0.24f, 0.24f, 0.28f));
        pRT->CreateSolidColorBrush(bg, &b);
        pRT->FillRoundedRectangle(D2D1::RoundedRect(rc, 6, 6), b); b->Release();
        CImg_Manager::Get_Instance()->Draw_Text_Center(pRT, label, rc, D2D1::ColorF(1.f, 1.f, 1.f));
    }

private:
    bool  m_bOpen = false;
    TCHAR m_szTitle[48] = {};
    TCHAR m_szMsg[256] = {};
    static constexpr float BOX_W = 380.f;
    static constexpr float BOX_H = 176.f;
};
