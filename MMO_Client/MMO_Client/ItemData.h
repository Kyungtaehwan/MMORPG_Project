#pragma once
#include "Item_Define.h"

class CItemData
{
public:
    virtual ~CItemData() = default;

    const TCHAR* Get_Name()    const { return m_szName; }
    const TCHAR* Get_IconKey() const { return m_szIconKey; }
    ITEM_TYPE       Get_Type()    const { return m_eType; }

    // 아이템 고유 코드 (category*1000+subType). 퀵슬롯/서버 동기화용.
    virtual int     Get_ItemCode() const { return -1; }

protected:
    TCHAR       m_szName[64] = {};
    TCHAR       m_szIconKey[64] = {};
    ITEM_TYPE   m_eType = ITEM_END;
};

