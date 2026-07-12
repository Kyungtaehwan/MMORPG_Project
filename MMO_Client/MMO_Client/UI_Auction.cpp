#include "pch.h"
#include "UI_Auction.h"
#include "Player.h"
#include "Inventory.h"
#include "ItemData.h"
#include "Img_Manager.h"
#include "Input_Manager.h"
#include "Network_Manager.h"

// 좌측정렬 텍스트 (경량 헬퍼)
static void DrawL(ID2D1RenderTarget* pRT, const TCHAR* t, D2D1_RECT_F rc, D2D1_COLOR_F c)
{
    if (!t) return;
    ID2D1SolidColorBrush* b = nullptr;
    pRT->CreateSolidColorBrush(c, &b);
    pRT->DrawText(t, (UINT32)lstrlen(t), CImg_Manager::Get_Instance()->Get_DebugFont(), rc, b);
    b->Release();
}

// char(계정 id / "경매장" CP949) - wide
static void ToWide(const char* src, wchar_t* dst, int cap)
{
    dst[0] = 0;
    if (src) MultiByteToWideChar(CP_ACP, 0, src, -1, dst, cap);
}

// ===================== 라이프사이클 =====================

void CUI_Auction::Initialize()
{
    Set_Pos(WINCX / 2.f, WINCY / 2.f);
    Set_Size(PANEL_W, PANEL_H);
    Update_Rect();

    m_closeBtn.Set_ByPanelCorner((float)m_tRect.right, (float)m_tRect.top);

    // 등록 탭 인벤 그리드 (8열 x 5행)
    m_grid.Set_Layout((float)m_tRect.left + 16.f, (float)m_tRect.top + CONTENT_TOP,
        8, 40.f, 4.f);
}

void CUI_Auction::Open()
{
    m_bVisible = true;
    m_iTab = TAB_BUY;
    m_iPage = 0;
    m_iFocus = FOCUS_NONE;
    m_szSearch[0] = 0;
    m_iRegSlot = -1;
    m_iRegCount = 1;
    m_iRegPrice = 10;
    m_iPending = PEND_NONE;
    m_confirm.Close();

    CInput_Manager::Get_Instance()->Set_InputMode(INPUT_MODE_UI);
    CInput_Manager::Get_Instance()->Set_CursorMode(CURSOR_UI);

    Request_List();   // 0페이지 구매탭 요청
}

void CUI_Auction::Close()
{
    m_bVisible = false;
    m_dlg.Close();
    m_confirm.Close();
    CInput_Manager::Get_Instance()->Set_InputMode(INPUT_MODE_GAME);
    CInput_Manager::Get_Instance()->Set_CursorMode(CURSOR_NORMAL);
}

// ===================== 유틸 =====================

RECT CUI_Auction::R(float ox, float oy, float w, float h) const
{
    float l = (float)m_tRect.left + ox;
    float t = (float)m_tRect.top + oy;
    return RECT{ (LONG)l, (LONG)t, (LONG)(l + w), (LONG)(t + h) };
}

void CUI_Auction::Item_Display(int code, TCHAR* outName, int nameCap,
    TCHAR* outIcon, int iconCap)
{
    outName[0] = 0; outIcon[0] = 0;
    CItemData* p = CInventory::Create_ItemFromCode(code);
    if (!p) return;
    lstrcpyn(outName, p->Get_Name(), nameCap);
    lstrcpyn(outIcon, p->Get_IconKey(), iconCap);
    delete p;
}

// 검색어와 이름이 일치하는 아이템 코드들을 수집(코드-이름은 클라만 앎).
// 서버는 이 코드 목록으로 item_code IN(...) 필터. 반환=수집 개수.
int CUI_Auction::Resolve_Search(int32_t* outCodes, int cap)
{
    int cnt = 0;
    if (m_szSearch[0] == 0) return 0;   // 검색어 없음 - 필터 없음

    // 카테고리별 (코드 = cat*1000 + sub) 열거. sub 상한은 각 enum의 _END.
    struct { int cat; int end; } cats[] = {
        { 1, POTION_END }, { 2, SCROLL_END }, { 3, EQUIP_TYPE_END }, { 4, ETC_END }
    };
    for (auto& c : cats)
    {
        for (int sub = 0; sub < c.end; ++sub)
        {
            int code = c.cat * 1000 + sub;
            TCHAR nm[64], ic[64];
            Item_Display(code, nm, 64, ic, 64);
            if (nm[0] && wcsstr(nm, m_szSearch))
            {
                if (cnt < cap) outCodes[cnt++] = code;
                if (cnt >= cap) return cnt;
            }
        }
    }
    // 검색어는 있는데 일치 코드가 0개 - 아무것도 안 나오게 불가능 코드 하나 전송
    if (cnt == 0 && cap > 0) { outCodes[0] = -1; cnt = 1; }
    return cnt;
}

