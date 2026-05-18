#include "pch.h"
#include "Player.h"
#include "Img_Manager.h"
#include "Input_Manager.h"
#include "Camera.h"
#include "Map_Manager.h"
#include "ItemData_Potion.h"
#include "ItemData_Equipment.h"
#include "ItemData_Etc.h" 
#include "Network_Manager.h"
#include "Object_Manager.h"
#include "Monster.h"

CPlayer::CPlayer()
{
	m_iMaxExp = 100.f;
	m_iCurExp = m_iMaxExp;
}

CPlayer::~CPlayer()
{
	Release();
}

void CPlayer::Initialize()
{

#pragma region Sprites :

	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/idle/idle_B.png", L"PLAYER_IDLE_B");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/idle/idle_LB.png", L"PLAYER_IDLE_LB");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/idle/idle_L.png", L"PLAYER_IDLE_L");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/idle/idle_LT.png", L"PLAYER_IDLE_LT");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/idle/idle_T.png", L"PLAYER_IDLE_T");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/idle/idle_RT.png", L"PLAYER_IDLE_RT");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/idle/idle_R.png", L"PLAYER_IDLE_R");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/idle/idle_RB.png", L"PLAYER_IDLE_RB");

#pragma endregion PLAYER IDLE

#pragma region Sprites :

	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_B.png", L"PLAYER_WALK_B");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_LB.png", L"PLAYER_WALK_LB");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_L.png", L"PLAYER_WALK_L");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_LT.png", L"PLAYER_WALK_LT");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_T.png", L"PLAYER_WALK_T");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_RT.png", L"PLAYER_WALK_RT");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_R.png", L"PLAYER_WALK_R");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_RB.png", L"PLAYER_WALK_RB");

#pragma endregion PLAYER WALK

#pragma region Sprites :

	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_B.png", L"PLAYER_WALK_B");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_LB.png", L"PLAYER_WALK_LB");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_L.png", L"PLAYER_WALK_L");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_LT.png", L"PLAYER_WALK_LT");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_T.png", L"PLAYER_WALK_T");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_RT.png", L"PLAYER_WALK_RT");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_R.png", L"PLAYER_WALK_R");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/walk/walk_RB.png", L"PLAYER_WALK_RB");

#pragma endregion PLAYER WALK

#pragma region Sprites :

	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/attack/attack_B.png", L"PLAYER_ATTACK_B");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/attack/attack_LB.png", L"PLAYER_ATTACK_LB");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/attack/attack_L.png", L"PLAYER_ATTACK_L");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/attack/attack_LT.png", L"PLAYER_ATTACK_LT");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/attack/attack_T.png", L"PLAYER_ATTACK_T");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/attack/attack_RT.png", L"PLAYER_ATTACK_RT");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/attack/attack_R.png", L"PLAYER_ATTACK_R");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/attack/attack_RB.png", L"PLAYER_ATTACK_RB");

#pragma endregion PLAYER ATTACK

#pragma region Sprites :

	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/hit/hit_B.png", L"PLAYER_HIT_B");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/hit/hit_LB.png", L"PLAYER_HIT_LB");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/hit/hit_L.png", L"PLAYER_HIT_L");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/hit/hit_LT.png", L"PLAYER_HIT_LT");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/hit/hit_T.png", L"PLAYER_HIT_T");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/hit/hit_RT.png", L"PLAYER_HIT_RT");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/hit/hit_R.png", L"PLAYER_HIT_R");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/hit/hit_RB.png", L"PLAYER_HIT_RB");

#pragma endregion PLAYER HIT

#pragma region Sprites :

	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/dead/dead_B.png", L"PLAYER_DEAD_B");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/dead/dead_LB.png", L"PLAYER_DEAD_LB");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/dead/dead_L.png", L"PLAYER_DEAD_L");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/dead/dead_LT.png", L"PLAYER_DEAD_LT");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/dead/dead_T.png", L"PLAYER_DEAD_T");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/dead/dead_RT.png", L"PLAYER_DEAD_RT");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/dead/dead_R.png", L"PLAYER_DEAD_R");
	CImg_Manager::Get_Instance()->Insert_Png(L"../Resource/Character/Player/dead/dead_RB.png", L"PLAYER_DEAD_RB");

#pragma endregion PLAYER DEAD

	m_pInventory = new CInventory;
	m_pEquipment = new CEquipment;
	m_iHp = 100;
	m_iMaxHp = 100;
	m_iMp = 100;
	m_iMaxMp = 100;
	m_iAttack = 10;
	m_iDef = 5;

	Motion_Change(PLAYER_IDLE);
	Direction_Change(DIR_B);
	Set_Collider(0.2f, 0.2f);
	m_fSpeed = 1.f;
	m_tIsoInfo.fWorldX = 10.f;
	m_tIsoInfo.fWorldZ = 10.f;

}

