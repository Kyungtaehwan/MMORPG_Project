#include "pch.h"
#include "Inventory.h"
#include "ItemData.h"
#include "ItemData_Potion.h"
#include "ItemData_Scroll.h"
#include "ItemData_Equipment.h"
#include "ItemData_Etc.h"

CInventory::~CInventory()
{
    for (int i = 0; i < INVEN_SIZE; ++i)
    {
        if (m_aSlot[i])
        {
            delete m_aSlot[i];
            m_aSlot[i] = nullptr;
        }
    }
}

bool CInventory::Add_Item(CItemData* pItem)
{
    if (pItem->Get_Type() == ITEM_EQUIPMENT)
    {
        int iSlot = Find_EmptySlot();
        if (iSlot == INVEN_FULL) return false;

        m_aSlot[iSlot] = pItem;
        m_iStackCount[iSlot] = 1;
        return true;
    }

    int iSameSlot = Find_SameItem(pItem);
    if (iSameSlot != ITEM_NOEXIST)
    {
        ++m_iStackCount[iSameSlot];
        delete pItem;
        return true;
    }

    int iEmptySlot = Find_EmptySlot();
    if (iEmptySlot == INVEN_FULL) return false;

    m_aSlot[iEmptySlot] = pItem;
    m_iStackCount[iEmptySlot] = 1;
    return true;
}

CItemData* CInventory::Remove_Item(int iSlot)
{
    if (iSlot < 0 || iSlot >= INVEN_SIZE) return nullptr;
    if (!m_aSlot[iSlot]) return nullptr;

    if (m_iStackCount[iSlot] > 1)
    {
        --m_iStackCount[iSlot];
        return nullptr;
    }

    CItemData* pItem = m_aSlot[iSlot];
    m_aSlot[iSlot] = nullptr;
    m_iStackCount[iSlot] = 0;
    return pItem;
}

CItemData* CInventory::Get_Item(int iSlot) const
{
    if (iSlot < 0 || iSlot >= INVEN_SIZE) return nullptr;
    return m_aSlot[iSlot];
}

bool CInventory::Is_Empty(int iSlot) const
{
    return m_aSlot[iSlot] == nullptr;
}

int CInventory::Find_SameItem(CItemData* pItem)
{
    for (int i = 0; i < INVEN_SIZE; ++i)
    {
        if (!m_aSlot[i]) continue;
        if (m_aSlot[i]->Get_Type() == pItem->Get_Type() &&
            lstrcmp(m_aSlot[i]->Get_Name(), pItem->Get_Name()) == 0 &&
            m_iStackCount[i] < STACK_FULL)
            return i;
    }
    return ITEM_NOEXIST;
}

int CInventory::Find_EmptySlot()
{
    for (int i = 0; i < INVEN_SIZE; ++i)
        if (!m_aSlot[i]) return i;
    return INVEN_FULL;
}

void CInventory::Add_Gold(int iAmount)
{
    m_iGold += iAmount;
}

bool CInventory::Spend_Gold(int iAmount)
{
    if (m_iGold < iAmount) return false;
    m_iGold -= iAmount;
    return true;
}

// 고유 코드(category*1000+subType) → CItemData 생성
CItemData* CInventory::Create_ItemFromCode(int iCode)
{
    int iCat = iCode / 1000;
    int iSub = iCode % 1000;

    switch (iCat)
    {
    case 1:
    {
        CItemData_Potion* p = new CItemData_Potion;
        p->Set_PotionType(static_cast<POTION_TYPE>(iSub));
        return p;
    }
    case 2:
    {
        CItemData_Scroll* p = new CItemData_Scroll;
        p->Set_ScrollType(static_cast<SCROLL_TYPE>(iSub));
        return p;
    }
    case 3:
    {
        CItemData_Equipment* p = new CItemData_Equipment;
        p->Set_EquipType(static_cast<EQUIPMENT_TYPE>(iSub));
        return p;
    }
    case 4:
    {
        CItemData_Etc* p = new CItemData_Etc;
        p->Set_EtcType(static_cast<ETC_TYPE>(iSub));
        return p;
    }
    default:
        return nullptr;  // 골드(9000) 등은 인벤 슬롯이 아님
    }
}

// 서버 인벤 스냅샷으로 통째로 재구성 (slot 대 slot)
void CInventory::Set_From_Snapshot(const int* pCodes, const int* pCounts, int iGold)
{
    for (int i = 0; i < INVEN_SIZE; ++i)
    {
        if (m_aSlot[i]) { delete m_aSlot[i]; m_aSlot[i] = nullptr; }
        m_iStackCount[i] = 0;
    }

    for (int i = 0; i < INVEN_SIZE; ++i)
    {
        if (pCodes[i] == 0) continue;
        CItemData* pItem = Create_ItemFromCode(pCodes[i]);
        if (!pItem) continue;
        m_aSlot[i] = pItem;
        m_iStackCount[i] = (pCounts[i] > 0) ? pCounts[i] : 1;
    }

    m_iGold = iGold;
}