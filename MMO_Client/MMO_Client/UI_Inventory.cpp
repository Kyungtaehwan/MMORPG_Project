#include "pch.h"
#include "UI_Inventory.h"
#include "Inventory.h"
#include "Equipment.h"
#include "Player.h"
#include "ItemData.h"
#include "ItemData_UseItem.h"
#include "ItemData_Equipment.h"
#include "Img_Manager.h"
#include "Input_Manager.h"

static const EQUIP_SLOT_INFO s_SlotLayout[SLOT_END] =
{
    {18.f * INVEN_RATE, 76.f * INVEN_RATE, 56.f * INVEN_RATE, 84.f * INVEN_RATE },   // SLOT_WEAPON  - 무기    28x84
    {133.f * INVEN_RATE, 76.f * INVEN_RATE, 56.f * INVEN_RATE, 84.f * INVEN_RATE},    // SLOT_ARMOR   - 방어구  56x84
    {249.f * INVEN_RATE, 76.f * INVEN_RATE,56.f * INVEN_RATE, 84.f * INVEN_RATE},      //SLOT_SHIELD   - 쉴드    56x84
    {133.f * INVEN_RATE, 4.f* INVEN_RATE,56.f * INVEN_RATE, 56.f * INVEN_RATE},            //SLOT_HELMAT   - 헬멧    56X56
    {205.f * INVEN_RATE, 32.f * INVEN_RATE, INVEN_SLOT_SIZE, INVEN_SLOT_SIZE},            //SLOT_PENDANT - INVEN_SLOT_SIZE
    {248.f * INVEN_RATE, 177.f * INVEN_RATE, INVEN_SLOT_SIZE, INVEN_SLOT_SIZE}             //SLOT_RING   - INVEN_SLOT_SIZE
};

void CUI_Inventory::Initialize()
{
    CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/UI/Inventory.png", L"INVENTORY_FRAME");

    Set_Pos(WINCX/2.f-300.f, WINCY/2.f-50.f);
    Set_Size(PANEL_W, PANEL_H);
    Update_Rect();

    m_closeBtn.Set_ByPanelCorner((float)m_tRect.right, (float)m_tRect.top);
}

void CUI_Inventory::Close_Panel()
{
    m_bVisible = false;
    m_iHoverInvenSlot = -1;
    m_iHoverEquipSlot = -1;
    CInput_Manager::Get_Instance()->Set_InputMode(INPUT_MODE_GAME);
    CInput_Manager::Get_Instance()->Set_CursorMode(CURSOR_NORMAL);
}

int CUI_Inventory::Update(float dt)
{
    CInput_Manager* pInput = CInput_Manager::Get_Instance();

    if (pInput->Key_Down('I'))
    {
        if (m_bVisible) { Close_Panel(); return OBJ_NOEVENT; }
        m_bVisible = true;
        pInput->Set_InputMode(INPUT_MODE_UI);
    }

    if (!m_bVisible) return OBJ_NOEVENT;

    pInput->Set_CursorMode(CURSOR_UI);
    POINT tMouse = pInput->Get_MousePos();
    bool  bClick = pInput->Key_Down(VK_LBUTTON);

    // 닫기: ESC / 우상단 X 버튼 (패널 밖 클릭은 무동작)
    if (pInput->Key_Down(VK_ESCAPE)) { Close_Panel(); return OBJ_NOEVENT; }
    if (m_closeBtn.Update(tMouse, bClick)) { Close_Panel(); return OBJ_NOEVENT; }

    if (CInput_Manager::Get_Instance()->Mouse_Down(MBUTTON_L))
    {
        int iSlot = Get_InvenSlotAt(tMouse);
        if (iSlot != -1)
        {
            CItemData* pItem = m_pInven->Get_Item(iSlot);
            if (pItem && pItem->Get_Type() == ITEM_USE)
                CInput_Manager::Get_Instance()->Start_Drag(pItem, iSlot);
        }
    }

    m_iHoverInvenSlot = Get_InvenSlotAt(tMouse);
    m_iHoverEquipSlot = (int)Get_EquipSlotAt(tMouse);



    if (CInput_Manager::Get_Instance()->Key_Down(VK_RBUTTON))
    {
        int iSlot = Get_InvenSlotAt(tMouse);
        if (iSlot != -1)
        {
            On_InvenRClick(iSlot);
            return OBJ_NOEVENT;
        }

        EQUIP_SLOT eSlot = Get_EquipSlotAt(tMouse);
        if (eSlot != SLOT_END)
        {
            On_EquipRClick(eSlot);
            return OBJ_NOEVENT;
        }
    }

    return OBJ_NOEVENT;
}