int CPlayer::Update(float dt)
{
	Key_Input(dt);
	Update_ClickEffect(dt);

	if (m_nAttackTargetID != -1)
		Update_AttackTarget();

	__super::Update_Rect();
	__super::Move_Frame();
	Check_AnimEnd();

	return OBJ_NOEVENT;
}

void CPlayer::Late_Update(float dt)
{
}

void CPlayer::Render(ID2D1RenderTarget* pRT)
{
	switch (m_eCurState)
	{
	case PLAYER_IDLE:   RenderIDLE(pRT);   break;
	case PLAYER_WALK:   RenderWALK(pRT);   break;
	case PLAYER_ATTACK: RenderATTACK(pRT); break;
	case PLAYER_HIT:    RenderHIT(pRT);    break;
	case PLAYER_DEAD:   RenderDEAD(pRT);   break;
	default: break;
	}

	Render_ClickEffect(pRT);
#ifdef GAME_DEBUG
	Debug_Render(pRT);
#endif
}

void CPlayer::Release(void)
{
	if (m_pInventory) { delete m_pInventory; m_pInventory = nullptr; }
	if (m_pEquipment) { delete m_pEquipment; m_pEquipment = nullptr; }
}



// ================================================================
//  렌더링
// ================================================================

void CPlayer::Render_Sprite(ID2D1RenderTarget* pRT, ID2D1Bitmap* pBitmap)
{
	if (!pBitmap) return;

	POINT tScreen = CCamera::Get_Instance()->IsoWorldToScreen(
		m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ);

	float fDrawX = tScreen.x - m_tIsoInfo.fCX / 2.f;
	float fDrawY = tScreen.y - m_tIsoInfo.fCY
		- m_tIsoInfo.fHeight + TILE_HALF_H;

	// 목적지 Rect
	D2D1_RECT_F destRect = D2D1::RectF(
		fDrawX,
		fDrawY,
		fDrawX + m_tIsoInfo.fCX,
		fDrawY + m_tIsoInfo.fCY
	);

	// 스프라이트 시트에서 현재 프레임 잘라내기
	float fSrcX = m_tIsoInfo.fCX * m_tFrame.iFrameStart;
	D2D1_RECT_F srcRect = D2D1::RectF(
		fSrcX,
		0.f,
		fSrcX + m_tIsoInfo.fCX,
		m_tIsoInfo.fCY
	);

	pRT->DrawBitmap(
		pBitmap,
		destRect,
		1.0f,
		D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
		srcRect
	);
}

void CPlayer::RenderIDLE(ID2D1RenderTarget* pRT)
{
	ID2D1Bitmap* pBitmap = nullptr;
	switch (m_eDir)
	{
	case DIR_B:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_IDLE_B");  break;
	case DIR_LB: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_IDLE_LB"); break;
	case DIR_L:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_IDLE_L");  break;
	case DIR_LT: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_IDLE_LT"); break;
	case DIR_T:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_IDLE_T");  break;
	case DIR_RT: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_IDLE_RT"); break;
	case DIR_R:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_IDLE_R");  break;
	case DIR_RB: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_IDLE_RB"); break;
	default: break;
	}
	Render_Sprite(pRT, pBitmap);
}

void CPlayer::RenderWALK(ID2D1RenderTarget* pRT)
{
	ID2D1Bitmap* pBitmap = nullptr;
	switch (m_eDir)
	{
	case DIR_B:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_WALK_B");  break;
	case DIR_LB: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_WALK_LB"); break;
	case DIR_L:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_WALK_L");  break;
	case DIR_LT: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_WALK_LT"); break;
	case DIR_T:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_WALK_T");  break;
	case DIR_RT: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_WALK_RT"); break;
	case DIR_R:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_WALK_R");  break;
	case DIR_RB: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_WALK_RB"); break;
	default: break;
	}
	Render_Sprite(pRT, pBitmap);
}

void CPlayer::RenderATTACK(ID2D1RenderTarget* pRT) { 
	ID2D1Bitmap* pBitmap = nullptr;
	switch (m_eDir)
	{
	case DIR_B:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_ATTACK_B");  break;
	case DIR_LB: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_ATTACK_LB"); break;
	case DIR_L:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_ATTACK_L");  break;
	case DIR_LT: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_ATTACK_LT"); break;
	case DIR_T:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_ATTACK_T");  break;
	case DIR_RT: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_ATTACK_RT"); break;
	case DIR_R:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_ATTACK_R");  break;
	case DIR_RB: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_ATTACK_RB"); break;
	default: break;
	}
	Render_Sprite(pRT, pBitmap);
}

