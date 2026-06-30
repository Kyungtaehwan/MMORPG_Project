#pragma once
#include "Item_Define.h"

class CItemData;

class CInventory
{
public:
    CInventory() = default;
    ~CInventory();

public:
    // 아이템
    bool            Add_Item(CItemData* pItem);         // 빈 슬롯 or 스택에 추가
    CItemData*      Remove_Item(int iSlot);             // 슬롯에서 꺼냄 (소유권 반환)
    CItemData*      Get_Item(int iSlot) const;          // 슬롯 참조
    bool            Is_Empty(int iSlot) const;
    int             Get_SlotCount() const { return INVEN_SIZE; }
    int             Get_StackCount(int iSlot) const { return m_iStackCount[iSlot]; }
    void            Decrease_Stack(int iSlot)
    {
        if (m_iStackCount[iSlot] > 0)
            --m_iStackCount[iSlot];
    }

    // 골드
    void            Add_Gold(int iAmount);
    bool            Spend_Gold(int iAmount);            // 돈 부족하면 false
    int             Get_Gold() const { return m_iGold; }

public:
    int             Find_SameItem(CItemData* pItem);    // 스택 가능한 슬롯 탐색
    int             Find_EmptySlot();                   // 빈 슬롯 탐색

    // ---- 서버 동기화 (서버 단일 진실) ----
    // 고유 코드(category*1000+subType) → CItemData 생성
    static CItemData* Create_ItemFromCode(int iCode);
    // 서버 인벤 스냅샷으로 통째로 재구성 (slot 대 slot)
    void            Set_From_Snapshot(const int* pCodes, const int* pCounts, int iGold);
    void            Set_Gold(int iGold) { m_iGold = iGold; }

private:
    CItemData* m_aSlot[INVEN_SIZE] = {};
    int             m_iStackCount[INVEN_SIZE] = {};     // 슬롯별 수량
    int             m_iGold = 0;
};