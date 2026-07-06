#include "pch.h"
#include "UI_Shop.h"
#include "Player.h"
#include "Inventory.h"
#include "ItemData.h"
#include "ItemData_Equipment.h"
#include "Img_Manager.h"
#include "Input_Manager.h"
#include "Network_Manager.h"

// ================================================================
//  판매(구매) 목록 — 포션 6종
//  code = 1000 + 배열인덱스 (아이콘/서버 배열 순서 기준)
//  price 는 서버 ServerItem.h g_PotionBuyPrice 와 값이 동일해야 함.
//  아이콘 키는 Level_Test.cpp 에서 이미 로드됨.
// ================================================================
struct FShopPotion
{
    int          code;
    const TCHAR* name;
    const TCHAR* icon;
    int          price;
};

static const FShopPotion s_ShopPotions[6] =
{
    { 1000, L"HP 물약(중)",  L"Potion_mHP",         30  },
    { 1001, L"HP 물약(대)",  L"Potion_lHP",         50  },
    { 1002, L"MP 물약(중)",  L"Potion_mMp",         30  },
    { 1003, L"MP 물약(대)",  L"Potion_lMp",         50  },
    { 1004, L"공격력 물약",  L"Potion_Atk",         120 },
    { 1005, L"무적 물약",    L"Potion_Invincible",  200 },
};

// ================================================================
//  판매가(클라 표시용). 서버 ServerItem.h ItemSellPrice 와 값이 동일해야 함.
//   포션: 구매가/2  장비:(공+방)*5  스크롤:20  기타:5  그외:0(판매불가)
// ================================================================
static int Shop_SellPrice(CItemData* pItem)
{
    if (!pItem) return 0;

    // 장비는 스탯으로 계산(코드 조회 없이 아이템 자체 값 사용)
    if (pItem->Get_Type() == ITEM_EQUIPMENT)
    {
        CItemData_Equipment* pEq = static_cast<CItemData_Equipment*>(pItem);
        return (pEq->Get_AtkBonus() + pEq->Get_DefBonus()) * 5;
    }

    int code = pItem->Get_ItemCode();
    switch (code / 1000)
    {
    case 1:  // 포션
    {
        int s = code % 1000;
        return (s >= 0 && s < 6) ? s_ShopPotions[s].price / 2 : 0;
    }
    case 2:  return 20;   // 스크롤
    case 4:  return 5;    // 기타
    default: return 0;
    }
}

void CUI_Shop::Initialize()
{
    Set_Pos(WINCX / 2.f, WINCY / 2.f);   // 중심 좌표 (Update_Rect가 ±CX/2로 계산)
    Set_Size(PANEL_W, PANEL_H);
    Update_Rect();

    m_closeBtn.Set_ByPanelCorner((float)m_tRect.right, (float)m_tRect.top);

    // 판매 그리드: 패널 중앙 정렬
    float fGridW = GRID_COLS * GRID_SLOT + (GRID_COLS - 1) * GRID_GAP;
    float fGridL = m_tRect.left + (PANEL_W - fGridW) * 0.5f;
    m_grid.Set_Layout(fGridL, m_tRect.top + GRID_TOP, GRID_COLS, GRID_SLOT, GRID_GAP);
}

void CUI_Shop::Open(int iShopType)
{
    m_iShopType = iShopType;
    m_bVisible = true;
    m_iMode = MODE_BUY;
    m_iHoverRow = -1;
    CInput_Manager::Get_Instance()->Set_InputMode(INPUT_MODE_UI);
    CInput_Manager::Get_Instance()->Set_CursorMode(CURSOR_UI);
}

void CUI_Shop::Close()
{
    m_bVisible = false;
    m_iHoverRow = -1;
    m_dlg.Close();
    CInput_Manager::Get_Instance()->Set_InputMode(INPUT_MODE_GAME);
    CInput_Manager::Get_Instance()->Set_CursorMode(CURSOR_NORMAL);
}