void CPlayer::RenderHIT(ID2D1RenderTarget* pRT) { 

	ID2D1Bitmap* pBitmap = nullptr;
	switch (m_eDir)
	{
	case DIR_B:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_HIT_B");  break;
	case DIR_LB: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_HIT_LB"); break;
	case DIR_L:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_HIT_L");  break;
	case DIR_LT: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_HIT_LT"); break;
	case DIR_T:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_HIT_T");  break;
	case DIR_RT: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_HIT_RT"); break;
	case DIR_R:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_HIT_R");  break;
	case DIR_RB: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_HIT_RB"); break;
	default: break;
	}
	Render_Sprite(pRT, pBitmap);

}

void CPlayer::RenderDEAD(ID2D1RenderTarget* pRT) { 

	ID2D1Bitmap* pBitmap = nullptr;
	switch (m_eDir)
	{
	case DIR_B:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_DEAD_B");  break;
	case DIR_LB: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_DEAD_LB"); break;
	case DIR_L:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_DEAD_L");  break;
	case DIR_LT: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_DEAD_LT"); break;
	case DIR_T:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_DEAD_T");  break;
	case DIR_RT: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_DEAD_RT"); break;
	case DIR_R:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_DEAD_R");  break;
	case DIR_RB: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PLAYER_DEAD_RB"); break;
	default: break;
	}
	Render_Sprite(pRT, pBitmap);
}

void CPlayer::Render_ClickEffect(ID2D1RenderTarget* pRT)
{
	if (!m_tClickEffect.bActive) return;

	POINT tScreen = CCamera::Get_Instance()->IsoWorldToScreen(
		m_tClickEffect.fWorldX, m_tClickEffect.fWorldZ);

	float fRX = TILE_HALF_W * 0.4f * m_tClickEffect.fScale;
	float fRY = TILE_HALF_H * 0.4f * m_tClickEffect.fScale;
	if (fRX <= 1.f || fRY <= 1.f) return;

	D2D1_ELLIPSE ellipse = D2D1::Ellipse(
		D2D1::Point2F((float)tScreen.x, (float)tScreen.y),
		fRX, fRY
	);

	//color에서 알파만 Scale로 덮어쓰기
	D2D1_COLOR_F col = m_tClickEffect.color;
	col.a = m_tClickEffect.fScale; // 페이드아웃

	ID2D1SolidColorBrush* pBrush = nullptr;
	pRT->CreateSolidColorBrush(col, &pBrush);
	pRT->DrawEllipse(ellipse, pBrush, 2.f);
	pBrush->Release();
}

void CPlayer::Check_AnimEnd()
{
	if (m_bLoopAnim) return;
	if (m_tFrame.iFrameStart < m_tFrame.iFrameEnd) return;

	switch (m_eCurState)
	{
	case PLAYER_HIT:
		m_bHit = false;  // 경직 해제
		if (m_bMoving)
			Motion_Change(PLAYER_WALK);
		else
			Motion_Change(PLAYER_IDLE);
		break;

	case PLAYER_ATTACK:
		if (m_bMoving)
			Motion_Change(PLAYER_WALK);
		else
			Motion_Change(PLAYER_IDLE);
		break;

	case PLAYER_DEAD:
		break;

	default: break;
	}
}
// ================================================================
//  상태/방향 뱐걍
// ================================================================

void CPlayer::Motion_Change(PLAYER_STATE eState)
{
	m_eCurState = eState;

	switch (eState)
	{
	case PLAYER_IDLE:
		m_tIsoInfo.fCX = 160.f;
		m_tIsoInfo.fCY = 128.f;
		m_tIsoInfo.fHeight = 30.f;
		m_bLoopAnim = true;
		Set_Frame(7, 100);
		break;

	case PLAYER_WALK:
		m_tIsoInfo.fCX = 160.f;
		m_tIsoInfo.fCY = 128.f;
		m_tIsoInfo.fHeight = 30.f;
		m_bLoopAnim = true;
		Set_Frame(7, 100);
		break;

	case PLAYER_ATTACK:
		m_tIsoInfo.fCX = 160.f;
		m_tIsoInfo.fCY = 128.f;
		m_tIsoInfo.fHeight = 30.f;
		m_bLoopAnim = false;
		Set_Frame(15, 30);
		break;

	case PLAYER_HIT:
		m_tIsoInfo.fCX = 160.f;
		m_tIsoInfo.fCY = 128.f;
		m_tIsoInfo.fHeight = 30.f;
		m_bLoopAnim = false;
		Set_Frame(3, 30);
		break;

	case PLAYER_DEAD:
		m_tIsoInfo.fCX = 160.f;
		m_tIsoInfo.fCY = 160.f;
		m_tIsoInfo.fHeight = 30.f;
		m_bLoopAnim = false;
		Set_Frame(23, 70);
		break;

	default:
		break;
	}
}

