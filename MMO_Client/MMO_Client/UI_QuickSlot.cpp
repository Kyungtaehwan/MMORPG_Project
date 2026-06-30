#include "pch.h"
#include "UI_QuickSlot.h"
#include "Player.h"
#include "Inventory.h"
#include "ItemData.h"
#include "ItemData_UseItem.h"
#include "Img_Manager.h"
#include "Input_Manager.h"

static const TCHAR* s_KeyLabel[8] = {
    L"1", L"2", L"3", L"4", L"Q", L"W", L"E", L"R"
};

void CUI_QuickSlot::Initialize()
{
    CImg_Manager::Get_Instance()->Insert_Png(
        L"../Resource/UI/Quick.png", L"QUICKSLOT_FRAME");

    // 화면 우측 하단 고정
    Set_Pos(WINCX - PANEL_W * 0.5f, WINCY - PANEL_H * 0.5f);
    Set_Size(PANEL_W, PANEL_H);
    Update_Rect();
}

int CUI_QuickSlot::Update(float dt)
{
    CInput_Manager* pInput = CInput_Manager::Get_Instance();
    POINT tMouse = pInput->Get_MousePos();

    // 드래그 중 마우스 뗌 퀵슬롯 영역 체크 후 드롭
    if (pInput->Is_Dragging() && pInput->Mouse_Up(MBUTTON_L))
    {
        On_LButtonUp(tMouse);
        return OBJ_NOEVENT;
    }

    // 퀵슬롯 우클릭 슬롯 해제 (퀵슬롯 영역일 때만)
    if (pInput->Mouse_Down(MBUTTON_R))
    {
        int iSlot = Get_SlotAt(tMouse);
        if (iSlot != -1)
        {
            On_RClick(tMouse);
            return OBJ_NOEVENT;
        }
    }

    // 단축키 사용 (1,2,3,4) 게임모드일 때만
    if (pInput->Is_GameMode())
    {
        int aKeys[4] = { '1', '2', '3', '4' };
        for (int i = 0; i < 4; ++i)
        {
            if (pInput->Key_Down(aKeys[i]))
            {
                if (m_aSlotCode[i] != 0)
                    m_pPlayer->Use_QuickSlot_ByCode(m_aSlotCode[i]);
                break;
            }
        }
    }
    Update_SlotValidity();
    return OBJ_NOEVENT;
}

void CUI_QuickSlot::Late_Update(float dt) {}

void CUI_QuickSlot::Render(ID2D1RenderTarget* pRT)
{
    Render_Slots(pRT);
    Render_DragIcon(pRT);
#ifdef GAME_DEBUG
    Debug_Render(pRT);
#endif
}

void CUI_QuickSlot::Release() {}

// ===================== 렌더 =====================

void CUI_QuickSlot::Render_Slots(ID2D1RenderTarget* pRT)
{
    ID2D1Bitmap* pFrame = CImg_Manager::Get_Instance()->Find_Png(L"QUICKSLOT_FRAME");
    if (pFrame)
    {
        pRT->DrawBitmap(pFrame,
            D2D1::RectF(
                (float)m_tRect.left, (float)m_tRect.top-18.f,
                (float)m_tRect.right, (float)m_tRect.bottom),
            1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    }

    ID2D1SolidColorBrush* pBrush = nullptr;

    for (int i = 0; i < 8; ++i)
    {
        float fX = m_tRect.left + i * SLOT_SIZE;
        float fY = (float)m_tRect.top;

        // 등록된 코드의 아이템을 인벤에서 찾아 표시
        if (m_aSlotCode[i] != 0)
        {
            CItemData* pItem = nullptr;
            int iCount = 0;
            for (int j = 0; j < INVEN_SIZE; ++j)
            {
                CItemData* pIt = m_pPlayer->Get_Inventory()->Get_Item(j);
                if (pIt && pIt->Get_ItemCode() == m_aSlotCode[i])
                {
                    pItem = pIt;
                    iCount = m_pPlayer->Get_Inventory()->Get_StackCount(j);
                    break;
                }
            }

            if (pItem)
            {
                ID2D1Bitmap* pBitmap = CImg_Manager::Get_Instance()->Find_Png(
                    pItem->Get_IconKey());
                if (pBitmap)
                    pRT->DrawBitmap(pBitmap,
                        D2D1::RectF(
                            fX + 6.f, fY + 6.f,
                            fX + SLOT_SIZE - 6.f, fY + SLOT_SIZE - 6.f),
                        1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            }

            if (iCount > 0)
            {
                TCHAR szCount[8];
                swprintf_s(szCount, 8, L"%d", iCount);

                ID2D1SolidColorBrush* pBrush = nullptr;
                pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f), &pBrush);
                IDWriteTextFormat* pFont = CImg_Manager::Get_Instance()->Get_DebugFont();
                pFont->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);  // 우측 정렬
                pRT->DrawText(szCount, lstrlen(szCount),
                    pFont,
                    D2D1::RectF(fX-30.f, fY + SLOT_SIZE - 2
                        ,
                        fX + SLOT_SIZE - 10.f,   // 오른쪽 여백 2px
                        fY + SLOT_SIZE - 15.f),
                    pBrush);

                // 원래대로 복구
                pFont->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                pBrush->Release();
            }
        }

    }
}