int CUI_Shop::Update(float dt)
{
    if (!m_bVisible) return OBJ_NOEVENT;

    CInput_Manager* pInput = CInput_Manager::Get_Instance();
    pInput->Set_CursorMode(CURSOR_UI);

    // 수량 다이얼로그가 떠 있으면 그쪽으로만 입력 라우팅
    if (m_dlg.Is_Open())
    {
        int r = m_dlg.Update();
        if (r == CUI_QtyDialog::RESULT_CONFIRM)
        {
            int iQty = m_dlg.Get_Qty();
            if (m_iPendingKind == 0)
                CNetwork_Manager::Get_Instance()->SendBuy(m_iPendingArg, iQty);
            else
                CNetwork_Manager::Get_Instance()->SendSell(m_iPendingArg, iQty);
        }
        return OBJ_NOEVENT;
    }

    POINT tMouse = pInput->Get_MousePos();
    bool  bClick = pInput->Key_Down(VK_LBUTTON);

    // 닫기: ESC
    if (pInput->Key_Down(VK_ESCAPE))
    {
        Close();
        return OBJ_NOEVENT;
    }

    // 닫기: 우상단 X 버튼
    if (m_closeBtn.Update(tMouse, bClick))
    {
        Close();
        return OBJ_NOEVENT;
    }

    // 탭 전환 (구매/판매)
    int iTab = Get_TabAt(tMouse);
    if (bClick && iTab != -1)
    {
        m_iMode = iTab;
        m_iHoverRow = -1;
        return OBJ_NOEVENT;
    }

    if (m_iMode == MODE_BUY)
    {
        m_iHoverRow = Get_RowAt(tMouse);

        // 포션 행 좌클릭 → 수량 다이얼로그 오픈(구매). 패널 밖 클릭은 무동작.
        if (bClick && m_iHoverRow != -1)
        {
            const FShopPotion& p = s_ShopPotions[m_iHoverRow];

            // 살 수 있는 최대 수량 = 보유골드/단가 (1~99)
            int iGold = m_pInven ? m_pInven->Get_Gold() : 0;
            int iMax = (p.price > 0) ? iGold / p.price : 1;
            if (iMax < 1)  iMax = 1;    // 못 사면 서버가 거절(스냅샷 불변)
            if (iMax > 99) iMax = 99;

            m_iPendingKind = 0;
            m_iPendingArg = p.code;
            m_dlg.Open(L"구매", p.icon, p.name, iMax);
        }
    }
    else // MODE_SELL
    {
        // 재사용 그리드가 슬롯 히트테스트. 아이템 든 슬롯 클릭 → 수량 다이얼로그.
        int iSlot = m_grid.Update(tMouse, bClick);
        if (iSlot != -1 && m_pInven)
        {
            CItemData* pItem = m_pInven->Get_Item(iSlot);
            if (pItem)
            {
                // 스택 개수 = 판매 가능 최대. 장비 등 비스택은 1(확인만).
                int iMax = m_pInven->Get_StackCount(iSlot);
                m_iPendingKind = 1;
                m_iPendingArg = iSlot;
                m_dlg.Open(L"판매", pItem->Get_IconKey(), pItem->Get_Name(), iMax);
            }
        }
    }

    return OBJ_NOEVENT;
}

void CUI_Shop::Late_Update(float dt) {}

void CUI_Shop::Render(ID2D1RenderTarget* pRT)
{
    if (!m_bVisible) return;

    Render_Background(pRT);
    Render_Tabs(pRT);

    if (m_iMode == MODE_BUY)
        Render_Rows(pRT);
    else
    {
        m_grid.Render(pRT);
        Render_SellPrice(pRT);
    }

    Render_Gold(pRT);
    m_closeBtn.Render(pRT);

    // 수량 다이얼로그(모달)는 항상 맨 위
    m_dlg.Render(pRT);
}

void CUI_Shop::Release() {}

// ===================== 렌더 =====================