void CPlayer::Direction_Change(DIRECTION eDir)
{
	m_eDir = eDir;
	m_tFrame.iFrameStart = 0;
}

void CPlayer::Key_Input(float dt)
{
	if (!CInput_Manager::Get_Instance()->Is_GameMode()) return;
	if (m_bHit) return;

	CInput_Manager* pInput = CInput_Manager::Get_Instance();

	// ===== 매 프레임 마우스 위치로 타일 체크 =====
	POINT tMouse = pInput->Get_MousePos();
	float fWorldX, fWorldZ;
	CCamera::Get_Instance()->ScreenToIsoWorld(tMouse.x, tMouse.y, fWorldX, fWorldZ);

	int iTileX = (int)floorf(fWorldX);
	int iTileZ = (int)floorf(fWorldZ);

	bool bMovable = CMap_Manager::Get_Instance()->Is_Movable(iTileX, iTileZ);

	if (!bMovable)
		pInput->Set_CursorMode(CURSOR_NON_ATTACK);

	// ===== 클릭 처리 =====
	if (pInput->Key_Down(VK_LBUTTON))
	{

		CGameObject* pClickedMonster =
			CObject_Manager::Get_Instance()->Pick_Monster(tMouse);

		if (pClickedMonster)
		{
			CMonster* pMonster = static_cast<CMonster*>(pClickedMonster);

			if (pMonster->Get_MonsterState() != MON_DEAD)
			{
				// 공격 타겟 설정
				m_nAttackTargetID = pMonster->Get_MonsterID();

				// 기존 A* 이동 코드 그대로 재사용
				// 몬스터 위치를 목적지로
				fWorldX = pMonster->Get_WorldX();
				fWorldZ = pMonster->Get_WorldZ();

				int32_t nStartX = static_cast<int32_t>(floorf(m_tIsoInfo.fWorldX));
				int32_t nStartZ = static_cast<int32_t>(floorf(m_tIsoInfo.fWorldZ));
				int32_t nEndX = static_cast<int32_t>(floorf(fWorldX));
				int32_t nEndZ = static_cast<int32_t>(floorf(fWorldZ));

				IsMovableFunc fnIsMovable = [](int32_t x, int32_t z) -> bool {
					return CMap_Manager::Get_Instance()->Is_Movable(x, z);
					};

				m_waypoints = CPathFinder::FindPath(
					nStartX, nStartZ,
					nEndX, nEndZ,
					m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ,
					fnIsMovable,
					EPathMode::CornerBased);

				// 마지막 웨이포인트를 몬스터 위치로
				if (!m_waypoints.empty())
					m_waypoints.back() = { fWorldX, fWorldZ };

				m_nCurWaypoint = 0;

				if (!m_waypoints.empty())
				{
					m_bMoving = true;
					Motion_Change(PLAYER_WALK);

					CNetwork_Manager::Get_Instance()->SendMoveDest(
						m_waypoints[0].first,
						m_waypoints[0].second,
						static_cast<uint32_t>(GetTickCount64()));
				}
			}
			return;  // 몬스터 클릭이면 이하 타일 이동 처리 스킵
		}
		m_nAttackTargetID = -1;

		if (!bMovable) return;

		// 시작 타일
		int32_t nStartX = static_cast<int32_t>(floorf(m_tIsoInfo.fWorldX));
		int32_t nStartZ = static_cast<int32_t>(floorf(m_tIsoInfo.fWorldZ));
		int32_t nEndX = static_cast<int32_t>(floorf(fWorldX));
		int32_t nEndZ = static_cast<int32_t>(floorf(fWorldZ));

		// A* 경로 계산
		IsMovableFunc fnIsMovable = [](int32_t x, int32_t z) -> bool {
			return CMap_Manager::Get_Instance()->Is_Movable(x, z);
			};

		m_waypoints = CPathFinder::FindPath(
			nStartX, nStartZ,
			nEndX, nEndZ,
			m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ,
			fnIsMovable,
			EPathMode::CornerBased);

		// 마지막 웨이포인트를 클릭한 정확한 위치로 교체
		if (!m_waypoints.empty())
			m_waypoints.back() = { fWorldX, fWorldZ };

		m_nCurWaypoint = 0;

		if (!m_waypoints.empty())
		{
			m_nCurWaypoint = 0;
			m_bMoving = true;
			Motion_Change(PLAYER_WALK);

			// 첫 웨이포인트만 전송
			CNetwork_Manager::Get_Instance()->SendMoveDest(
				m_waypoints[0].first,
				m_waypoints[0].second,
				static_cast<uint32_t>(GetTickCount64()));
		}
#ifdef GAME_DEBUG
		m_iDebugTileX = (int)floorf(m_fDestWorldX);
		m_iDebugTileZ = (int)floorf(m_fDestWorldZ);
		m_fDebugLocalX = m_fDestWorldX - (float)m_iDebugTileX;
		m_fDebugLocalZ = m_fDestWorldZ - (float)m_iDebugTileZ;
#endif
	}

	if (m_bMoving)
		Move_To_Dest(dt);

}