void CUI_QuickSlot::Render_DragIcon(ID2D1RenderTarget* pRT)
{
    CInput_Manager* pInput = CInput_Manager::Get_Instance();
    if (!pInput->Is_Dragging()) return;

    CItemData* pItem = pInput->Get_Drag().pItem;
    if (!pItem) return;

    ID2D1Bitmap* pBitmap = CImg_Manager::Get_Instance()->Find_Png(pItem->Get_IconKey());
    if (!pBitmap) return;

    POINT tMouse = pInput->Get_MousePos();
    float fHalf = INVEN_SLOT_SIZE * 0.5f;

    // 반투명으로 마우스 따라다님
    pRT->DrawBitmap(pBitmap,
        D2D1::RectF(
            tMouse.x - fHalf, tMouse.y - fHalf,
            tMouse.x + fHalf, tMouse.y + fHalf),
        0.7f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

// ===================== 입력 처리 =====================

void CUI_QuickSlot::On_LButtonUp(POINT tMouse)
{
    CInput_Manager* pInput = CInput_Manager::Get_Instance();
    int iSlot = Get_SlotAt(tMouse);

    if (iSlot != -1 && Is_UseSlot(iSlot))
    {
        CItemData* pItem = pInput->Get_Drag().pItem;
        if (pItem && pItem->Get_Type() == ITEM_USE)
        {
            // 포인터가 아니라 아이템 코드를 등록 (인벤 재구성에도 유지)
            m_aSlotCode[iSlot] = pItem->Get_ItemCode();
        }
    }

    pInput->End_Drag();
}

void CUI_QuickSlot::On_RClick(POINT tMouse)
{
    int iSlot = Get_SlotAt(tMouse);
    if (iSlot == -1 || !Is_UseSlot(iSlot)) return;

    m_aSlotCode[iSlot] = 0;
}

int CUI_QuickSlot::Get_SlotAt(POINT tMouse)
{
    for (int i = 0; i < 8; ++i)
    {
        float fX = m_tRect.left + i * SLOT_SIZE;
        float fY = (float)m_tRect.top;

        RECT tSlot = {
            (LONG)fX, (LONG)fY,
            (LONG)(fX + SLOT_SIZE), (LONG)(fY + SLOT_SIZE)
        };

        if (PtInRect(&tSlot, tMouse))
            return i;
    }
    return -1;
}

void CUI_QuickSlot::Update_SlotValidity()
{
    // 등록한 코드의 아이템이 인벤에 하나도 없으면 슬롯 비움
    for (int i = 0; i < 4; ++i)
    {
        if (m_aSlotCode[i] == 0) continue;

        bool bFound = false;
        for (int j = 0; j < INVEN_SIZE; ++j)
        {
            CItemData* pIt = m_pPlayer->Get_Inventory()->Get_Item(j);
            if (pIt && pIt->Get_ItemCode() == m_aSlotCode[i])
            {
                bFound = true;
                break;
            }
        }

        if (!bFound)
            m_aSlotCode[i] = 0;
    }
}

#ifdef GAME_DEBUG
void CUI_QuickSlot::Debug_Render(ID2D1RenderTarget* pRT)
{
    ID2D1SolidColorBrush* pBrush = nullptr;

    for (int i = 0; i < 8; ++i)
    {
        float fX = m_tRect.left + i * SLOT_SIZE;
        float fY = (float)m_tRect.top;

        // 소비 슬롯(0~3)노란색, 스킬 슬롯(4~7)하늘색
        D2D1_COLOR_F color = (i < 4)
            ? D2D1::ColorF(1.f, 1.f, 0.f, 0.8f)
            : D2D1::ColorF(0.4f, 0.8f, 1.f, 0.8f);

        pRT->CreateSolidColorBrush(color, &pBrush);
        pRT->DrawRectangle(
            D2D1::RectF(fX, fY, fX + SLOT_SIZE, fY + SLOT_SIZE),
            pBrush, 1.5f);

        // 슬롯 번호
        TCHAR szNum[4];
        swprintf_s(szNum, 4, L"%d", i);
        pRT->DrawText(szNum, lstrlen(szNum),
            CImg_Manager::Get_Instance()->Get_DebugFont(),
            D2D1::RectF(fX + 2.f, fY + 2.f, fX + 20.f, fY + 16.f),
            pBrush);
        pBrush->Release();

        // 아이템 있는 슬롯은 초록 테두리
        if (m_aSlotCode[i] != 0)
        {
            pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 1.f, 0.f, 0.9f), &pBrush);
            pRT->DrawRectangle(
                D2D1::RectF(fX + 2.f, fY + 2.f,
                    fX + SLOT_SIZE - 2.f, fY + SLOT_SIZE - 2.f),
                pBrush, 2.f);
            pBrush->Release();
        }
    }

    // 전체 패널 외곽선 흰색
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f, 0.6f), &pBrush);
    pRT->DrawRectangle(
        D2D1::RectF(
            (float)m_tRect.left, (float)m_tRect.top,
            (float)m_tRect.right, (float)m_tRect.bottom),
        pBrush, 1.f);
    pBrush->Release();
}
#endif