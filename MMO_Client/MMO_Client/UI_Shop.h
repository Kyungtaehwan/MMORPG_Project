#pragma once
#include "UI.h"
#include "UI_CloseButton.h"
#include "UI_InvenGrid.h"
#include "UI_QtyDialog.h"

class CPlayer;
class CInventory;

// ================================================================
//  CUI_Shop  상점 UI (서버 권위 매매)
//  - NPC_Shop 클릭 시 CUI_Manager::Open_Shop 으로 열림
//  - 배경: 검은 박스(반투명) + 회색 테두리
//  - 상단 [구매]/[판매] 탭 전환
//    · 구매: 포션 6종 (좌클릭 = 1개 구매 요청 CS_BUY)
//    · 판매: 인벤의 판매가능(포션) 아이템 (좌클릭 = 1개 판매 요청 CS_SELL)
//  - 매매 결과는 서버 SC_INVEN_UPDATE 스냅샷으로 반영됨
// ================================================================
class CUI_Shop : public CUI
{
public:
    enum SHOP_MODE { MODE_BUY, MODE_SELL };

public:
    CUI_Shop() = default;
    virtual ~CUI_Shop() = default;

public:
    virtual void Initialize()                    override;
    virtual int  Update(float dt)                override;
    virtual void Late_Update(float dt)           override;
    virtual void Render(ID2D1RenderTarget* pRT)  override;
    virtual void Release()                       override;
    virtual void Process_Event()                 override {}

public:
    void Set_References(CPlayer* pPlayer, CInventory* pInven)
    {
        m_pPlayer = pPlayer;
        m_pInven = pInven;
        m_grid.Set_Reference(pInven);
    }

    void Open(int iShopType);
    void Close();
    bool Is_Open() const { return m_bVisible; }

private:
    int  Get_RowAt(POINT tMouse);   // 마우스가 올라간 포션 행 (-1 = 없음)
    int  Get_TabAt(POINT tMouse);   // 0=구매탭 1=판매탭 -1=없음
    void Render_Background(ID2D1RenderTarget* pRT);
    void Render_Tabs(ID2D1RenderTarget* pRT);
    void Render_Rows(ID2D1RenderTarget* pRT);          // 구매 목록
    void Render_SellPrice(ID2D1RenderTarget* pRT);     // 판매 호버 아이템 가격
    void Render_Gold(ID2D1RenderTarget* pRT);

private:
    CPlayer* m_pPlayer = nullptr;
    CInventory* m_pInven = nullptr;
    bool        m_bVisible = false;
    int         m_iShopType = 0;
    int         m_iMode = MODE_BUY;   // 구매/판매 탭
    int         m_iHoverRow = -1;
    CUI_CloseButton m_closeBtn;   // 우상단 X 버튼
    CUI_InvenGrid   m_grid;       // 판매 탭 인벤 그리드(재사용 위젯)
    CUI_QtyDialog   m_dlg;        // 수량 확인 다이얼로그(재사용 위젯)

    // 다이얼로그 확인 시 처리할 대상
    int  m_iPendingKind = 0;      // 0=구매(코드) 1=판매(슬롯)
    int  m_iPendingArg = 0;       // 구매=itemCode / 판매=invenSlot

    // 레이아웃 (인벤토리와 동일 배율)
    static constexpr float PANEL_W = 320.f * 1.3f;
    static constexpr float PANEL_H = 352.f * 1.3f;
    static constexpr int   ROW_COUNT = 6;
    static constexpr float ROW_TOP = 96.f;    // 패널 상단 → 첫 행(탭 아래)
    static constexpr float ROW_H = 52.f;      // 행 높이
    static constexpr float ROW_PAD = 26.f;    // 좌우 여백
    static constexpr float ICON = 44.f;       // 아이콘 크기

    // 탭 (구매/판매)
    static constexpr float TAB_TOP = 58.f;    // 패널 상단 → 탭
    static constexpr float TAB_H = 30.f;
    static constexpr float TAB_W = 78.f;

    // 판매 그리드 (재사용 위젯 레이아웃) — 40칸을 8열×5행으로
    static constexpr int   GRID_COLS = 8;
    static constexpr float GRID_SLOT = 44.f;
    static constexpr float GRID_GAP = 5.f;
    static constexpr float GRID_TOP = 100.f;  // 패널 상단 → 그리드
};
