#include "pch.h"
#include "UI_LoginBox.h"
#include "Img_Manager.h"
#include "Input_Manager.h"
#include "Network_Manager.h"

// 로그인 입력은 ASCII(계정 id/pw) → wide 문자열을 char 버퍼로 단순 복사
static void WideToNarrow(const std::wstring& src, char* out, int cap)
{
    int i = 0;
    for (; i < (int)src.size() && i < cap - 1; ++i)
        out[i] = (char)src[i];
    out[i] = '\0';
}

CUI_LoginBox::CUI_LoginBox()
{
}

CUI_LoginBox::~CUI_LoginBox()
{
}

void CUI_LoginBox::Initialize()
{
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/UI/LoginBox.png", L"LOGIN_BOX");
	Set_Pos(WINCX/2.f, WINCY/2.f + 150.f);
	Set_Size(PANEL_W, PANEL_H);
	Update_Rect();
}

int CUI_LoginBox::Update(float dt)
{
    m_fCaretTimer += dt;
    if (m_fCaretTimer >= CARET_BLINK)
    {
        m_fCaretTimer -= CARET_BLINK;
        m_bCaretVis = !m_bCaretVis;
    }

    auto inp = CInput_Manager::Get_Instance();
    POINT mp = inp->Get_MousePos();

    float fL = (float)m_tRect.left;
    float fT = (float)m_tRect.top;

    // ── 마우스 클릭으로 포커스 이동 ───────────────────────
    if (inp->Mouse_Down(MBUTTON_L))
    {
        // ID 칸 클릭?
        if (mp.x >= fL + FIELD_X && mp.x <= fL + FIELD_X + FIELD_W &&
            mp.y >= fT + ID_Y && mp.y <= fT + ID_Y + FIELD_H)
            m_iFocus = 0;

        // PW 칸 클릭?
        else if (mp.x >= fL + FIELD_X && mp.x <= fL + FIELD_X + FIELD_W &&
            mp.y >= fT + PW_Y && mp.y <= fT + PW_Y + FIELD_H)
            m_iFocus = 1;
    }

    // ── Tab으로 포커스 전환 ───────────────────────────────
    if (inp->Key_Down(VK_TAB))
        m_iFocus = (m_iFocus + 1) % 2;

    // ── Backspace ─────────────────────────────────────────
    if (inp->Key_Down(VK_BACK))
    {
        std::wstring& str = (m_iFocus == 0) ? m_strID : m_strPW;
        if (!str.empty()) str.pop_back();
    }

    // ── Enter → 로그인 요청 (서버가 AccountDB로 인증) ──
    if (inp->Key_Down(VK_RETURN))
    {
        if (!m_strID.empty() && !m_strPW.empty())
        {
            char szID[20] = {}, szPW[20] = {};
            WideToNarrow(m_strID, szID, 20);
            WideToNarrow(m_strPW, szPW, 20);
            CNetwork_Manager::Get_Instance()->SendLogin(szID, szPW);
            // 성공하면 서버 SC_ENTER_GAME 처리에서 LEVEL_TEST로 전환됨.
            // 실패하면 SC_LOGIN_FAIL → Is_LoginFailed()가 true (Render에서 표시).
        }
    }

    return UI_NOEVENT;

}

void CUI_LoginBox::Late_Update(float dt)
{
}