void CUI_Inventory::Late_Update(float dt) {}

void CUI_Inventory::Render(ID2D1RenderTarget* pRT)
{
    if (!m_bVisible) return;

    Render_Shadow(pRT);
    Render_BackPanel(pRT);
    Render_Background(pRT);
    Render_EquipSlots(pRT);
    Render_InvenSlots(pRT);
    Render_Gold(pRT);
    m_closeBtn.Render(pRT);
    Render_Tooltip(pRT);
#ifdef GAME_DEBUG
    Debug_Render(pRT);
#endif
}

void CUI_Inventory::Release() {}

// ===================== 렌더 =====================

void CUI_Inventory::Render_Background(ID2D1RenderTarget* pRT)
{
    ID2D1Bitmap* pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"INVENTORY_FRAME");
    if (!pBitmap) return;

    float fL = (float)m_tRect.left;
    float fT = (float)m_tRect.top;

    pRT->DrawBitmap(pBitmap,
        D2D1::RectF(fL, fT, fL + PANEL_W, fT + PANEL_H),
        0.7f,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

}

void CUI_Inventory::Render_EquipSlots(ID2D1RenderTarget* pRT)
{
    float fPanelL = m_tRect.left;
    float fPanelT = m_tRect.top;

    ID2D1SolidColorBrush* pBrush = nullptr;

    for (int i = 0; i < SLOT_END; ++i)
    {
        const EQUIP_SLOT_INFO& info = s_SlotLayout[i];

        float fX = fPanelL  + info.fX;
        float fY = fPanelT  + info.fY;

        // 장착 아이템 -장비창 사이즈 그대로
        CItemData_Equipment* pEquip = m_pEquip->Get_Equipped((EQUIP_SLOT)i);
        if (pEquip)
            Render_Item(pRT, pEquip, fX, fY, info.fW, info.fH);
    }
}

void CUI_Inventory::Render_InvenSlots(ID2D1RenderTarget* pRT)
{
    float fPanelL = m_tRect.left;
    float fPanelT = m_tRect.top;

    ID2D1SolidColorBrush* pBrush = nullptr;

    for (int i = 0; i < INVEN_SIZE; ++i)
    {
        int iCol = i % INVEN_COLS;
        int iRow = i / INVEN_COLS;

        float fX = fPanelL + INVEN_OFF_X + iCol * (INVEN_SLOT_SIZE + SLOT_GAP);
        float fY = fPanelT + INVEN_OFF_Y + iRow * (INVEN_SLOT_SIZE + SLOT_GAP);

        CItemData* pItem = m_pInven->Get_Item(i);
        if (!pItem) continue;

        Render_Item(pRT, pItem, fX, fY, INVEN_SLOT_SIZE, INVEN_SLOT_SIZE);

        // 스택 수량
        if (m_pInven->Get_StackCount(i) > 1)
        {
            TCHAR szCount[8];
            swprintf_s(szCount, 8, L"%d", m_pInven->Get_StackCount(i));

            pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f), &pBrush);
            pRT->DrawText(szCount, lstrlen(szCount),
                CImg_Manager::Get_Instance()->Get_DebugFont(),
                D2D1::RectF(
                    fX,
                    fY + INVEN_SLOT_SIZE - 14.f,    // 슬롯 하단에 고정
                    fX + INVEN_SLOT_SIZE,
                    fY + INVEN_SLOT_SIZE),
                pBrush);
            pBrush->Release();
        }
    }
}

void CUI_Inventory::Render_BackPanel(ID2D1RenderTarget* pRT)
{
    ID2D1SolidColorBrush* pBrush = nullptr;

    float padding = 20.f;

    pRT->CreateSolidColorBrush(
        D2D1::ColorF(0.f, 0.f, 0.f, 0.1f), // 반투명 검정
        &pBrush);

    pRT->FillRoundedRectangle(
        D2D1::RoundedRect(
            D2D1::RectF(
                m_tRect.left - padding,
                m_tRect.top - padding,
                m_tRect.right + padding,
                m_tRect.bottom + padding),
            12.f, 12.f),
        pBrush);

    pBrush->Release();
}

