#pragma once
#include "UI.h"

class CUI_Manager
{
private:
	CUI_Manager();
	~CUI_Manager();

public:
	void		Add_UI(UI_ID eID, CUI* pUI);
	int			Update(float dt);
	void		Late_Update(float dt);
	void		Render(ID2D1RenderTarget* pRT);
	void		Release(void);

public:

	std::list<CUI*>* Get_List(UI_ID eID) {
		return &m_UIList[eID];
	}

	bool List_Empty(UI_ID eID) {
		if (m_UIList[eID].empty() == true) {
			return true;
		}
		else {
			return false;
		}

	};

	void		DeleteID(UI_ID eID);
	void		On_Char(wchar_t ch);
	void		Open_Shop(int iShopType);   // NPC_Shop 클릭 시 상점 UI 열기
	void		Open_Auction();             // NPC_Market 클릭 시 경매장 UI 열기
	void		Open_ZoneSelect();          // NPC_Angel 클릭 시 대형 맵 선택창 열기

private:

	std::list<CUI*>		m_UIList[UI_END];

public:
	static	CUI_Manager* Get_Instance()
	{
		if (!m_pInstance)
		{
			m_pInstance = new CUI_Manager;
		}
		return m_pInstance;
	}

	static void			Destroy_Instance()
	{
		if (m_pInstance)
		{
			delete m_pInstance;
			m_pInstance = nullptr;
		}
	}
private:
	static	CUI_Manager* m_pInstance;


};

