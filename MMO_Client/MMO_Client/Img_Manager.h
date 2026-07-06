#pragma once
#include "define.h"


class CMyPng;

class CImg_Manager
{
private:
    CImg_Manager();
    ~CImg_Manager();

public:
    void         Set_RenderTarget(ID2D1RenderTarget* pRT) { m_pRT = pRT; }
    void         Insert_Png(const TCHAR* pFilePath, const TCHAR* pImgKey);
    ID2D1Bitmap* Find_Png(const TCHAR* pImgKey);
    void         Release(void);



public:
    static CImg_Manager* Get_Instance()
    {
        if (!m_pInstance)
            m_pInstance = new CImg_Manager;
        return m_pInstance;
    }
    static void Destroy_Instance()
    {
        if (m_pInstance)
        {
            delete m_pInstance;
            m_pInstance = nullptr;
        }
    }

public:
    IDWriteTextFormat* Get_DebugFont() { return m_pDebugFont; }
    // 가운데 정렬(수평+수직) 폰트 — 버튼/박스 라벨 공용
    IDWriteTextFormat* Get_CenterFont() { return m_pCenterFont; }

    void Create_DebugFont(IDWriteFactory* pDW)
    {
        m_pDWrite = pDW;   // 텍스트 폭 측정용으로 보관

        pDW->CreateTextFormat(
            L"Arial", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            14.f, L"ko-KR",
            &m_pDebugFont
        );

        // 가운데 정렬 폰트 (같은 글꼴/크기, 정렬만 중앙)
        pDW->CreateTextFormat(
            L"Arial", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            14.f, L"ko-KR",
            &m_pCenterFont
        );
        if (m_pCenterFont)
        {
            m_pCenterFont->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_pCenterFont->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }

    // ================================================================
    //  Draw_Text_Center  주어진 rect의 정중앙에 텍스트를 그린다(공식화).
    //   - 박스/버튼 rect를 그대로 넘기면 글자 길이와 무관하게 항상 중앙.
    //   - 브러시 생성/해제까지 내부 처리(한 줄 호출).
    // ================================================================
    void Draw_Text_Center(ID2D1RenderTarget* pRT, const TCHAR* pText,
        const D2D1_RECT_F& rc, const D2D1_COLOR_F& color)
    {
        if (!pRT || !pText || !m_pCenterFont) return;
        ID2D1SolidColorBrush* pBrush = nullptr;
        pRT->CreateSolidColorBrush(color, &pBrush);
        pRT->DrawText(pText, (UINT32)lstrlen(pText), m_pCenterFont, rc, pBrush);
        pBrush->Release();
    }

    // ================================================================
    //  Measure_TextWidth  텍스트를 디버그 폰트로 그렸을 때의 픽셀 폭(DIP).
    //   - 박스를 글자 길이에 맞게 줄일 때 사용. 실패 시 0.
    // ================================================================
    float Measure_TextWidth(const TCHAR* pText)
    {
        if (!m_pDWrite || !m_pDebugFont || !pText || pText[0] == '\0') return 0.f;

        IDWriteTextLayout* pLayout = nullptr;
        if (FAILED(m_pDWrite->CreateTextLayout(pText, (UINT32)lstrlen(pText),
            m_pDebugFont, 4096.f, 64.f, &pLayout)) || !pLayout)
            return 0.f;

        DWRITE_TEXT_METRICS tm = {};
        pLayout->GetMetrics(&tm);
        pLayout->Release();
        return tm.width;
    }


private:
    static CImg_Manager* m_pInstance;

    IDWriteFactory*    m_pDWrite = nullptr;   // 텍스트 측정용(소유 아님)
    IDWriteTextFormat* m_pDebugFont = nullptr;
    IDWriteTextFormat* m_pCenterFont = nullptr;
    ID2D1RenderTarget* m_pRT = nullptr;

    std::map<const TCHAR*, CMyPng*> m_mapPng;
};