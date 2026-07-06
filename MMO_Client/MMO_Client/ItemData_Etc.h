#pragma once
#include "ItemData.h"

class CItemData_Etc : public CItemData
{
public:
	CItemData_Etc() = default;
	virtual ~CItemData_Etc() = default;

	void        Set_EtcType(ETC_TYPE eType);
	ETC_TYPE    Get_EtcType() const { return m_eEtcType; }

	// 고유 코드 = 4000 + 기타 서브타입 (서버/경매장/판매가 산정 공용)
	virtual int Get_ItemCode() const override { return 4000 + (int)m_eEtcType; }

private:
	ETC_TYPE    m_eEtcType = ETC_END;
};

