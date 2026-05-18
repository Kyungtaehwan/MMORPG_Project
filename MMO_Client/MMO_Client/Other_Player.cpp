#include "pch.h"
#include "Other_Player.h"
#include "Img_Manager.h"
#include "Camera.h"

COther_Player::COther_Player()
{
}

COther_Player::~COther_Player()
{
    Release();
}

void COther_Player::Initialize()
{
}

void COther_Player::Initialize(int32_t nPlayerID, const char* pszName,float fX, float fZ)
{
    m_nPlayerID = nPlayerID;
    strncpy_s(m_szName, pszName, sizeof(m_szName) - 1);

    m_tIsoInfo.fWorldX = fX;
    m_tIsoInfo.fWorldZ = fZ;
    m_fDestX = fX;
    m_fDestZ = fZ;
    m_bMoving = false;

    Motion_Change(PLAYER_IDLE);
    Direction_Change(DIR_B);
}

int COther_Player::Update(float dt)
{
    if (m_bDead)
        return OBJ_DEAD;
    
    if (m_bMoving && m_eCurState != PLAYER_HIT)
        Move_To_Dest(dt);


    Check_AnimEnd();

    __super::Update_Rect();
    __super::Move_Frame();

    return OBJ_NOEVENT;
}

void COther_Player::Late_Update(float dt)
{
}

void COther_Player::Render(ID2D1RenderTarget* pRT)
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

    // 이름표 렌더링
    //Render_NameTag(pRT);
}

void COther_Player::Release()
{

}

// ================================================================
//  서버 패킷 수신
// ================================================================
void COther_Player::OnMoveDestPacket(float fCurX, float fCurZ,
    float fDestX, float fDestZ,
    float fSpeed, uint32_t moveTime)
{
    // 현재 위치 보정
    // 오차가 너무 크면 순간이동, 작으면 부드럽게 보간
    float fDX = fCurX - m_tIsoInfo.fWorldX;
    float fDZ = fCurZ - m_tIsoInfo.fWorldZ;
    float fDiff = sqrtf(fDX * fDX + fDZ * fDZ);

    constexpr float TELEPORT_THRESHOLD = 3.f;  // 이 이상 오차면 순간이동
    if (fDiff > TELEPORT_THRESHOLD)
    {
        m_tIsoInfo.fWorldX = fCurX;
        m_tIsoInfo.fWorldZ = fCurZ;
    }

    m_fDestX = fDestX;
    m_fDestZ = fDestZ;
    m_fSpeed = fSpeed;
    m_bMoving = true;

    if (m_eCurState != PLAYER_WALK)
        Motion_Change(PLAYER_WALK);
}

void COther_Player::OnMovePosPacket(float fCurX, float fCurZ,
    uint32_t moveTime)
{
    // 타일 변경 시 위치 보정
    float fDX = fCurX - m_tIsoInfo.fWorldX;
    float fDZ = fCurZ - m_tIsoInfo.fWorldZ;
    float fDiff = sqrtf(fDX * fDX + fDZ * fDZ);

    constexpr float TELEPORT_THRESHOLD = 3.f;
    if (fDiff > TELEPORT_THRESHOLD)
    {
        m_tIsoInfo.fWorldX = fCurX;
        m_tIsoInfo.fWorldZ = fCurZ;
    }
    // 작은 오차는 보간으로 자연스럽게 수렴
}


void COther_Player::OnAttackPacket()
{
    Motion_Change(PLAYER_ATTACK);
    m_bMoving = false;
}

void COther_Player::OnHitPacket(int32_t iHp)
{
    Motion_Change(PLAYER_HIT);
    m_iHp = iHp;
}

// ================================================================
//  렌더링
// ================================================================


void COther_Player::RenderIDLE(ID2D1RenderTarget* pRT)
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

void COther_Player::RenderWALK(ID2D1RenderTarget* pRT)
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

