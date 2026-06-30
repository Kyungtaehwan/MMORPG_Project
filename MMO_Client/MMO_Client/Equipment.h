#pragma once
#include "Item_Define.h"

class CItemData_Equipment;

class CEquipment
{
public:
    CEquipment() = default;
    ~CEquipment();

    CItemData_Equipment* Equip(EQUIP_SLOT eSlot, CItemData_Equipment* pItem);
    CItemData_Equipment* UnEquip(EQUIP_SLOT eSlot);
    CItemData_Equipment* Get_Equipped(EQUIP_SLOT eSlot) const;

    int     Get_TotalAtk() const;
    int     Get_TotalDef() const;

    // 서버 스냅샷(슬롯별 itemCode)으로 장비창 재구성
    void    Set_From_Snapshot(const int* pEquipCodes);

private:
    CItemData_Equipment* m_aSlot[SLOT_END] = {};
};