void CUI_LoginBox::Render(ID2D1RenderTarget* pRT)
{

    if (!m_pBrushText)
    {
        pRT->CreateSolidColorBrush(
            D2D1::ColorF(0.9f, 0.9f, 0.9f), &m_pBrushText);
        pRT->CreateSolidColorBrush(
            D2D1::ColorF(0.f, 0.f, 0.f, 0.45f), &m_pBrushFocus);   // 반투명 검정으로 변경
        pRT->CreateSolidColorBrush(
            D2D1::ColorF(0.f, 0.f, 0.f, 0.45f), &m_pBrushNormal);  // 동일하게 맞춤
        pRT->CreateSolidColorBrush(
            D2D1::ColorF(0.6f, 0.6f, 0.6f, 0.7f), &m_pBrushHint);  // 흐린 회색
    }
    if (!m_pDWFactory)
    {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&m_pDWFactory));

        m_pDWFactory->CreateTextFormat(
            L"맑은 고딕", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            15.f, L"ko-KR", &m_pFmtInput);

        m_pFmtInput->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_pFmtInput->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    float fL = (float)m_tRect.left;
    float fT = (float)m_tRect.top;

    // ── 배경 이미지 ──────────────────────────────────────
    ID2D1Bitmap* pFrame = CImg_Manager::Get_Instance()->Find_Png(L"LOGIN_BOX");
    if (pFrame)
        pRT->DrawBitmap(pFrame,
            D2D1::RectF(fL, fT, fL + PANEL_W, fT + PANEL_H),
            0.8f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

    // ── ID 입력 칸 ───────────────────────────────────────
    {
        D2D1_RECT_F rc = D2D1::RectF(
            fL + FIELD_X, fT + ID_Y,
            fL + FIELD_X + FIELD_W, fT + ID_Y + FIELD_H);

        pRT->FillRectangle(rc, m_pBrushNormal);

        D2D1_RECT_F rcText = D2D1::RectF(rc.left + 8.f, rc.top, rc.right - 4.f, rc.bottom);

        // 비어있고 포커스 없으면 힌트 텍스트
        if (m_strID.empty() && m_iFocus != 0)
        {
            const wchar_t* hint = L"ID";
            pRT->DrawText(hint, (UINT32)wcslen(hint),
                m_pFmtInput, rcText, m_pBrushHint);
        }
        else
        {
            std::wstring display = m_strID;
            if (m_iFocus == 0 && m_bCaretVis) display += L'|';
            pRT->DrawText(display.c_str(), (UINT32)display.size(),
                m_pFmtInput, rcText, m_pBrushText);
        }
    }

    // PW 칸 - 동일하게
    {
        D2D1_RECT_F rc = D2D1::RectF(
            fL + FIELD_X, fT + PW_Y,
            fL + FIELD_X + FIELD_W, fT + PW_Y + FIELD_H);

        pRT->FillRectangle(rc, m_pBrushNormal);

        D2D1_RECT_F rcText = D2D1::RectF(rc.left + 8.f, rc.top, rc.right - 4.f, rc.bottom);

        // 비어있고 포커스 없으면 힌트 텍스트
        if (m_strPW.empty() && m_iFocus != 1)
        {
            const wchar_t* hint = L"Password";
            pRT->DrawText(hint, (UINT32)wcslen(hint),
                m_pFmtInput, rcText, m_pBrushHint);
        }
        else
        {
            std::wstring display(m_strPW.size(), L'*');
            if (m_iFocus == 1 && m_bCaretVis) display += L'|';
            pRT->DrawText(display.c_str(), (UINT32)display.size(),
                m_pFmtInput, rcText, m_pBrushText);
        }
    }

    // 안내 / 로그인 실패 메시지
    {
        D2D1_RECT_F rcMsg = D2D1::RectF(
            fL + FIELD_X, fT + PW_Y + FIELD_H + 8.f,
            fL + FIELD_X + FIELD_W, fT + PW_Y + FIELD_H + 30.f);

        if (CNetwork_Manager::Get_Instance()->Is_LoginFailed())
        {
            ID2D1SolidColorBrush* pFail = nullptr;
            pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 0.3f, 0.3f), &pFail);
            const wchar_t* msg = L"로그인 실패 - 아이디/비밀번호 확인";
            pRT->DrawText(msg, (UINT32)wcslen(msg), m_pFmtInput, rcMsg, pFail);
            pFail->Release();
        }
        else
        {
            const wchar_t* msg = L"Enter: 로그인 / Tab: 칸 이동";
            pRT->DrawText(msg, (UINT32)wcslen(msg), m_pFmtInput, rcMsg, m_pBrushHint);
        }
    }
}

void CUI_LoginBox::Release(void)
{
    if (m_pBrushText) { m_pBrushText->Release();   m_pBrushText = nullptr; }
    if (m_pBrushFocus) { m_pBrushFocus->Release();  m_pBrushFocus = nullptr; }
    if (m_pBrushNormal) { m_pBrushNormal->Release();  m_pBrushNormal = nullptr; }
    if (m_pFmtInput) { m_pFmtInput->Release();    m_pFmtInput = nullptr; }
    if (m_pDWFactory) { m_pDWFactory->Release();   m_pDWFactory = nullptr; }
    if (m_pBrushHint) { m_pBrushHint->Release(); m_pBrushHint = nullptr; }
}

void CUI_LoginBox::Process_Event()
{
}

void CUI_LoginBox::On_Char(wchar_t ch)
{
    // 제어문자 무시
    if (ch == L'\b' || ch == L'\t' || ch == L'\r' || ch == L'\n') return;

    std::wstring& str = (m_iFocus == 0) ? m_strID : m_strPW;
    if (str.size() < 32)
        str += ch;
}