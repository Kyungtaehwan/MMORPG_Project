#include "pch.h"
#include "Npc_OldMan.h"
#include "Img_Manager.h"
#include "Camera.h"

// OldMan: Idle 28x71x16ea (단일 방향, 정면)

CNPC_OldMan::CNPC_OldMan() {}
CNPC_OldMan::~CNPC_OldMan() { Release(); }

void CNPC_OldMan::Initialize()
{
    __super::Initialize();

    m_eType = NPC_GUARD;

    Set_NpcName(L"노인");
    m_tIsoInfo.fCX = 28.f;
    m_tIsoInfo.fCY = 71.f;
    m_tIsoInfo.fHeight = 10.f;

    Set_Collider(1.5f, 1.5f);
    Set_MouseCollider(0.f, 0.f,
        m_tIsoInfo.fCX * m_fScale, m_tIsoInfo.fCY * m_fScale);

    Motion_Change(NPC_IDLE);
}

void CNPC_OldMan::Motion_Change(NPC_STATE eState)
{
    m_eState = eState;
    // Idle만 존재 (16프레임 → 0~15)
    Set_Frame(15, 130);
    m_bLoopAnim = true;
}

int CNPC_OldMan::Update(float dt)
{
    __super::Update(dt);   // 클릭 판정 + 말풍선 + 프레임 진행
    return OBJ_NOEVENT;
}

void CNPC_OldMan::Late_Update(float dt)
{
    __super::Late_Update(dt);
}

void CNPC_OldMan::Render(ID2D1RenderTarget* pRT)
{
    ID2D1Bitmap* pBmp = CImg_Manager::Get_Instance()->Find_Png(L"OLDMAN_IDLE");
    Render_Sprite(pRT, pBmp);
    __super::Render(pRT);   // 이름표, 말풍선
}

void CNPC_OldMan::On_Click()
{
    // 특별 창 없이 말풍선만
    m_bClick = true;
    m_bShowBubble = true;
    m_fBubbleTimer = 3.f;
    lstrcpy(m_szBubbleText, L"허허, 어서 오게.");
}

void CNPC_OldMan::On_Interact() {}
void CNPC_OldMan::Release() {}