// 현재 탭/페이지/검색으로 서버에 목록 재요청.
void CUI_Auction::Request_List()
{
    if (m_iTab == TAB_REGISTER) return;   // 등록 탭은 목록 없음

    int32_t codes[AUCTION_SEARCH_MAX];
    int count = (m_iTab == TAB_BUY) ? Resolve_Search(codes, AUCTION_SEARCH_MAX) : 0;
    if (m_iPage < 0) m_iPage = 0;
    CNetwork_Manager::Get_Instance()->SendAuctionList(
        m_iPage, Server_Tab(), codes, count);
}

// ===================== 입력 =====================

int CUI_Auction::Update(float dt)
{
    if (!m_bVisible) return OBJ_NOEVENT;

    CInput_Manager* in = CInput_Manager::Get_Instance();
    in->Set_CursorMode(CURSOR_UI);

    // 확인 팝업(등록/취소) 우선
    if (m_confirm.Is_Open())
    {
        int r = m_confirm.Update();
        if (r == CUI_ConfirmDialog::RESULT_CONFIRM)
        {
            if (m_iPending == PEND_REGISTER && m_iRegSlot >= 0)
            {
                CNetwork_Manager::Get_Instance()->SendAuctionRegister(
                    m_iRegSlot, m_iRegCount, m_iRegPrice);
                m_iRegSlot = -1; m_iRegCount = 1;
            }
            else if (m_iPending == PEND_CANCEL)
            {
                CNetwork_Manager::Get_Instance()->SendAuctionCancel(m_iPendingCancelID);
                Request_List();   // 취소 반영 위해 현재 페이지 재요청(서버가 취소 후 처리)
            }
        }
        if (r != CUI_ConfirmDialog::RESULT_NONE) m_iPending = PEND_NONE;
        return OBJ_NOEVENT;
    }

    // 수량 다이얼로그(구매) 우선
    if (m_dlg.Is_Open())
    {
        int r = m_dlg.Update();
        if (r == CUI_QtyDialog::RESULT_CONFIRM)
        {
            CNetwork_Manager::Get_Instance()->SendAuctionBuy(m_iPendingListing, m_dlg.Get_Qty());
            Request_List();   // 구매 반영 위해 현재 페이지 재요청(서버가 구매 후 처리)
        }
        return OBJ_NOEVENT;
    }

    POINT m = in->Get_MousePos();
    bool  bClick = in->Key_Down(VK_LBUTTON);

    if (in->Key_Down(VK_ESCAPE)) { Close(); return OBJ_NOEVENT; }
    if (m_closeBtn.Update(m, bClick)) { Close(); return OBJ_NOEVENT; }

    // 새로고침(모든 탭 공통) — 현재 탭/페이지/검색 그대로 재요청
    RECT rRefresh = R(PANEL_W - 100.f, 44.f, 80.f, 30.f);
    if (bClick && PtInRect(&rRefresh, m))
    {
        Request_List();
        return OBJ_NOEVENT;
    }

    int tab = Get_TabAt(m);
    if (bClick && tab != -1)
    {
        if (tab != m_iTab)
        {
            m_iTab = tab; m_iPage = 0; m_iFocus = FOCUS_NONE;
            Request_List();   // 탭 전환 시 0페이지부터 새로 요청
        }
        return OBJ_NOEVENT;
    }

    if (m_iTab == TAB_BUY)          Update_Buy(m, bClick);
    else if (m_iTab == TAB_REGISTER) Update_Register(m, bClick);
    else                             Update_Mine(m, bClick);

    return OBJ_NOEVENT;
}

