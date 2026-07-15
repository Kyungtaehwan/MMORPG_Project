#include "pch.h"
#include "UI_Manager.h"
#include "Img_Manager.h"
#include "UI_LoginBox.h"
#include "UI_Shop.h"
#include "UI_Auction.h"
#include "UI_ZoneSelect.h"

CUI_Manager* CUI_Manager::m_pInstance = nullptr;

CUI_Manager::CUI_Manager()
{

}

CUI_Manager::~CUI_Manager()
{
	Release();
}

void CUI_Manager::Add_UI(UI_ID eID, CUI* pUI)
{
	if (UI_END <= eID || nullptr == pUI)
		return;

	m_UIList[eID].push_back(pUI);
}

int CUI_Manager::Update(float dt)
{
	for (size_t i = 0; i < UI_END; ++i)
	{
		for (auto iter = m_UIList[i].begin();
			iter != m_UIList[i].end(); )
		{
			int iResult = (*iter)->Update(dt);

			if (UI_EVENT == iResult)
			{
				(*iter)->Process_Event();
				Safe_Delete<CUI*>(*iter);
				iter = m_UIList[i].erase(iter);
			}
			else
				++iter;
		}
	}

	return OBJ_NOEVENT;
}

void CUI_Manager::Late_Update(float dt)
{
	for (size_t i = 0; i < UI_END; ++i)
	{
		for (auto& iter : m_UIList[i])
		{
			iter->Late_Update(dt);

			if (m_UIList[i].empty())
				break;

		}
	}

}

void CUI_Manager::Render(ID2D1RenderTarget* pRT)
{
	for (size_t i = 0; i < UI_END; ++i)
	{
		for (auto& iter : m_UIList[i])
		{
			iter->Render(pRT);
		}
	}
}

void CUI_Manager::On_Char(wchar_t ch)
{
	for (auto* pUI : m_UIList[UI_BOX])
	{
		auto* pLoginBox = dynamic_cast<CUI_LoginBox*>(pUI);
		if (pLoginBox)
		{
			pLoginBox->On_Char(ch);
			break;
		}
	}

	// 경매장 검색/숫자 입력
	for (auto* pUI : m_UIList[UI_AUCTION])
	{
		auto* pAuc = dynamic_cast<CUI_Auction*>(pUI);
		if (pAuc && pAuc->Is_Open())
		{
			pAuc->On_Char(ch);
			break;
		}
	}
}

void CUI_Manager::Release(void)
{

	for (size_t i = 0; i < UI_END; ++i)
	{
		for_each(m_UIList[i].begin(), m_UIList[i].end(), Safe_Delete<CUI*>);
		m_UIList[i].clear();
	}
}

void CUI_Manager::Open_Shop(int iShopType)
{
	for (auto* pUI : m_UIList[UI_SHOP])
	{
		auto* pShop = dynamic_cast<CUI_Shop*>(pUI);
		if (pShop)
		{
			pShop->Open(iShopType);
			break;
		}
	}
}

void CUI_Manager::Open_Auction()
{
	for (auto* pUI : m_UIList[UI_AUCTION])
	{
		auto* pAuc = dynamic_cast<CUI_Auction*>(pUI);
		if (pAuc)
		{
			pAuc->Open();
			break;
		}
	}
}

void CUI_Manager::Open_ZoneSelect()
{
	for (auto* pUI : m_UIList[UI_ZONESELECT])
	{
		auto* pSel = dynamic_cast<CUI_ZoneSelect*>(pUI);
		if (pSel)
		{
			pSel->Open();
			break;
		}
	}
}

void CUI_Manager::DeleteID(UI_ID eID)
{
	for (auto& iter : m_UIList[eID])
		Safe_Delete(iter);

	m_UIList[eID].clear();
}