void CUI_Inventory::Render_Shadow(ID2D1RenderTarget* pRT)
{
    ID2D1SolidColorBrush* pBrush = nullptr;

    pRT->CreateSolidColorBrush(
        D2D1::ColorF(0.f, 0.f, 0.f, 0.3f),
        &pBrush);

    pRT->FillRoundedRectangle(
        D2D1::RoundedRect(
            D2D1::RectF(
                m_tRect.left + 6.f,
                m_tRect.top + 6.f,
                m_tRect.right + 6.f,
                m_tRect.bottom + 6.f),
            10.f, 10.f),
        pBrush);

    pBrush->Release();
}

void CUI_Inventory::Render_Gold(ID2D1RenderTarget* pRT)
{
    float fX = m_tRect.left + 48.f * INVEN_RATE;
    float fY = m_tRect.top + 180.f * INVEN_RATE;
    float fW = INVEN_SIZE;
    float fH = INVEN_SIZE;

    ID2D1Bitmap* pBitmap =
        CImg_Manager::Get_Instance()->Find_Png(L"Gold");
    if (!pBitmap) return;
    pRT->DrawBitmap(pBitmap,
        D2D1::RectF(fX, fY, fX + fW, fY + fH),
        1.0f,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

    TCHAR szGold[32];
    swprintf_s(szGold, 32, L"%d", m_pInven->Get_Gold());

    ID2D1SolidColorBrush* pBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.2f, 1.f, 0.f), &pBrush);
    pRT->DrawText(szGold, lstrlen(szGold),
        CImg_Manager::Get_Instance()->Get_DebugFont(),
        D2D1::RectF(fX, fY + 25.f, fX + INVEN_SIZE+ 20.f, fY + 40.f),
        pBrush);
    pBrush->Release();
}