int CUI_Auction::Get_TabAt(POINT m)
{
    for (int i = 0; i < 3; ++i)
    {
        RECT r = R(20.f + i * 96.f, 44.f, 90.f, 30.f);
        if (PtInRect(&r, m)) return i;
    }
    return -1;
}

void CUI_Auction::Update_Buy(POINT m, bool bClick)
{
    if (!bClick) return;

    RECT rs = R(70.f, CONTENT_TOP, 250.f, 28.f);
    if (PtInRect(&rs, m)) { m_iFocus = FOCUS_SEARCH; return; }

    CNetwork_Manager* net = CNetwork_Manager::Get_Instance();
    RECT rp = R(PANEL_W - 150.f, CONTENT_TOP, 34.f, 28.f);
    RECT rn = R(PANEL_W - 48.f, CONTENT_TOP, 34.f, 28.f);
    if (PtInRect(&rp, m)) { if (m_iPage > 0) { --m_iPage; Request_List(); } return; }
    if (PtInRect(&rn, m)) { if (net->GetAuctionHasNext()) { ++m_iPage; Request_List(); } return; }

    const FAuctionEntry* ents = net->GetAuctionEntries();
    int n = net->GetAuctionCount();   // 서버가 이미 필터+페이지 처리한 결과
    for (int i = 0; i < n && i < ROWS_PER_PAGE; ++i)
    {
        RECT rb = R(PANEL_W - 96.f, LIST_TOP + i * ROW_H + 16.f, 80.f, 30.f);
        if (PtInRect(&rb, m))
        {
            const FAuctionEntry& e = ents[i];
            TCHAR nm[64], ic[64];
            Item_Display(e.itemCode, nm, 64, ic, 64);
            m_iPendingListing = e.listingID;
            m_dlg.Open(L"구매", ic, nm, e.count);
            return;
        }
    }
    m_iFocus = FOCUS_NONE;
}

void CUI_Auction::Update_Mine(POINT m, bool bClick)
{
    if (!bClick) return;

    CNetwork_Manager* net = CNetwork_Manager::Get_Instance();
    RECT rp = R(PANEL_W - 150.f, CONTENT_TOP, 34.f, 28.f);
    RECT rn = R(PANEL_W - 48.f, CONTENT_TOP, 34.f, 28.f);
    if (PtInRect(&rp, m)) { if (m_iPage > 0) { --m_iPage; Request_List(); } return; }
    if (PtInRect(&rn, m)) { if (net->GetAuctionHasNext()) { ++m_iPage; Request_List(); } return; }

    const FAuctionEntry* ents = net->GetAuctionEntries();
    int n = net->GetAuctionCount();
    for (int i = 0; i < n && i < ROWS_PER_PAGE; ++i)
    {
        const FAuctionEntry& e = ents[i];

        RECT rCollect = R(PANEL_W - 176.f, LIST_TOP + i * ROW_H + 16.f, 74.f, 30.f);
        RECT rCancel = R(PANEL_W - 96.f, LIST_TOP + i * ROW_H + 16.f, 74.f, 30.f);

        if (PtInRect(&rCollect, m))
        {
            if (e.pendingGold > 0)
            {
                net->SendAuctionCollect(e.listingID);
                Request_List();   // 수령 반영 위해 재요청
            }
            return;
        }
        if (PtInRect(&rCancel, m))
        {
            // 취소 확인 팝업 (남은 수량 반환 + 미수령 골드 지급 + 매물 제거)
            m_iPending = PEND_CANCEL;
            m_iPendingCancelID = e.listingID;
            m_confirm.Open(L"등록 취소",
                L"이 매물을 취소하시겠습니까?\n남은 수량과 미수령 골드를 돌려받습니다.");
            return;
        }
    }
}

