#pragma once
#include "GameObject.h"
class CObject_Manager
{
private:
	CObject_Manager();
	~CObject_Manager();

public:
	void		Add_Object(OBJ_ID eID, CGameObject* pGameObject);
	int			Update(float dt);
	void		Late_Update(float dt);
	void		Render(ID2D1RenderTarget* pRT);
	void		Release(void);

	// 플레이어가 아직 없으면(로그인 전, 존 전환 중) nullptr. 호출부는 반드시 검사할 것.
	CGameObject* Get_Player() {
		if (List_Empty(OBJ_PLAYER))
			return nullptr;
		return m_ObjectList[OBJ_PLAYER].front();
	}

	CGameObject* Find_OtherPlayer(int32_t nPlayerID);
	CGameObject* Find_Monster(int32_t nMonsterID);
	CGameObject* Find_Drop(int32_t nDropID);
	CGameObject* Find_DropInContact(CGameObject* pPlayer);  // 접촉 중인 드롭(획득용)

	std::list<CGameObject*>* Get_List(OBJ_ID eID) {
		return &m_ObjectList[eID];
	}

	bool List_Empty(OBJ_ID eID) {
		if (m_ObjectList[eID].empty() == true) {
			return true;
		}
		else {
			return false;
		}

	};

	void		DeleteID(OBJ_ID eID);

	CGameObject* Pick_Monster(POINT tMouse);
private:

	std::list<CGameObject*>		m_ObjectList[OBJ_END];

public:
	static	CObject_Manager* Get_Instance()
	{
		if (!m_pInstance)
		{
			m_pInstance = new CObject_Manager;
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
	static	CObject_Manager* m_pInstance;


};

