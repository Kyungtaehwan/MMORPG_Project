#pragma once
#include "ItemData_UseItem.h"




class CItemData_Potion : public CItemData_UseItem
{
public:
    CItemData_Potion() = default;
    virtual ~CItemData_Potion() = default;

    void            Set_PotionType(POTION_TYPE eType);
    POTION_TYPE     Get_PotionType() const { return m_ePotionType; }
    virtual int     Get_ItemCode() const override { return 1000 + (int)m_ePotionType; }
    virtual void    Use_Item(CPlayer* pPlayer)          override;

private:

    POTION_TYPE     m_ePotionType = POTION_END;
    int             m_iAmount = 0;
};