void CUI_Auction::Update_Register(POINT m, bool bClick)
{
    // 인벤 그리드에서 아이템 선택
    int slot = m_grid.Update(m, bClick);
    if (slot != -1)
    {
        m_iRegSlot = slot;
        int cnt = m_pInven ? m_pInven->Get_StackCount(slot) : 1;
        m_iRegCount = (cnt > 0) ? cnt : 1;
        m_iFocus = FOCUS_NONE;
        return;
    }
    if (!bClick) return;

    const float rx = 372.f;
    const float y = CONTENT_TOP + 78.f;
    RECT rMinus = R(rx, y, 28.f, 28.f);
    RECT rNum = R(rx + 32.f, y, 54.f, 28.f);
    RECT rPlus = R(rx + 90.f, y, 28.f, 28.f);
    RECT rAll = R(rx + 122.f, y, 44.f, 28.f);
    RECT rPrice = R(rx, CONTENT_TOP + 140.f, 120.f, 28.f);
    RECT rReg = R(rx, CONTENT_TOP + 206.f, 150.f, 34.f);

    int stackMax = (m_iRegSlot >= 0 && m_pInven) ? m_pInven->Get_StackCount(m_iRegSlot) : 1;

    if (PtInRect(&rMinus, m)) { if (m_iRegCount > 1) --m_iRegCount; m_iFocus = FOCUS_NONE; return; }
    if (PtInRect(&rPlus, m)) { if (m_iRegCount < stackMax) ++m_iRegCount; m_iFocus = FOCUS_NONE; return; }
    if (PtInRect(&rAll, m)) { m_iRegCount = (stackMax > 0) ? stackMax : 1; m_iFocus = FOCUS_NONE; return; }
    if (PtInRect(&rNum, m)) { m_iFocus = FOCUS_COUNT; m_bFieldFresh = true; return; }
    if (PtInRect(&rPrice, m)) { m_iFocus = FOCUS_PRICE; m_bFieldFresh = true; return; }
    if (PtInRect(&rReg, m))
    {
        if (m_iRegSlot >= 0 && m_iRegCount > 0 && m_iRegPrice > 0 && m_pInven)
        {
            CItemData* pItem = m_pInven->Get_Item(m_iRegSlot);
            TCHAR msg[192];
            swprintf_s(msg, 192, L"'%s' %d개를\n개당 %d G (합계 %d G)에 등록하시겠습니까?",
                pItem ? pItem->Get_Name() : L"아이템", m_iRegCount, m_iRegPrice,
                m_iRegPrice * m_iRegCount);
            m_iPending = PEND_REGISTER;
            m_confirm.Open(L"경매 등록", msg);
        }
        m_iFocus = FOCUS_NONE;
        return;
    }
    m_iFocus = FOCUS_NONE;
}

void CUI_Auction::On_Char(wchar_t ch)
{
    if (!m_bVisible) return;

    if (m_iFocus == FOCUS_SEARCH)
    {
        int len = lstrlenW(m_szSearch);
        bool changed = false;
        if (ch == 8) { if (len > 0) { m_szSearch[len - 1] = 0; changed = true; } }
        else if (ch >= 32 && len < 31) { m_szSearch[len] = ch; m_szSearch[len + 1] = 0; changed = true; }
        if (changed) { m_iPage = 0; Request_List(); }   // 라이브 검색: 입력마다 서버 재조회
        return;
    }

    if (m_iFocus == FOCUS_COUNT || m_iFocus == FOCUS_PRICE)
    {
        int* v = (m_iFocus == FOCUS_COUNT) ? &m_iRegCount : &m_iRegPrice;
        if (ch == 8) { *v /= 10; }
        else if (ch >= L'0' && ch <= L'9')
        {
            int d = (int)(ch - L'0');
            *v = m_bFieldFresh ? d : (*v) * 10 + d;
            m_bFieldFresh = false;
        }
        if (m_iFocus == FOCUS_COUNT)
        {
            int sm = (m_iRegSlot >= 0 && m_pInven) ? m_pInven->Get_StackCount(m_iRegSlot) : 1;
            if (m_iRegCount < 1) m_iRegCount = 1;
            if (sm > 0 && m_iRegCount > sm) m_iRegCount = sm;
        }
        else
        {
            if (m_iRegPrice < 0) m_iRegPrice = 0;
            if (m_iRegPrice > 9999999) m_iRegPrice = 9999999;
        }
    }
}

void CUI_Auction::Late_Update(float dt) {}
void CUI_Auction::Release() {}

// ===================== 렌더 =====================

