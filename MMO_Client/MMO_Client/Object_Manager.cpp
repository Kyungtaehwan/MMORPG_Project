#include "pch.h"
#include "Object_Manager.h"
#include "Camera.h"
#include "Collision_Manager.h"
#include "Other_Player.h"
#include "Monster.h"
#include "Monster_Orc.h"
CObject_Manager* CObject_Manager::m_pInstance = nullptr;

CObject_Manager::CObject_Manager()
{
}

CObject_Manager::~CObject_Manager()
{
	Release();
}

void CObject_Manager::Add_Object(OBJ_ID eID, CGameObject* pGameObject)
{
	if (OBJ_END <= eID || nullptr == pGameObject)
		return;

	m_ObjectList[eID].push_back(pGameObject);
}

int CObject_Manager::Update(float dt)
{
	for (size_t i = 0; i < OBJ_END; ++i)
	{
		for (auto iter = m_ObjectList[i].begin();
			iter != m_ObjectList[i].end(); )
		{
			int iResult = (*iter)->Update(dt);

			if (OBJ_DEAD == iResult)
			{
				Safe_Delete<CGameObject*>(*iter);
				iter = m_ObjectList[i].erase(iter);
			}
			else
				++iter;
		}
	}

	CCollision_Manager::Get_Instance()->Collision_RectEx(
		m_ObjectList[OBJ_PLAYER],
		m_ObjectList[OBJ_NPC]
	);
	CCollision_Manager::Get_Instance()->Collision_RectEx(
		m_ObjectList[OBJ_PLAYER],
		m_ObjectList[OBJ_PORTAL]
	);
	return OBJ_NOEVENT;
}

void CObject_Manager::Late_Update(float dt)
{
	for (size_t i = 0; i < OBJ_END; ++i)
	{
		for (auto& iter : m_ObjectList[i])
		{
			iter->Late_Update(dt);

			if (m_ObjectList[i].empty())
				break;

		}
	}

}

void CObject_Manager::Render(ID2D1RenderTarget* pRT)
{
	std::vector<CGameObject*> vecSortList;

	// OBJ_END 전까지 모든 리스트를 다 넣기
	for (size_t i = 0; i < OBJ_END; ++i)
	{
		for (auto& pObj : m_ObjectList[i])
			vecSortList.push_back(pObj);
	}

	// Y소팅
	sort(vecSortList.begin(), vecSortList.end(),
		[](CGameObject* pA, CGameObject* pB)
		{
			return (pA->Get_IsoInfo().fWorldX + pA->Get_IsoInfo().fWorldZ)
				< (pB->Get_IsoInfo().fWorldX + pB->Get_IsoInfo().fWorldZ);
		});

	// 컬링 후 렌더
	for (auto& pObj : vecSortList)
	{
		ISO_INFO tInfo = pObj->Get_IsoInfo();
		if (CCamera::Get_Instance()->Is_InViewport(
			tInfo.fWorldX, tInfo.fWorldZ, tInfo.fCX, tInfo.fCY))
		{
			pObj->Render(pRT);
		}
	}
}

CGameObject* CObject_Manager::Find_OtherPlayer(int32_t nPlayerID)
{
	for (auto& pObj : m_ObjectList[OBJ_OTHER_PLAYER])
	{
		COther_Player* pOther = static_cast<COther_Player*>(pObj);
		if (pOther->GetPlayerID() == nPlayerID)
			return pObj;
	}
	return nullptr;
}

CGameObject* CObject_Manager::Find_Monster(int32_t nMonsterID)
{
	for (auto& pObj : m_ObjectList[OBJ_MONSTER])
	{
		CMonster* pMonster = static_cast<CMonster*>(pObj);
		if (pMonster->Get_MonsterID() == nMonsterID)
			return pObj;
	}
	return nullptr;
}

void CObject_Manager::Release(void)
{

	for (size_t i = 0; i < OBJ_END; ++i)
	{
		for_each(m_ObjectList[i].begin(), m_ObjectList[i].end(), Safe_Delete<CGameObject*>);
		m_ObjectList[i].clear();
	}
}

void CObject_Manager::DeleteID(OBJ_ID eID)
{
	for (auto& iter : m_ObjectList[eID])
		Safe_Delete(iter);

	m_ObjectList[eID].clear();
}

CGameObject* CObject_Manager::Pick_Monster(POINT tMouse)
{
	for (auto& pObj : m_ObjectList[OBJ_MONSTER])
	{
		CMonster* pMonster = static_cast<CMonster*>(pObj);
		if (pMonster->Is_MouseCollide(tMouse))
			return pObj;
	}
	return nullptr;
}