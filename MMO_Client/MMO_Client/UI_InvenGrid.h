#pragma once
#include "define.h"

class CInventory;
class CItemData;

// ================================================================
//  CUI_InvenGrid  재사용 인벤토리 그리드 위젯
//  - 인벤 슬롯을 골라야 하는 모든 UI 공용: 
//      1.상점 판매 
//      2.경매장 등록
// ================================================================
class CUI_InvenGrid
{
public:
    void Set_Reference(CInventory* pInven) { m_pInven = pInven; }

    // 좌상단 기준 배치. cols=열 수, slot=칸 크기, gap=칸 간격.
    void Set_Layout(float fLeft, float fTop, int iCols, float fSlot, float fGap)
    {
        m_fLeft = fLeft; m_fTop = fTop;
        m_iCols = (iCols > 0) ? iCols : 1;
        m_fSlot = fSlot;  m_fGap = fGap;
    }

    // 마우스/좌클릭 처리. 아이템이 있는 슬롯을 클릭하면 그 인덱스, 아니면 -1.
    int  Update(POINT tMouse, bool bLeftClick);
    void Render(ID2D1RenderTarget* pRT);

    int         Get_HoverSlot() const { return m_iHoverSlot; }
    CItemData*  Get_HoverItem() const;

private:
    void Slot_Pos(int iSlot, float& fX, float& fY) const;
    int  Slot_At(POINT tMouse) const;

private:
    CInventory* m_pInven = nullptr;
    float m_fLeft = 0.f;
    float m_fTop  = 0.f;
    int   m_iCols = 5;
    float m_fSlot = 44.f;
    float m_fGap  = 4.f;
    int   m_iHoverSlot = -1;
};