void CUI_Auction::Render(ID2D1RenderTarget* pRT)
{
    if (!m_bVisible) return;

    Render_Bg(pRT);
    Render_Tabs(pRT);

    if (m_iTab == TAB_BUY)          Render_Buy(pRT);
    else if (m_iTab == TAB_REGISTER) Render_Register(pRT);
    else                             Render_Mine(pRT);

    m_closeBtn.Render(pRT);
    m_dlg.Render(pRT);
    m_confirm.Render(pRT);
}

void CUI_Auction::Render_Bg(ID2D1RenderTarget* pRT)
{
    float fL = (float)m_tRect.left, fT = (float)m_tRect.top;
    ID2D1SolidColorBrush* b = nullptr;

    pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.35f), &b);
    pRT->FillRoundedRectangle(D2D1::RoundedRect(
        D2D1::RectF(fL + 6, fT + 6, fL + PANEL_W + 6, fT + PANEL_H + 6), 10, 10), b);
    b->Release();

    pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.9f), &b);
    pRT->FillRoundedRectangle(D2D1::RoundedRect(
        D2D1::RectF(fL, fT, fL + PANEL_W, fT + PANEL_H), 10, 10), b);
    b->Release();

    pRT->CreateSolidColorBrush(D2D1::ColorF(0.5f, 0.5f, 0.55f, 0.9f), &b);
    pRT->DrawRoundedRectangle(D2D1::RoundedRect(
        D2D1::RectF(fL, fT, fL + PANEL_W, fT + PANEL_H), 10, 10), b, 1.5f);
    b->Release();

    CImg_Manager::Get_Instance()->Draw_Text_Center(pRT, L"경매장",
        D2D1::RectF(fL, fT + 12.f, fL + PANEL_W, fT + 38.f),
        D2D1::ColorF(1.f, 0.95f, 0.6f));
}

void CUI_Auction::Render_Tabs(ID2D1RenderTarget* pRT)
{
    const TCHAR* label[3] = { L"구매", L"등록", L"내판매" };
    for (int i = 0; i < 3; ++i)
    {
        RECT r = R(20.f + i * 96.f, 44.f, 90.f, 30.f);
        bool active = (m_iTab == i);
        ID2D1SolidColorBrush* b = nullptr;
        D2D1::ColorF bg = active ? D2D1::ColorF(0.55f, 0.42f, 0.12f) : D2D1::ColorF(0.18f, 0.18f, 0.2f);
        pRT->CreateSolidColorBrush(bg, &b);
        pRT->FillRoundedRectangle(D2D1::RoundedRect(
            D2D1::RectF((float)r.left, (float)r.top, (float)r.right, (float)r.bottom), 6, 6), b);
        b->Release();

        D2D1::ColorF fg = active ? D2D1::ColorF(1.f, 0.95f, 0.7f) : D2D1::ColorF(0.6f, 0.6f, 0.65f);
        CImg_Manager::Get_Instance()->Draw_Text_Center(pRT, label[i],
            D2D1::RectF((float)r.left, (float)r.top, (float)r.right, (float)r.bottom), fg);
    }

    // 새로고침 버튼 (탭 우측)
    Draw_Btn(pRT, R(PANEL_W - 100.f, 44.f, 80.f, 30.f), L"새로고침",
        CInput_Manager::Get_Instance()->Get_MousePos(), false, true);
}

void CUI_Auction::Draw_Btn(ID2D1RenderTarget* pRT, const RECT& r, const TCHAR* label,
    POINT m, bool bAccent, bool bEnabled)
{
    ID2D1SolidColorBrush* b = nullptr;
    bool hover = bEnabled && PtInRect(&r, m);
    D2D1_RECT_F rc = D2D1::RectF((float)r.left, (float)r.top, (float)r.right, (float)r.bottom);

    D2D1::ColorF bg = !bEnabled ? D2D1::ColorF(0.15f, 0.15f, 0.17f)
        : bAccent ? (hover ? D2D1::ColorF(0.20f, 0.55f, 0.25f) : D2D1::ColorF(0.14f, 0.42f, 0.18f))
        : (hover ? D2D1::ColorF(0.40f, 0.40f, 0.45f) : D2D1::ColorF(0.24f, 0.24f, 0.28f));
    pRT->CreateSolidColorBrush(bg, &b);
    pRT->FillRoundedRectangle(D2D1::RoundedRect(rc, 6, 6), b);
    b->Release();

    D2D1::ColorF fg = bEnabled ? D2D1::ColorF(1.f, 1.f, 1.f) : D2D1::ColorF(0.45f, 0.45f, 0.5f);
    CImg_Manager::Get_Instance()->Draw_Text_Center(pRT, label, rc, fg);
}