void CPlayer::Move_To_Dest(float dt)
{
	if (m_waypoints.empty() || m_nCurWaypoint >= (int32_t)m_waypoints.size())
	{
		m_bMoving = false;
		Motion_Change(PLAYER_IDLE);
		return;
	}

	float fDestX = m_waypoints[m_nCurWaypoint].first;
	float fDestZ = m_waypoints[m_nCurWaypoint].second;

	float fDX = fDestX - m_tIsoInfo.fWorldX;
	float fDZ = fDestZ - m_tIsoInfo.fWorldZ;
	float fDist = sqrtf(fDX * fDX + fDZ * fDZ);
	float fFrameSpeed = m_fSpeed * dt;

	if (fDist <= fFrameSpeed)
	{
		m_tIsoInfo.fWorldX = fDestX;
		m_tIsoInfo.fWorldZ = fDestZ;
		m_nCurWaypoint++;

		if (m_nCurWaypoint >= (int32_t)m_waypoints.size())
		{
			// 최종 도착
			m_bMoving = false;
			m_waypoints.clear();
			m_nCurWaypoint = 0;
			Motion_Change(PLAYER_IDLE);

			CNetwork_Manager::Get_Instance()->SendMovePos(
				m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ,
				static_cast<uint32_t>(GetTickCount64()));
		}
		else
		{
			// 다음 웨이포인트로 → 서버에 전송
			CNetwork_Manager::Get_Instance()->SendMoveDest(
				m_waypoints[m_nCurWaypoint].first,
				m_waypoints[m_nCurWaypoint].second,
				static_cast<uint32_t>(GetTickCount64()));
		}
		return;
	}

	float fNX = fDX / fDist;
	float fNZ = fDZ / fDist;
	m_tIsoInfo.fWorldX += fNX * fFrameSpeed;
	m_tIsoInfo.fWorldZ += fNZ * fFrameSpeed;

	Decide_Direction(fNX, fNZ);

	// 타일 변경 시 서버 전송
	int32_t nCurTileX = static_cast<int32_t>(floorf(m_tIsoInfo.fWorldX));
	int32_t nCurTileZ = static_cast<int32_t>(floorf(m_tIsoInfo.fWorldZ));

	if (nCurTileX != m_nLastTileX || nCurTileZ != m_nLastTileZ)
	{
		m_nLastTileX = nCurTileX;
		m_nLastTileZ = nCurTileZ;

		CNetwork_Manager::Get_Instance()->SendMovePos(
			m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ,
			static_cast<uint32_t>(GetTickCount64()));
	}
}

void CPlayer::Decide_Direction(float fNX, float fNZ)
{
	// 이동 벡터의 각도로 8방향 결정
	float fScreenDX = (fNX - fNZ) * TILE_HALF_W;  // 화면 X방향
	float fScreenDY = (fNX + fNZ) * TILE_HALF_H;  // 화면 Y방향

	// 화면상 방향벡터로 각도 계산
	float fAngle = atan2f(fScreenDY, fScreenDX) * 180.f / 3.14159f;

	DIRECTION eNewDir = m_eDir;

	if (fAngle >= -22.5f && fAngle < 22.5f)  eNewDir = DIR_R;
	else if (fAngle >= 22.5f && fAngle < 67.5f)  eNewDir = DIR_RB;
	else if (fAngle >= 67.5f && fAngle < 112.5f)  eNewDir = DIR_B;
	else if (fAngle >= 112.5f && fAngle < 157.5f)  eNewDir = DIR_LB;
	else if (fAngle >= 157.5f || fAngle < -157.5f) eNewDir = DIR_L;
	else if (fAngle >= -157.5f && fAngle < -112.5f) eNewDir = DIR_LT;
	else if (fAngle >= -112.5f && fAngle < -67.5f) eNewDir = DIR_T;
	else                                              eNewDir = DIR_RT;

	if (eNewDir != m_eDir)
		Direction_Change(eNewDir);
}

