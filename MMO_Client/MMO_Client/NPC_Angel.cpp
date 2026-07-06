#include "pch.h"
#include "Npc_Angel.h"
#include "Img_Manager.h"
#include "Camera.h"

// Angel: Idle 47x102x24ea (마젠타 배경 제거) / Effect 68x98x24ea (검정 배경 제거)

const float CNPC_Angel::s_fEffectCX = 78.f;
const float CNPC_Angel::s_fEffectCY = 107.f;
const float CNPC_Angel::s_fEffectFrameSec = 0.055f;
const float CNPC_Angel::s_fEffectShowSec = 3.0f;

CNPC_Angel::CNPC_Angel() {}
CNPC_Angel::~CNPC_Angel() { Release(); }

void CNPC_Angel::Initialize()
{
    __super::Initialize();

    m_eType = NPC_GUARD;

    Set_NpcName(L"천사");
    m_tIsoInfo.fCX = 44.f;
    m_tIsoInfo.fCY = 86.f;
    m_tIsoInfo.fHeight = 10.f;

    Set_Collider(1.5f, 1.5f);
    Set_MouseCollider(0.f, 0.f,
        m_tIsoInfo.fCX * m_fScale, m_tIsoInfo.fCY * m_fScale);

    Motion_Change(NPC_IDLE);
}

void CNPC_Angel::Motion_Change(NPC_STATE eState)
{
    m_eState = eState;
    // Idle만 존재 (23프레임 → 0~22). 대화 연출은 이펙트로 처리.
    Set_Frame(22, 120);
    m_bLoopAnim = true;
}

int CNPC_Angel::Update(float dt)
{
    __super::Update(dt);   // 클릭 판정 + 말풍선 + 프레임 진행

    // 이펙트 독립 재생
    if (m_bEffectActive)
    {
        m_fEffectTimer -= dt;
        m_fEffectFrameTimer += dt;
        if (m_fEffectFrameTimer >= s_fEffectFrameSec)
        {
            m_fEffectFrameTimer = 0.f;
            m_iEffectFrame = (m_iEffectFrame + 1) % s_iEffectFrameCnt;
        }
        if (m_fEffectTimer <= 0.f)
        {
            m_bEffectActive = false;
            m_iEffectFrame = 0;
        }
    }
    return OBJ_NOEVENT;
}

void CNPC_Angel::Late_Update(float dt)
{
    __super::Late_Update(dt);
}

void CNPC_Angel::Render_Effect(ID2D1RenderTarget* pRT)
{
    ID2D1Bitmap* pEff = CImg_Manager::Get_Instance()->Find_Png(L"ANGEL_EFFECT");
    if (!pEff) return;

    POINT tScreen = CCamera::Get_Instance()->IsoWorldToScreen(
        m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ);

    float fW = s_fEffectCX * m_fScale;
    float fH = s_fEffectCY * m_fScale;
    float fDrawX = tScreen.x - fW / 2.f + 8.f;
    float fDrawY = tScreen.y - fH - m_tIsoInfo.fHeight + TILE_HALF_H;

    float fSrcX = s_fEffectCX * m_iEffectFrame;

    pRT->DrawBitmap(pEff,
        D2D1::RectF(fDrawX, fDrawY, fDrawX + fW, fDrawY + fH),
        1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
        D2D1::RectF(fSrcX, 0.f, fSrcX + s_fEffectCX, s_fEffectCY));
}

void CNPC_Angel::Render(ID2D1RenderTarget* pRT)
{
    // 이펙트 먼저(뒤) → NPC 스프라이트(앞)
    if (m_bEffectActive)
        Render_Effect(pRT);

    ID2D1Bitmap* pBmp = CImg_Manager::Get_Instance()->Find_Png(L"ANGEL_IDLE");
    Render_Sprite(pRT, pBmp);
    __super::Render(pRT);   // 이름표, 말풍선
}

void CNPC_Angel::On_Click()
{
    // 특별 창 없이 뒤에 이펙트 표시 + 말풍선
    m_bClick = true;
    m_bShowBubble = true;
    m_fBubbleTimer = 3.f;
    lstrcpy(m_szBubbleText, L"축복이 있기를.");

    m_bEffectActive = true;
    m_fEffectTimer = s_fEffectShowSec;
    m_fEffectFrameTimer = 0.f;
    m_iEffectFrame = 0;
}

void CNPC_Angel::On_Interact() {}
void CNPC_Angel::Release() {}