// 검색바 + 페이지바 공통
static void Draw_Field(ID2D1RenderTarget* pRT, const RECT& r, const TCHAR* text,
    bool bFocus, const TCHAR* placeholder)
{
    ID2D1SolidColorBrush* b = nullptr;
    D2D1_RECT_F rc = D2D1::RectF((float)r.left, (float)r.top, (float)r.right, (float)r.bottom);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.15f), &b);
    pRT->FillRoundedRectangle(D2D1::RoundedRect(rc, 5, 5), b); b->Release();
    pRT->CreateSolidColorBrush(bFocus ? D2D1::ColorF(1.f, 0.85f, 0.2f) : D2D1::ColorF(0.4f, 0.4f, 0.45f), &b);
    pRT->DrawRoundedRectangle(D2D1::RoundedRect(rc, 5, 5), b, 1.2f); b->Release();

    bool empty = (!text || text[0] == 0);
    DrawL(pRT, empty ? placeholder : text,
        D2D1::RectF(rc.left + 8.f, rc.top + 5.f, rc.right - 4.f, rc.bottom),
        empty ? D2D1::ColorF(0.5f, 0.5f, 0.55f) : D2D1::ColorF(1.f, 1.f, 1.f));
}

void CUI_Auction::Render_ListRows(ID2D1RenderTarget* pRT, bool bMine)
{
    CNetwork_Manager* net = CNetwork_Manager::Get_Instance();
    const FAuctionEntry* ents = net->GetAuctionEntries();
    int n = net->GetAuctionCount();   // 서버가 이미 필터+페이지 처리

    if (n == 0)
    {
        DrawL(pRT, bMine ? L"등록한 매물이 없습니다." : L"매물이 없습니다.",
            D2D1::RectF((float)m_tRect.left + 20.f, (float)m_tRect.top + LIST_TOP + 10.f,
                (float)m_tRect.right, (float)m_tRect.top + LIST_TOP + 34.f),
            D2D1::ColorF(0.6f, 0.6f, 0.65f));
        return;
    }

    for (int i = 0; i < n && i < ROWS_PER_PAGE; ++i)
    {
        const FAuctionEntry& e = ents[i];

        float rowY = LIST_TOP + i * ROW_H;

        // 행 배경
        ID2D1SolidColorBrush* b = nullptr;
        pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f, 0.05f), &b);
        pRT->FillRoundedRectangle(D2D1::RoundedRect(
            D2D1::RectF((float)m_tRect.left + 12.f, (float)m_tRect.top + rowY + 4.f,
                (float)m_tRect.left + PANEL_W - 12.f, (float)m_tRect.top + rowY + ROW_H - 4.f), 6, 6), b);
        b->Release();

        // 아이콘 + 이름
        TCHAR nm[64], ic[64];
        Item_Display(e.itemCode, nm, 64, ic, 64);
        RECT ri = R(20.f, rowY + 8.f, 44.f, 44.f);
        ID2D1Bitmap* pIcon = CImg_Manager::Get_Instance()->Find_Png(ic);
        if (pIcon)
            pRT->DrawBitmap(pIcon, D2D1::RectF((float)ri.left, (float)ri.top, (float)ri.right, (float)ri.bottom),
                1.f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

        // 버튼이 차지하는 폭만큼 텍스트 우측 여백 확보(내판매=버튼2개)
        float textR = bMine ? (PANEL_W - 186.f) : (PANEL_W - 110.f);

        DrawL(pRT, nm, D2D1::RectF((float)m_tRect.left + 76.f, (float)m_tRect.top + rowY + 8.f,
            (float)m_tRect.left + textR, (float)m_tRect.top + rowY + 28.f),
            D2D1::ColorF(1.f, 1.f, 1.f));

        // 세부 정보
        TCHAR info[128];
        if (bMine)
        {
            swprintf_s(info, 128, L"남은 %d개    미수령 %d G", e.count, e.pendingGold);
        }
        else
        {
            wchar_t seller[24]; ToWide(e.sellerName, seller, 24);
            swprintf_s(info, 128, L"x%d   개당 %d G   판매자:%s", e.count, e.unitPrice, seller);
        }
        DrawL(pRT, info, D2D1::RectF((float)m_tRect.left + 76.f, (float)m_tRect.top + rowY + 32.f,
            (float)m_tRect.left + textR, (float)m_tRect.top + rowY + 52.f),
            D2D1::ColorF(0.8f, 0.85f, 0.9f));

        // 액션 버튼
        POINT mp = CInput_Manager::Get_Instance()->Get_MousePos();
        if (bMine)
        {
            // 수령(판매분 있을 때만) + 취소(항상)
            Draw_Btn(pRT, R(PANEL_W - 176.f, rowY + 16.f, 74.f, 30.f), L"수령", mp, true, e.pendingGold > 0);
            Draw_Btn(pRT, R(PANEL_W - 96.f, rowY + 16.f, 74.f, 30.f), L"취소", mp, false, true);
        }
        else
        {
            Draw_Btn(pRT, R(PANEL_W - 96.f, rowY + 16.f, 80.f, 30.f), L"구매", mp, true, true);
        }
    }
}