void CPlayer::Update_ClickEffect(float dt)
{
	if (!m_tClickEffect.bActive) return;

	m_tClickEffect.fScale -= dt * 2.f;  // 속도 조절
	if (m_tClickEffect.fScale <= 0.f)
	{
		m_tClickEffect.fScale = 0.f;
		m_tClickEffect.bActive = false;
	}
}

void CPlayer::Update_AttackTarget()
{
	// 타겟 탐색
	CGameObject* pObj =
		CObject_Manager::Get_Instance()->Find_Monster(m_nAttackTargetID);

	if (!pObj)
	{
		m_nAttackTargetID = -1;
		return;
	}

	CMonster* pMonster = static_cast<CMonster*>(pObj);

	// 사망 시 타겟 해제
	if (pMonster->Get_MonsterState() == MON_DEAD)
	{
		m_nAttackTargetID = -1;
		return;
	}

	// 거리 체크
	float fDX = pMonster->Get_WorldX() - m_tIsoInfo.fWorldX;
	float fDZ = pMonster->Get_WorldZ() - m_tIsoInfo.fWorldZ;
	float fDist = sqrtf(fDX * fDX + fDZ * fDZ);

	if (fDist <= m_fAttackRange)
	{
		// 이동 중지
		m_bMoving = false;
		m_waypoints.clear();
		m_nCurWaypoint = 0;
		m_nAttackTargetID = -1;

		// 몬스터 방향 바라봄
		float fNX = fDX / fDist;
		float fNZ = fDZ / fDist;
		Decide_Direction(fNX, fNZ);

		// 공격 모션 (기존 Motion_Change 재사용)
		Motion_Change(PLAYER_ATTACK);

		// 공격 패킷 전송
		CNetwork_Manager::Get_Instance()->SendAttackMonster(
			pMonster->Get_MonsterID(),
			m_tIsoInfo.fWorldX,
			m_tIsoInfo.fWorldZ);
	}
}

void CPlayer::Hit()
{
	m_bHit = true;
	m_bMoving = false;
	m_waypoints.clear();
	m_nCurWaypoint = 0;
	m_nAttackTargetID = -1;

	Motion_Change(PLAYER_HIT);
}


// ================================================================
//  아이템 / 인벤토리
// ================================================================
void CPlayer::Use_Item(int iSlot)
{
	CItemData* pItem = m_pInventory->Get_Item(iSlot);
	if (!pItem || pItem->Get_Type() != ITEM_USE) return;

	CItemData_UseItem* pUse = dynamic_cast<CItemData_UseItem*>(pItem);
	pUse->Use_Item(this);   // 효과 적용

	// 수량 감소 or 슬롯 제거
	if (m_pInventory->Get_StackCount(iSlot) > 1)
	{
		m_pInventory->Decrease_Stack(iSlot);
	}
	else
	{
		CItemData* pRemoved = m_pInventory->Remove_Item(iSlot);
		delete pRemoved;
	}
}

void CPlayer::Use_QuickSlot(int iSlot, CItemData_UseItem* pItem)
{

	for (int i = 0; i < INVEN_SIZE; ++i)
	{
		if (m_pInventory->Get_Item(i) == pItem)
		{
			Use_Item(i);
			return;
		}
	}
}

void CPlayer::Equip_Item(int iSlot)
{
	CItemData* pItem = m_pInventory->Get_Item(iSlot);
	if (!pItem || pItem->Get_Type() != ITEM_EQUIPMENT) return;

	CItemData_Equipment* pEquip = dynamic_cast<CItemData_Equipment*>(pItem);
	EQUIP_SLOT eSlot = pEquip->Get_EquipSlot();

	// 인벤토리에서 소유권 꺼냄
	m_pInventory->Remove_Item(iSlot);

	// 기존 아이템 있으면 인벤토리로 반환
	CItemData_Equipment* pPrev = m_pEquipment->Equip(eSlot, pEquip);
	if (pPrev)
		m_pInventory->Add_Item(pPrev);
}

void CPlayer::UnEquip_Item(EQUIP_SLOT eSlot)
{
	CItemData_Equipment* pItem = m_pEquipment->Get_Equipped(eSlot);
	if (!pItem) return;

	if (m_pInventory->Find_EmptySlot() == INVEN_FULL) return;  // 꽉 차있으면 해제 불가

	pItem = m_pEquipment->UnEquip(eSlot);
	m_pInventory->Add_Item(pItem);
}



#ifdef GAME_DEBUG
void CPlayer::Debug_Render(ID2D1RenderTarget* pRT)
{
	Debug_DrawClickedTile(pRT);
	Debug_DrawClickPoint(pRT);
	Debug_DrawCollider(pRT);
	Debug_DrawText(pRT);
	Debug_DrawPath(pRT);  // ← 추가
}

