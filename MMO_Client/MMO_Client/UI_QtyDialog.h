#pragma once
#include "define.h"

// ================================================================
//  CUI_QtyDialog  수량 확인 다이얼로그 — 재사용 위젯
// ================================================================
class CUI_QtyDialog
{
public:
    enum RESULT { RESULT_NONE, RESULT_CONFIRM, RESULT_CANCEL };

public:
    void Open(const TCHAR* szAction, const TCHAR* szIconKey,
        const TCHAR* szName, int iMaxQty);
    void Close() { m_bOpen = false; }
    bool Is_Open() const { return m_bOpen; }
    int  Get_Qty() const { return m_iQty; }

    int  Update();                          // RESULT_*
    void Render(ID2D1RenderTarget* pRT);

private:
    RECT Box()      const;
    RECT Rc_Icon()  const;
    RECT Rc_Minus() const;
    RECT Rc_QtyBox()const;
    RECT Rc_Plus()  const;
    RECT Rc_All()   const;
    RECT Rc_Confirm()const;
    RECT Rc_Cancel()const;
    void Set_Qty(int v);
    void Draw_Button(ID2D1RenderTarget* pRT, const RECT& r,
        const TCHAR* szLabel, POINT tMouse, bool bAccent);

private:
    bool  m_bOpen = false;
    bool  m_bStackable = false;   // 수량 입력
    int   m_iMax = 1;
    int   m_iQty = 1;
    bool  m_bTyped = false;       // 숫자키 첫 입력 시 값 교체용
    TCHAR m_szAction[32] = {};    // "판매" / "구매"
    TCHAR m_szName[64] = {};
    TCHAR m_szIconKey[64] = {};

    static constexpr float BOX_W = 320.f;
    static constexpr float BOX_H = 200.f;
};