void CUI_Auction::Render_Buy(ID2D1RenderTarget* pRT)
{
    POINT mp = CInput_Manager::Get_Instance()->Get_MousePos();

    DrawL(pRT, L"검색:", D2D1::RectF((float)m_tRect.left + 20.f, (float)m_tRect.top + CONTENT_TOP + 4.f,
        (float)m_tRect.left + 70.f, (float)m_tRect.top + CONTENT_TOP + 26.f), D2D1::ColorF(0.85f, 0.85f, 0.9f));
    Draw_Field(pRT, R(70.f, CONTENT_TOP, 250.f, 28.f), m_szSearch,
        m_iFocus == FOCUS_SEARCH, L"아이템 이름");

    // 페이지 (서버 페이지네이션: 총 페이지수는 모르므로 현재 페이지만 표시, >는 hasNext로)
    bool bNext = CNetwork_Manager::Get_Instance()->GetAuctionHasNext();
    Draw_Btn(pRT, R(PANEL_W - 150.f, CONTENT_TOP, 34.f, 28.f), L"<", mp, false, m_iPage > 0);
    TCHAR pg[24]; swprintf_s(pg, 24, L"%d 페이지", m_iPage + 1);
    CImg_Manager::Get_Instance()->Draw_Text_Center(pRT, pg,
        D2D1::RectF((float)m_tRect.left + PANEL_W - 112.f, (float)m_tRect.top + CONTENT_TOP,
            (float)m_tRect.left + PANEL_W - 52.f, (float)m_tRect.top + CONTENT_TOP + 28.f),
        D2D1::ColorF(0.9f, 0.9f, 0.95f));
    Draw_Btn(pRT, R(PANEL_W - 48.f, CONTENT_TOP, 34.f, 28.f), L">", mp, false, bNext);

    Render_ListRows(pRT, false);
}

void CUI_Auction::Render_Mine(ID2D1RenderTarget* pRT)
{
    POINT mp = CInput_Manager::Get_Instance()->Get_MousePos();

    DrawL(pRT, L"내 판매 목록 (판매되면 '수령'으로 골드 회수)",
        D2D1::RectF((float)m_tRect.left + 20.f, (float)m_tRect.top + CONTENT_TOP + 4.f,
            (float)m_tRect.left + PANEL_W - 160.f, (float)m_tRect.top + CONTENT_TOP + 26.f),
        D2D1::ColorF(0.85f, 0.85f, 0.9f));

    bool bNext = CNetwork_Manager::Get_Instance()->GetAuctionHasNext();
    Draw_Btn(pRT, R(PANEL_W - 150.f, CONTENT_TOP, 34.f, 28.f), L"<", mp, false, m_iPage > 0);
    TCHAR pg[24]; swprintf_s(pg, 24, L"%d 페이지", m_iPage + 1);
    CImg_Manager::Get_Instance()->Draw_Text_Center(pRT, pg,
        D2D1::RectF((float)m_tRect.left + PANEL_W - 112.f, (float)m_tRect.top + CONTENT_TOP,
            (float)m_tRect.left + PANEL_W - 52.f, (float)m_tRect.top + CONTENT_TOP + 28.f),
        D2D1::ColorF(0.9f, 0.9f, 0.95f));
    Draw_Btn(pRT, R(PANEL_W - 48.f, CONTENT_TOP, 34.f, 28.f), L">", mp, false, bNext);

    Render_ListRows(pRT, true);
}

