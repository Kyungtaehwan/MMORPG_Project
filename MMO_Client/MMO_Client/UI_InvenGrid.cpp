#include "pch.h"
#include "UI_InvenGrid.h"
#include "Inventory.h"
#include "ItemData.h"
#include "Img_Manager.h"

// 슬롯 i의 좌상단 좌표
void CUI_InvenGrid::Slot_Pos(int iSlot, float& fX, float& fY) const
{
    int iCol = iSlot % m_iCols;
    int iRow = iSlot / m_iCols;
    fX = m_fLeft + iCol * (m_fSlot + m_fGap);
    fY = m_fTop  + iRow * (m_fSlot + m_fGap);
}

// 마우스가 올라간 슬롯 인덱스 (없으면 -1)
int CUI_InvenGrid::Slot_At(POINT tMouse) const
{
    for (int i = 0; i < INVEN_SIZE; ++i)
    {
        float fX, fY;
        Slot_Pos(i, fX, fY);
        RECT r = { (LONG)fX, (LONG)fY, (LONG)(fX + m_fSlot), (LONG)(fY + m_fSlot) };
        if (PtInRect(&r, tMouse))
            return i;
    }
    return -1;
}

CItemData* CUI_InvenGrid::Get_HoverItem() const
{
    if (!m_pInven || m_iHoverSlot < 0) return nullptr;
    return m_pInven->Get_Item(m_iHoverSlot);
}

int CUI_InvenGrid::Update(POINT tMouse, bool bLeftClick)
{
    m_iHoverSlot = Slot_At(tMouse);

    // 아이템이 실제로 든 슬롯을 클릭했을 때만 인덱스 반환
    if (bLeftClick && m_iHoverSlot != -1 && m_pInven &&
        m_pInven->Get_Item(m_iHoverSlot))
        return m_iHoverSlot;

    return -1;
}

void CUI_InvenGrid::Render(ID2D1RenderTarget* pRT)
{
    if (!m_pInven) return;

    CImg_Manager* pImg = CImg_Manager::Get_Instance();
    IDWriteTextFormat* pFont = pImg->Get_DebugFont();
    ID2D1SolidColorBrush* pBrush = nullptr;

    for (int i = 0; i < INVEN_SIZE; ++i)
    {
        float fX, fY;
        Slot_Pos(i, fX, fY);
        D2D1_RECT_F rc = D2D1::RectF(fX, fY, fX + m_fSlot, fY + m_fSlot);

        // 슬롯 배경 (반투명), 호버 시 밝게
        D2D1::ColorF bg = (i == m_iHoverSlot)
            ? D2D1::ColorF(1.f, 1.f, 1.f, 0.22f)
            : D2D1::ColorF(0.f, 0.f, 0.f, 0.28f);
        pRT->CreateSolidColorBrush(bg, &pBrush);
        pRT->FillRoundedRectangle(D2D1::RoundedRect(rc, 5.f, 5.f), pBrush);
        pBrush->Release();

        CItemData* pItem = m_pInven->Get_Item(i);
        if (!pItem) continue;

        // 아이콘
        ID2D1Bitmap* pIcon = pImg->Find_Png(pItem->Get_IconKey());
        if (pIcon)
            pRT->DrawBitmap(pIcon, rc, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

        // 스택 수량
        int iCount = m_pInven->Get_StackCount(i);
        if (iCount > 1)
        {
            TCHAR szCount[8];
            swprintf_s(szCount, 8, L"%d", iCount);
            pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f), &pBrush);
            pRT->DrawText(szCount, (UINT32)wcslen(szCount), pFont,
                D2D1::RectF(fX, fY + m_fSlot - 15.f, fX + m_fSlot, fY + m_fSlot),
                pBrush);
            pBrush->Release();
        }
    }
}