void CUI_Shop::Render_Background(ID2D1RenderTarget* pRT)
{
    float fL = (float)m_tRect.left;
    float fT = (float)m_tRect.top;

    ID2D1SolidColorBrush* pBrush = nullptr;

    // 그림자
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.35f), &pBrush);
    pRT->FillRoundedRectangle(
        D2D1::RoundedRect(
            D2D1::RectF(fL + 6.f, fT + 6.f, fL + PANEL_W + 6.f, fT + PANEL_H + 6.f),
            10.f, 10.f),
        pBrush);
    pBrush->Release();

    // 배경: 검은 박스
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.88f), &pBrush);
    pRT->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(fL, fT, fL + PANEL_W, fT + PANEL_H), 10.f, 10.f),
        pBrush);
    pBrush->Release();

    // 테두리 (은은한 회색)
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.5f, 0.5f, 0.55f, 0.9f), &pBrush);
    pRT->DrawRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(fL, fT, fL + PANEL_W, fT + PANEL_H), 10.f, 10.f),
        pBrush, 1.5f);
    pBrush->Release();

    // 제목(패널 상단 중앙)
    CImg_Manager::Get_Instance()->Draw_Text_Center(pRT, L"상  점",
        D2D1::RectF(fL, fT + 22.f, fL + PANEL_W, fT + 48.f),
        D2D1::ColorF(1.f, 0.95f, 0.6f));
}

void CUI_Shop::Render_Tabs(ID2D1RenderTarget* pRT)
{
    float fL = (float)m_tRect.left;
    float fT = (float)m_tRect.top;

    float fTabsW = TAB_W * 2.f + 8.f;
    float fBuyL = fL + (PANEL_W - fTabsW) * 0.5f;
    float fSellL = fBuyL + TAB_W + 8.f;
    float fTabT = fT + TAB_TOP;

    ID2D1SolidColorBrush* pBrush = nullptr;

    const TCHAR* szLabel[2] = { L"구매", L"판매" };
    float        fLefts[2] = { fBuyL, fSellL };

    for (int i = 0; i < 2; ++i)
    {
        bool bActive = (m_iMode == i);
        D2D1_RECT_F rc = D2D1::RectF(fLefts[i], fTabT, fLefts[i] + TAB_W, fTabT + TAB_H);

        // 탭 배경 (활성=밝은 골드, 비활성=어두운 회색)
        D2D1::ColorF bg = bActive
            ? D2D1::ColorF(0.55f, 0.42f, 0.12f)
            : D2D1::ColorF(0.18f, 0.18f, 0.2f);
        pRT->CreateSolidColorBrush(bg, &pBrush);
        pRT->FillRoundedRectangle(D2D1::RoundedRect(rc, 6.f, 6.f), pBrush);
        pBrush->Release();

        // 라벨(탭 정중앙)
        D2D1::ColorF fg = bActive
            ? D2D1::ColorF(1.f, 0.95f, 0.7f)
            : D2D1::ColorF(0.6f, 0.6f, 0.65f);
        CImg_Manager::Get_Instance()->Draw_Text_Center(pRT, szLabel[i], rc, fg);
    }
}

// 판매 탭: 호버 중인 아이템 이름 + 판매가를 그리드 아래에 표시
void CUI_Shop::Render_SellPrice(ID2D1RenderTarget* pRT)
{
    CItemData* pItem = m_grid.Get_HoverItem();
    if (!pItem) return;

    float fL = (float)m_tRect.left;
    float fT = (float)m_tRect.top;

    IDWriteTextFormat* pFont = CImg_Manager::Get_Instance()->Get_DebugFont();
    ID2D1SolidColorBrush* pBrush = nullptr;

    int iPrice = Shop_SellPrice(pItem);

    TCHAR szLine[96];
    if (iPrice > 0)
        swprintf_s(szLine, 96, L"%s  →  %d G 에 판매", pItem->Get_Name(), iPrice);
    else
        swprintf_s(szLine, 96, L"%s (판매 불가)", pItem->Get_Name());

    pRT->CreateSolidColorBrush(
        iPrice > 0 ? D2D1::ColorF(1.f, 0.85f, 0.2f) : D2D1::ColorF(0.8f, 0.4f, 0.4f),
        &pBrush);
    pRT->DrawText(szLine, (UINT32)wcslen(szLine), pFont,
        D2D1::RectF(fL + ROW_PAD, fT + PANEL_H - 74.f, fL + PANEL_W - ROW_PAD, fT + PANEL_H - 52.f),
        pBrush);
    pBrush->Release();
}