void CUI_Inventory::Render_Item(ID2D1RenderTarget* pRT, CItemData* pItem,
    float fX, float fY, float fW, float fH)
{
    ID2D1Bitmap* pBitmap =
        CImg_Manager::Get_Instance()->Find_Png(pItem->Get_IconKey());
    if (!pBitmap) return;

    pRT->DrawBitmap(pBitmap,
        D2D1::RectF(fX, fY, fX + fW, fY + fH),
        1.0f,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

// ===================== 입력 처리 =====================

void CUI_Inventory::On_InvenRClick(int iSlot)
{
    CItemData* pItem = m_pInven->Get_Item(iSlot);
    if (!pItem) return;

    if (pItem->Get_Type() == ITEM_USE)
        m_pPlayer->Use_Item(iSlot);
    else if (pItem->Get_Type() == ITEM_EQUIPMENT)
        m_pPlayer->Equip_Item(iSlot);
}

void CUI_Inventory::On_EquipRClick(EQUIP_SLOT eSlot)
{
    // 인벤토리에 빈 슬롯 있을 때만 해제
    CItemData_Equipment* pEquip = m_pEquip->Get_Equipped(eSlot);
    if (!pEquip) return;

    m_pPlayer->UnEquip_Item(eSlot);
}

// ===================== Hit Test =====================

int CUI_Inventory::Get_InvenSlotAt(POINT tMouse)
{
    float fPanelL = m_tRect.left;
    float fPanelT = m_tRect.top;

    for (int i = 0; i < INVEN_SIZE; ++i)
    {
        int iCol = i % INVEN_COLS;
        int iRow = i / INVEN_COLS;

        float fX = fPanelL + INVEN_OFF_X + iCol * (INVEN_SLOT_SIZE + SLOT_GAP);
        float fY = fPanelT + INVEN_OFF_Y + iRow * (INVEN_SLOT_SIZE + SLOT_GAP);

        RECT tSlot = {
            (LONG)fX, (LONG)fY,
            (LONG)(fX + INVEN_SLOT_SIZE), (LONG)(fY + INVEN_SLOT_SIZE)
        };

        if (PtInRect(&tSlot, tMouse))
            return i;
    }
    return -1;
}

EQUIP_SLOT CUI_Inventory::Get_EquipSlotAt(POINT tMouse)
{
    float fPanelL = m_tRect.left;
    float fPanelT = m_tRect.top;

    for (int i = 0; i < SLOT_END; ++i)
    {
        const EQUIP_SLOT_INFO& info = s_SlotLayout[i];

        float fX = fPanelL  + info.fX;
        float fY = fPanelT  + info.fY;

        RECT tSlot = {
            (LONG)fX, (LONG)fY,
            (LONG)(fX + info.fW), (LONG)(fY + info.fH)
        };

        if (PtInRect(&tSlot, tMouse))
            return (EQUIP_SLOT)i;
    }
    return SLOT_END;
}

void CUI_Inventory::Render_Tooltip(ID2D1RenderTarget* pRT)
{
    CItemData* pItem = nullptr;

    if (m_iHoverInvenSlot != -1)
        pItem = m_pInven->Get_Item(m_iHoverInvenSlot);
    else if (m_iHoverEquipSlot != (int)SLOT_END)
        pItem = m_pEquip->Get_Equipped((EQUIP_SLOT)m_iHoverEquipSlot);

    if (!pItem) return;

    // 툴팁 텍스트 구성
    TCHAR szTooltip[256] = {};

    if (pItem->Get_Type() == ITEM_EQUIPMENT)
    {
        CItemData_Equipment* pEquip = static_cast<CItemData_Equipment*>(pItem);
        swprintf_s(szTooltip, 256,
            L"%s\n공격력 +%d\n방어력 +%d",
            pItem->Get_Name(),
            pEquip->Get_AtkBonus(),
            pEquip->Get_DefBonus());
    }
    else if (pItem->Get_Type() == ITEM_USE)
    {
        swprintf_s(szTooltip, 256, L"%s\n우클릭으로 사용",
            pItem->Get_Name());
    }
    else
    {
        swprintf_s(szTooltip, 256, L"%s", pItem->Get_Name());
    }

    // 마우스 위치 기준으로 툴팁 위치 결정
    POINT tMouse = CInput_Manager::Get_Instance()->Get_MousePos();
    float fTipX = (float)tMouse.x + 30.f;
    float fTipY = (float)tMouse.y + 30.f;
    float fTipW = 140.f;
    float fTipH = 60.f;

    // 화면 밖으로 나가면 왼쪽에 표시
    if (fTipX + fTipW > WINCX)
        fTipX = tMouse.x - fTipW - 4.f;
    if (fTipY + fTipH > WINCY)
        fTipY = tMouse.y - fTipH - 4.f;

    ID2D1SolidColorBrush* pBrush = nullptr;

    // 배경 반투명 검정
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.75f), &pBrush);
    pRT->FillRoundedRectangle(
        D2D1::RoundedRect(
            D2D1::RectF(fTipX, fTipY, fTipX + fTipW, fTipY + fTipH),
            4.f, 4.f),
        pBrush);
    pBrush->Release();

    // 테두리 골드
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.7f, 0.6f, 0.1f, 0.9f), &pBrush);
    pRT->DrawRoundedRectangle(
        D2D1::RoundedRect(
            D2D1::RectF(fTipX, fTipY, fTipX + fTipW, fTipY + fTipH),
            4.f, 4.f),
        pBrush, 1.f);
    pBrush->Release();

    // 아이템 이름 흰색
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f), &pBrush);
    pRT->DrawText(pItem->Get_Name(), lstrlen(pItem->Get_Name()),
        CImg_Manager::Get_Instance()->Get_DebugFont(),
        D2D1::RectF(fTipX + 6.f, fTipY + 4.f, fTipX + fTipW - 4.f, fTipY + 18.f),
        pBrush);
    pBrush->Release();

    // 스탯 텍스트 아이템 타입별 색상
    if (pItem->Get_Type() == ITEM_EQUIPMENT)
    {
        CItemData_Equipment* pEquip = static_cast<CItemData_Equipment*>(pItem);

        // 공격력
        if (pEquip->Get_AtkBonus() > 0)
        {
            TCHAR szAtk[32];
            swprintf_s(szAtk, 32, L"공격력  +%d", pEquip->Get_AtkBonus());
            pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 0.4f, 0.4f), &pBrush);  // 빨강
            pRT->DrawText(szAtk, lstrlen(szAtk),
                CImg_Manager::Get_Instance()->Get_DebugFont(),
                D2D1::RectF(fTipX + 6.f, fTipY + 22.f, fTipX + fTipW, fTipY + 36.f),
                pBrush);
            pBrush->Release();
        }

        // 방어력
        if (pEquip->Get_DefBonus() > 0)
        {
            TCHAR szDef[32];
            swprintf_s(szDef, 32, L"방어력  +%d", pEquip->Get_DefBonus());
            pRT->CreateSolidColorBrush(D2D1::ColorF(0.4f, 0.7f, 1.f), &pBrush);  // 파랑
            pRT->DrawText(szDef, lstrlen(szDef),
                CImg_Manager::Get_Instance()->Get_DebugFont(),
                D2D1::RectF(fTipX + 6.f, fTipY + 38.f, fTipX + fTipW, fTipY + 52.f),
                pBrush);
            pBrush->Release();
        }
    }
    else if (pItem->Get_Type() == ITEM_USE)
    {
        pRT->CreateSolidColorBrush(D2D1::ColorF(0.6f, 1.f, 0.6f), &pBrush);  // 연두
        pRT->DrawText(L"우클릭으로 사용", 8,
            CImg_Manager::Get_Instance()->Get_DebugFont(),
            D2D1::RectF(fTipX + 6.f, fTipY + 22.f, fTipX + fTipW, fTipY + 36.f),
            pBrush);
        pBrush->Release();
    }
    else
    {
        pRT->CreateSolidColorBrush(D2D1::ColorF(0.8f, 0.8f, 0.8f), &pBrush);  // 회색
        pRT->DrawText(L"기타 아이템", 6,
            CImg_Manager::Get_Instance()->Get_DebugFont(),
            D2D1::RectF(fTipX + 6.f, fTipY + 22.f, fTipX + fTipW, fTipY + 36.f),
            pBrush);
        pBrush->Release();
    }
}