void CPlayer::Debug_DrawClickedTile(ID2D1RenderTarget* pRT)
{
	POINT tS = CCamera::Get_Instance()->IsoWorldToScreen(
		(float)m_iDebugTileX, (float)m_iDebugTileZ);

	ID2D1SolidColorBrush* pBrush = nullptr;
	pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 0.f, 0.f), &pBrush);

	float cx = (float)tS.x, cy = (float)tS.y;
	float hw = TILE_WIDTH / 2.f, hh = TILE_HEIGHT / 2.f;

	pRT->DrawLine({ cx,      cy }, { cx + hw, cy + hh }, pBrush, 2.f);
	pRT->DrawLine({ cx + hw, cy + hh }, { cx,      cy + hh * 2 }, pBrush, 2.f);
	pRT->DrawLine({ cx,      cy + hh * 2 }, { cx - hw, cy + hh }, pBrush, 2.f);
	pRT->DrawLine({ cx - hw, cy + hh }, { cx,      cy }, pBrush, 2.f);

	pBrush->Release();
}

void CPlayer::Debug_DrawClickPoint(ID2D1RenderTarget* pRT)
{
	POINT tS = CCamera::Get_Instance()->IsoWorldToScreen(
		m_fDestWorldX, m_fDestWorldZ);

	ID2D1SolidColorBrush* pBrush = nullptr;
	pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 1.f, 0.f), &pBrush);

	float cx = (float)tS.x, cy = (float)tS.y;
	// 십자선
	pRT->DrawLine({ cx - 10.f, cy }, { cx + 10.f, cy }, pBrush, 2.f);
	pRT->DrawLine({ cx,        cy - 10.f }, { cx,        cy + 10.f }, pBrush, 2.f);

	pBrush->Release();
}

void CPlayer::Debug_DrawCollider(ID2D1RenderTarget* pRT)
{
	float fCX = Get_ColliderX();
	float fCZ = Get_ColliderZ();
	float fRX = m_tCollider.fRadiusX;
	float fRZ = m_tCollider.fRadiusZ;

	POINT tTL = CCamera::Get_Instance()->IsoWorldToScreen(fCX - fRX, fCZ - fRZ);
	POINT tTR = CCamera::Get_Instance()->IsoWorldToScreen(fCX + fRX, fCZ - fRZ);
	POINT tBR = CCamera::Get_Instance()->IsoWorldToScreen(fCX + fRX, fCZ + fRZ);
	POINT tBL = CCamera::Get_Instance()->IsoWorldToScreen(fCX - fRX, fCZ + fRZ);

	ID2D1SolidColorBrush* pBrush = nullptr;
	pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 1.f, 1.f), &pBrush);

	auto P = [](POINT p) { return D2D1::Point2F((float)p.x, (float)p.y); };

	pRT->DrawLine(P(tTL), P(tTR), pBrush, 2.f);
	pRT->DrawLine(P(tTR), P(tBR), pBrush, 2.f);
	pRT->DrawLine(P(tBR), P(tBL), pBrush, 2.f);
	pRT->DrawLine(P(tBL), P(tTL), pBrush, 2.f);

	pBrush->Release();
}

