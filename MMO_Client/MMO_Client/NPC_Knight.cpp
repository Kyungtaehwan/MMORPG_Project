#include "pch.h"
#include "Npc_Knight.h"
#include "Img_Manager.h"
#include "Camera.h"

// Knight: Idle 48x84x16ea (좌하향) / Talk 52x85x36ea (우하향, Special)

CNPC_Knight::CNPC_Knight() {}
CNPC_Knight::~CNPC_Knight() { Release(); }

void CNPC_Knight::Initialize()
{
    __super::Initialize();

    m_eType = NPC_GUARD;

    Set_NpcName(L"기사");
    m_tIsoInfo.fCX = 48.f;
    m_tIsoInfo.fCY = 84.f;
    m_tIsoInfo.fHeight = 10.f;

    Set_Collider(1.5f, 1.5f);
    Set_MouseCollider(0.f, 0.f,
        m_tIsoInfo.fCX * m_fScale, m_tIsoInfo.fCY * m_fScale);

    Motion_Change(NPC_IDLE);
}

void CNPC_Knight::Motion_Change(NPC_STATE eState)
{
    m_eState = eState;

    switch (eState)
    {
    case NPC_IDLE:
        m_tIsoInfo.fCX = 48.f;
        m_tIsoInfo.fCY = 84.f;
        Set_Frame(15, 130);   // 16프레임 → 0~15
        m_bLoopAnim = true;
        break;

    case NPC_TALK:
        m_tIsoInfo.fCX = 52.f;
        m_tIsoInfo.fCY = 85.f;
        Set_Frame(35, 60);    // 36프레임 → 0~35
        m_bLoopAnim = false;
        break;

    default:
        break;
    }
}

int CNPC_Knight::Update(float dt)
{
    __super::Update(dt);   // 클릭 판정 + 말풍선 + 프레임 진행

    // Talk 1회 재생이 끝나면 Idle 복귀
    if (m_eState == NPC_TALK && !m_bLoopAnim)
    {
        if (m_tFrame.iFrameStart == m_tFrame.iFrameEnd)
            Motion_Change(NPC_IDLE);
    }
    return OBJ_NOEVENT;
}

void CNPC_Knight::Late_Update(float dt)
{
    __super::Late_Update(dt);
}

void CNPC_Knight::Render(ID2D1RenderTarget* pRT)
{
    ID2D1Bitmap* pBmp = nullptr;
    switch (m_eState)
    {
    case NPC_TALK: pBmp = CImg_Manager::Get_Instance()->Find_Png(L"KNIGHT_TALK"); break;
    default:       pBmp = CImg_Manager::Get_Instance()->Find_Png(L"KNIGHT_IDLE"); break;
    }
    Render_Sprite(pRT, pBmp);
    __super::Render(pRT);   // 이름표, 말풍선
}

void CNPC_Knight::On_Click()
{
    // 특별 창 없이 Talk 모션 + 말풍선
    Motion_Change(NPC_TALK);
    m_bClick = true;
    m_bShowBubble = true;
    m_fBubbleTimer = 3.f;
    lstrcpy(m_szBubbleText, L"무엇이 궁금한가?");
}

void CNPC_Knight::On_Interact() {}
void CNPC_Knight::Release() {}
