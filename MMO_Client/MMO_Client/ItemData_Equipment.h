#pragma once
#include "ItemData.h"



class CItemData_Equipment : public CItemData
{
public:
    CItemData_Equipment() = default;
    virtual ~CItemData_Equipment() = default;

    void            Set_EquipType(EQUIPMENT_TYPE eType);
    EQUIPMENT_TYPE  Get_EquipType()  const { return m_eEquipType; }
    EQUIP_SLOT      Get_EquipSlot()  const { return m_eSlot; }
    int             Get_AtkBonus()   const { return m_iAtkBonus; }
    int             Get_DefBonus()   const { return m_iDefBonus; }

    // 고유 코드 = 3000 + 장비 서브타입 (서버/경매장/판매가 산정 공용)
    virtual int     Get_ItemCode() const override { return 3000 + (int)m_eEquipType; }

private:
    EQUIPMENT_TYPE  m_eEquipType = EQUIP_TYPE_END;
    EQUIP_SLOT      m_eSlot = SLOT_END;
    int             m_iAtkBonus = 0;
    int             m_iDefBonus = 0;
};