void COther_Player::RenderATTACK(ID2D1RenderTarget* pRT) {
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

void COther_Player::RenderHIT(ID2D1RenderTarget* pRT)
{
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

void COther_Player::RenderDEAD(ID2D1RenderTarget* pRT)
{
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

void COther_Player::Render_Sprite(ID2D1RenderTarget* pRT,
    ID2D1Bitmap* pBitmap)
{
    if (!pBitmap) return;

    POINT tScreen = CCamera::Get_Instance()->IsoWorldToScreen(
        m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ);

    float fDrawX = tScreen.x - m_tIsoInfo.fCX / 2.f;
    float fDrawY = tScreen.y - m_tIsoInfo.fCY
        - m_tIsoInfo.fHeight + TILE_HALF_H;

    D2D1_RECT_F destRect = D2D1::RectF(
        fDrawX, fDrawY,
        fDrawX + m_tIsoInfo.fCX,
        fDrawY + m_tIsoInfo.fCY);

    float fSrcX = m_tIsoInfo.fCX * m_tFrame.iFrameStart;
    D2D1_RECT_F srcRect = D2D1::RectF(
        fSrcX, 0.f,
        fSrcX + m_tIsoInfo.fCX,
        m_tIsoInfo.fCY);

    pRT->DrawBitmap(pBitmap, destRect, 1.0f,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, srcRect);
}
//
//void COther_Player::Render_NameTag(ID2D1RenderTarget* pRT)
//{
//    POINT tScreen = CCamera::Get_Instance()->IsoWorldToScreen(
//        m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ);
//
//    TCHAR szName[20];
//    MultiByteToWideChar(CP_ACP, 0, m_szName, -1, szName, 20);
//
//    ID2D1SolidColorBrush* pBrush = nullptr;
//    pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f), &pBrush);
//
//    IDWriteTextFormat* pFont =
//        CImg_Manager::Get_Instance()->Get_DebugFont();
//
//    float fNameX = tScreen.x - 40.f;
//    float fNameY = tScreen.y - m_tIsoInfo.fCY - m_tIsoInfo.fHeight;
//
//    pRT->DrawText(szName, wcslen(szName), pFont,
//        D2D1::RectF(fNameX, fNameY, fNameX + 80.f, fNameY + 20.f),
//        pBrush);
//
//    pBrush->Release();
//}


// ================================================================
//  이동
// ================================================================
void COther_Player::Move_To_Dest(float dt)
{
    float fDX = m_fDestX - m_tIsoInfo.fWorldX;
    float fDZ = m_fDestZ - m_tIsoInfo.fWorldZ;
    float fDist = sqrtf(fDX * fDX + fDZ * fDZ);
    float fFrameSpeed = m_fSpeed * dt;

    if (fDist <= fFrameSpeed)
    {
        m_tIsoInfo.fWorldX = m_fDestX;
        m_tIsoInfo.fWorldZ = m_fDestZ;
        m_bMoving = false;
        Motion_Change(PLAYER_IDLE);
        return;
    }

    float fNX = fDX / fDist;
    float fNZ = fDZ / fDist;
    m_tIsoInfo.fWorldX += fNX * fFrameSpeed;
    m_tIsoInfo.fWorldZ += fNZ * fFrameSpeed;

    Decide_Direction(fNX, fNZ);
}

void COther_Player::Decide_Direction(float fNX, float fNZ)
{
    float fScreenDX = (fNX - fNZ) * TILE_HALF_W;
    float fScreenDY = (fNX + fNZ) * TILE_HALF_H;
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

// ================================================================
//  상태/방향 세팅
// ================================================================
void COther_Player::Motion_Change(PLAYER_STATE eState)
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
        m_bMoving = false;
        m_bDead = true;
        Set_Frame(23, 70);
        break;

    default: break;
    }
}

void COther_Player::Direction_Change(DIRECTION eDir)
{
    m_eDir = eDir;
    m_tFrame.iFrameStart = 0;
}

void COther_Player::Check_AnimEnd()
{
    if (m_bLoopAnim) return;
    if (m_tFrame.iFrameStart < m_tFrame.iFrameEnd) return;

    switch (m_eCurState)
    {
    case PLAYER_HIT:
        if (m_bMoving)
            Motion_Change(PLAYER_WALK);
        else
            Motion_Change(PLAYER_IDLE);
        break;

    case PLAYER_DEAD:

        break;
    case PLAYER_ATTACK:
        if (m_bMoving)
            Motion_Change(PLAYER_WALK);
        else
            Motion_Change(PLAYER_IDLE);

        break;
    default: break;
    }
}

void COther_Player::SetDeadState()
{
    m_eCurState = PLAYER_DEAD;
    m_tIsoInfo.fCX = 160.f;
    m_tIsoInfo.fCY = 160.f;
    m_tIsoInfo.fHeight = 30.f;
    m_bLoopAnim = false;
    m_bMoving = false;

    Set_Frame(23, 70);
    m_tFrame.iFrameStart = m_tFrame.iFrameEnd;
}