void CPlayer::Debug_DrawText(ID2D1RenderTarget* pRT)
{
	TCHAR szBuf[256];
	swprintf_s(szBuf, 256,
		L"클릭타일:[%d,%d] 내부위치:[%.2f,%.2f] 논리좌표:[%.2f,%.2f]",
		m_iDebugTileX, m_iDebugTileZ,
		m_fDebugLocalX, m_fDebugLocalZ,
		m_fDestWorldX, m_fDestWorldZ);

	TCHAR szPlayer[128];
	swprintf_s(szPlayer, 128,
		L"플레이어 월드:[%.2f, %.2f]  방향:%d  콜라이더 중심:[%.2f, %.2f]",
		m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ, (int)m_eDir,
		Get_ColliderX(), Get_ColliderZ());

	ID2D1SolidColorBrush* pBrush = nullptr;
	pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 0.f), &pBrush);

	IDWriteTextFormat* pFont = CImg_Manager::Get_Instance()->Get_DebugFont();

	pRT->DrawText(szBuf, wcslen(szBuf),
		pFont, D2D1::RectF(10.f, 10.f, 800.f, 30.f), pBrush);
	pRT->DrawText(szPlayer, wcslen(szPlayer),
		pFont, D2D1::RectF(10.f, 35.f, 800.f, 55.f), pBrush);

	TCHAR szInven[256];
	swprintf_s(szInven, 256,
		L"HP:%d/%d  MP:%d/%d  Gold:%d  ATK:%d  DEF:%d",
		m_iHp, m_iMaxHp,
		m_iMp, m_iMaxMp,
		m_pInventory->Get_Gold(),
		Get_TotalAtk(),
		Get_TotalDef());

	// 인벤토리 슬롯 0~4 상태
	TCHAR szSlot[256] = L"슬롯: ";
	for (int i = 0; i < 5; ++i)
	{
		CItemData* pItem = m_pInventory->Get_Item(i);
		if (pItem)
		{
			TCHAR szTemp[64];
			swprintf_s(szTemp, 64, L"[%d:%s x%d] ",
				i, pItem->Get_Name(),
				m_pInventory->Get_StackCount(i));
			lstrcat(szSlot, szTemp);
		}
		else
		{
			TCHAR szTemp[16];
			swprintf_s(szTemp, 16, L"[%d:빈슬롯] ", i);
			lstrcat(szSlot, szTemp);
		}
	}

	// 장착 상태
	TCHAR szEquip[128];
	CItemData_Equipment* pWeapon = m_pEquipment->Get_Equipped(SLOT_WEAPON);
	CItemData_Equipment* pArmor = m_pEquipment->Get_Equipped(SLOT_ARMOR);
	swprintf_s(szEquip, 128, L"무기:%s  갑옷:%s",
		pWeapon ? pWeapon->Get_Name() : L"없음",
		pArmor ? pArmor->Get_Name() : L"없음");



	pRT->DrawText(szInven, wcslen(szInven),
		pFont, D2D1::RectF(10.f, 60.f, 800.f, 80.f), pBrush);
	pRT->DrawText(szSlot, wcslen(szSlot),
		pFont, D2D1::RectF(10.f, 85.f, 1000.f, 105.f), pBrush);
	pRT->DrawText(szEquip, wcslen(szEquip),
		pFont, D2D1::RectF(10.f, 110.f, 800.f, 130.f), pBrush);

	pBrush->Release();
}

void CPlayer::Debug_DrawPath(ID2D1RenderTarget* pRT)
{
#ifdef GAME_DEBUG
	if (m_waypoints.empty()) return;

	ID2D1SolidColorBrush* pLineBrush = nullptr;
	ID2D1SolidColorBrush* pDotBrush = nullptr;
	ID2D1SolidColorBrush* pStartBrush = nullptr;

	pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 1.f, 0.f, 0.8f), &pLineBrush);  // 초록 선
	pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 0.f, 1.f), &pDotBrush);   // 노란 점
	pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 0.f, 0.f, 1.f), &pStartBrush); // 빨간 현재위치

	// 현재 위치
	POINT tCur = CCamera::Get_Instance()->IsoWorldToScreen(
		m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ);

	// 현재 위치 → 첫 웨이포인트
	if (m_nCurWaypoint < (int32_t)m_waypoints.size())
	{
		POINT tFirst = CCamera::Get_Instance()->IsoWorldToScreen(
			m_waypoints[m_nCurWaypoint].first,
			m_waypoints[m_nCurWaypoint].second);

		pRT->DrawLine(
			D2D1::Point2F((float)tCur.x, (float)tCur.y),
			D2D1::Point2F((float)tFirst.x, (float)tFirst.y),
			pLineBrush, 2.f);
	}

	// 웨이포인트 간 선
	for (int32_t i = m_nCurWaypoint; i < (int32_t)m_waypoints.size() - 1; ++i)
	{
		POINT tA = CCamera::Get_Instance()->IsoWorldToScreen(
			m_waypoints[i].first, m_waypoints[i].second);
		POINT tB = CCamera::Get_Instance()->IsoWorldToScreen(
			m_waypoints[i + 1].first, m_waypoints[i + 1].second);

		pRT->DrawLine(
			D2D1::Point2F((float)tA.x, (float)tA.y),
			D2D1::Point2F((float)tB.x, (float)tB.y),
			pLineBrush, 2.f);
	}

	// 웨이포인트 점
	for (int32_t i = m_nCurWaypoint; i < (int32_t)m_waypoints.size(); ++i)
	{
		POINT tW = CCamera::Get_Instance()->IsoWorldToScreen(
			m_waypoints[i].first, m_waypoints[i].second);

		// 마지막 웨이포인트는 다른 색
		ID2D1SolidColorBrush* pBrush =
			(i == (int32_t)m_waypoints.size() - 1) ? pStartBrush : pDotBrush;

		pRT->FillEllipse(
			D2D1::Ellipse(D2D1::Point2F((float)tW.x, (float)tW.y), 5.f, 5.f),
			pBrush);
	}

	pLineBrush->Release();
	pDotBrush->Release();
	pStartBrush->Release();
#endif
}

#endif