void CUI_Auction::Render_Register(ID2D1RenderTarget* pRT)
{
    POINT mp = CInput_Manager::Get_Instance()->Get_MousePos();

    // 인벤 그리드
    m_grid.Render(pRT);

    const float rx = 372.f;
    float fL = (float)m_tRect.left, fT = (float)m_tRect.top;

    // 선택 아이템
    if (m_iRegSlot >= 0 && m_pInven && m_pInven->Get_Item(m_iRegSlot))
    {
        CItemData* pItem = m_pInven->Get_Item(m_iRegSlot);
        ID2D1Bitmap* pIcon = CImg_Manager::Get_Instance()->Find_Png(pItem->Get_IconKey());
        RECT ri = R(rx, CONTENT_TOP + 4.f, 40.f, 40.f);
        if (pIcon)
            pRT->DrawBitmap(pIcon, D2D1::RectF((float)ri.left, (float)ri.top, (float)ri.right, (float)ri.bottom),
                1.f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        DrawL(pRT, pItem->Get_Name(), D2D1::RectF(fL + rx + 48.f, fT + CONTENT_TOP + 12.f,
            fL + PANEL_W - 12.f, fT + CONTENT_TOP + 36.f), D2D1::ColorF(1.f, 1.f, 1.f));
    }
    else
    {
        DrawL(pRT, L"- 아이템 선택", D2D1::RectF(fL + rx, fT + CONTENT_TOP + 12.f,
            fL + PANEL_W - 12.f, fT + CONTENT_TOP + 36.f), D2D1::ColorF(0.6f, 0.6f, 0.65f));
    }

    // 개수
    DrawL(pRT, L"개수", D2D1::RectF(fL + rx, fT + CONTENT_TOP + 56.f, fL + rx + 100.f, fT + CONTENT_TOP + 76.f),
        D2D1::ColorF(0.85f, 0.85f, 0.9f));
    float y = CONTENT_TOP + 78.f;
    Draw_Btn(pRT, R(rx, y, 28.f, 28.f), L"-", mp, false, true);
    { TCHAR c[16]; swprintf_s(c, 16, L"%d", m_iRegCount);
      Draw_Field(pRT, R(rx + 32.f, y, 54.f, 28.f), c, m_iFocus == FOCUS_COUNT, L""); }
    Draw_Btn(pRT, R(rx + 90.f, y, 28.f, 28.f), L"+", mp, false, true);
    Draw_Btn(pRT, R(rx + 122.f, y, 44.f, 28.f), L"전체", mp, false, true);

    // 개당 가격
    DrawL(pRT, L"개당 가격(G)", D2D1::RectF(fL + rx, fT + CONTENT_TOP + 118.f,
        fL + rx + 160.f, fT + CONTENT_TOP + 138.f), D2D1::ColorF(0.85f, 0.85f, 0.9f));
    { TCHAR p[24]; swprintf_s(p, 24, L"%d", m_iRegPrice);
      Draw_Field(pRT, R(rx, CONTENT_TOP + 140.f, 120.f, 28.f), p, m_iFocus == FOCUS_PRICE, L""); }

    // 합계
    TCHAR total[48]; swprintf_s(total, 48, L"합계 %d G", m_iRegPrice * m_iRegCount);
    DrawL(pRT, total, D2D1::RectF(fL + rx, fT + CONTENT_TOP + 176.f,
        fL + PANEL_W - 12.f, fT + CONTENT_TOP + 198.f), D2D1::ColorF(1.f, 0.85f, 0.2f));

    // 등록
    Draw_Btn(pRT, R(rx, CONTENT_TOP + 206.f, 150.f, 34.f), L"등록", mp, true, m_iRegSlot >= 0);
}