void CUI_Shop::Render_Rows(ID2D1RenderTarget* pRT)
{
    float fL = (float)m_tRect.left;
    float fT = (float)m_tRect.top;

    IDWriteTextFormat* pFont = CImg_Manager::Get_Instance()->Get_DebugFont();
    ID2D1SolidColorBrush* pBrush = nullptr;

    for (int i = 0; i < ROW_COUNT; ++i)
    {
        const FShopPotion& p = s_ShopPotions[i];

        float fRowL = fL + ROW_PAD;
        float fRowR = fL + PANEL_W - ROW_PAD;
        float fRowT = fT + ROW_TOP + i * ROW_H;

        // 호버 하이라이트
        if (i == m_iHoverRow)
        {
            pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f, 0.15f), &pBrush);
            pRT->FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(fRowL, fRowT, fRowR, fRowT + ICON), 6.f, 6.f),
                pBrush);
            pBrush->Release();
        }

        // 아이콘
        ID2D1Bitmap* pIcon = CImg_Manager::Get_Instance()->Find_Png(p.icon);
        if (pIcon)
        {
            pRT->DrawBitmap(pIcon,
                D2D1::RectF(fRowL, fRowT, fRowL + ICON, fRowT + ICON),
                1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        }

        // 이름
        pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f), &pBrush);
        pRT->DrawText(p.name, (UINT32)wcslen(p.name), pFont,
            D2D1::RectF(fRowL + ICON + 12.f, fRowT + 8.f, fRowR - 70.f, fRowT + ICON),
            pBrush);
        pBrush->Release();

        // 가격 (골드색)
        TCHAR szPrice[32];
        swprintf_s(szPrice, 32, L"%d G", p.price);
        pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 0.85f, 0.2f), &pBrush);
        pRT->DrawText(szPrice, (UINT32)wcslen(szPrice), pFont,
            D2D1::RectF(fRowR - 68.f, fRowT + 8.f, fRowR, fRowT + ICON),
            pBrush);
        pBrush->Release();
    }
}

void CUI_Shop::Render_Gold(ID2D1RenderTarget* pRT)
{
    if (!m_pInven) return;

    float fL = (float)m_tRect.left;
    float fT = (float)m_tRect.top;

    IDWriteTextFormat* pFont = CImg_Manager::Get_Instance()->Get_DebugFont();
    ID2D1SolidColorBrush* pBrush = nullptr;

    TCHAR szGold[48];
    swprintf_s(szGold, 48, L"보유 골드: %d G", m_pInven->Get_Gold());
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.3f, 1.f, 0.3f), &pBrush);
    pRT->DrawText(szGold, (UINT32)wcslen(szGold), pFont,
        D2D1::RectF(fL + ROW_PAD, fT + PANEL_H - 44.f, fL + PANEL_W - ROW_PAD, fT + PANEL_H - 24.f),
        pBrush);
    pBrush->Release();
}

// ===================== Hit Test =====================

int CUI_Shop::Get_TabAt(POINT tMouse)
{
    float fL = (float)m_tRect.left;
    float fT = (float)m_tRect.top;

    float fTabsW = TAB_W * 2.f + 8.f;
    float fBuyL = fL + (PANEL_W - fTabsW) * 0.5f;
    float fSellL = fBuyL + TAB_W + 8.f;
    float fTabT = fT + TAB_TOP;

    RECT tBuy = { (LONG)fBuyL, (LONG)fTabT, (LONG)(fBuyL + TAB_W), (LONG)(fTabT + TAB_H) };
    if (PtInRect(&tBuy, tMouse)) return MODE_BUY;

    RECT tSell = { (LONG)fSellL, (LONG)fTabT, (LONG)(fSellL + TAB_W), (LONG)(fTabT + TAB_H) };
    if (PtInRect(&tSell, tMouse)) return MODE_SELL;

    return -1;
}

int CUI_Shop::Get_RowAt(POINT tMouse)
{
    float fL = (float)m_tRect.left;
    float fT = (float)m_tRect.top;

    float fRowL = fL + ROW_PAD;
    float fRowR = fL + PANEL_W - ROW_PAD;

    for (int i = 0; i < ROW_COUNT; ++i)
    {
        float fRowT = fT + ROW_TOP + i * ROW_H;
        RECT tRow = { (LONG)fRowL, (LONG)fRowT, (LONG)fRowR, (LONG)(fRowT + ICON) };
        if (PtInRect(&tRow, tMouse))
            return i;
    }
    return -1;
}
