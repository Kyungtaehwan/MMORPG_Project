#include "pch.h"
#include "Equipment.h"
#include "ItemData_Equipment.h"
#include "Inventory.h"  

CEquipment::~CEquipment()
{
    for (int i = 0; i < SLOT_END; ++i)
    {
        if (m_aSlot[i])
        {
            delete m_aSlot[i];
            m_aSlot[i] = nullptr;
        }
    }
}

CItemData_Equipment* CEquipment::Equip(EQUIP_SLOT eSlot, CItemData_Equipment* pItem)
{
    CItemData_Equipment* pPrev = m_aSlot[eSlot];
    m_aSlot[eSlot] = pItem;
    return pPrev;
}

CItemData_Equipment* CEquipment::UnEquip(EQUIP_SLOT eSlot)
{
    CItemData_Equipment* pItem = m_aSlot[eSlot];
    m_aSlot[eSlot] = nullptr;
    return pItem; 
}

CItemData_Equipment* CEquipment::Get_Equipped(EQUIP_SLOT eSlot) const
{
    return m_aSlot[eSlot];
}

int CEquipment::Get_TotalAtk() const
{
    int iTotal = 0;
    for (int i = 0; i < SLOT_END; ++i)
        if (m_aSlot[i])
            iTotal += m_aSlot[i]->Get_AtkBonus();
    return iTotal;
}

int CEquipment::Get_TotalDef() const
{
    int iTotal = 0;
    for (int i = 0; i < SLOT_END; ++i)
        if (m_aSlot[i])
            iTotal += m_aSlot[i]->Get_DefBonus();
    return iTotal;
}

// 서버 스냅샷(슬롯별 itemCode)으로 장비창 통째 재구성
void CEquipment::Set_From_Snapshot(const int* pEquipCodes)
{
    for (int i = 0; i < SLOT_END; ++i)
    {
        if (m_aSlot[i]) { delete m_aSlot[i]; m_aSlot[i] = nullptr; }
    }
    for (int i = 0; i < SLOT_END; ++i)
    {
        if (pEquipCodes[i] == 0) continue;
        CItemData* pItem = CInventory::Create_ItemFromCode(pEquipCodes[i]);
        CItemData_Equipment* pEquip = dynamic_cast<CItemData_Equipment*>(pItem);
        if (pEquip) m_aSlot[i] = pEquip;
        else        delete pItem;
    }
}