#ifdef GAME_DEBUG
void CUI_Inventory::Debug_Render(ID2D1RenderTarget* pRT)
{
    float fPanelL = m_tRect.left;
    float fPanelT = m_tRect.top;

    ID2D1SolidColorBrush* pBrush = nullptr;

    // ===== 장비 슬롯 빨간색 =====
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 0.f, 0.f, 0.8f), &pBrush);
    for (int i = 0; i < SLOT_END; ++i)
    {
        const EQUIP_SLOT_INFO& info = s_SlotLayout[i];
        float fX = fPanelL  + info.fX;
        float fY = fPanelT  + info.fY;

        pRT->DrawRectangle(
            D2D1::RectF(fX, fY, fX + info.fW, fY + info.fH),
            pBrush, 1.5f);

        // 슬롯 이름 표시
        static const TCHAR* s_SlotName[SLOT_END] = {
            L"WEAPON", L"ARMOR", L"SHIELD", L"HELMET", L"PENDANT", L"RING"
        };
        pRT->DrawText(s_SlotName[i], lstrlen(s_SlotName[i]),
            CImg_Manager::Get_Instance()->Get_DebugFont(),
            D2D1::RectF(fX, fY, fX + info.fW, fY + 14.f),
            pBrush);
    }
    pBrush->Release();

    // ===== 인벤토리 슬롯 초록색 =====
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 1.f, 0.f, 0.8f), &pBrush);
    for (int i = 0; i < INVEN_SIZE; ++i)
    {
        int iCol = i % INVEN_COLS;
        int iRow = i / INVEN_COLS;

        float fX = fPanelL + INVEN_OFF_X + iCol * (INVEN_SLOT_SIZE + SLOT_GAP);
        float fY = fPanelT + INVEN_OFF_Y + iRow * (INVEN_SLOT_SIZE + SLOT_GAP);

        pRT->DrawRectangle(
            D2D1::RectF(fX, fY, fX + INVEN_SLOT_SIZE, fY + INVEN_SLOT_SIZE),
            pBrush, 1.5f);

        // 슬롯 번호
        TCHAR szNum[4];
        swprintf_s(szNum, 4, L"%d", i);
        pRT->DrawText(szNum, lstrlen(szNum),
            CImg_Manager::Get_Instance()->Get_DebugFont(),
            D2D1::RectF(fX, fY, fX + INVEN_SLOT_SIZE, fY + INVEN_SLOT_SIZE),
            pBrush);
    }
    pBrush->Release();

    // ===== 패널 전체 외곽선 노란색 =====
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 0.f, 0.8f), &pBrush);
    pRT->DrawRectangle(
        D2D1::RectF(fPanelL, fPanelT, fPanelL + PANEL_W, fPanelT + PANEL_H),
        pBrush, 2.f);
    pBrush->Release();
}
#endif