#include "pch.h"
#include "Monster_Wing.h"
#include "Img_Manager.h"
#include "Camera.h"
#include "Input_Manager.h"

// 부유 몬스터 높이 상수: (fCY/2 + fHeight)를 일정하게 유지해 모션 전환 시
// 박쥐의 세로 중심이 튀지 않도록 함 (지면보다 약 60px 위에 떠 있음)
namespace
{
    constexpr float HOVER_K = 100.f;
    inline float HoverHeight(float fCY) { return HOVER_K - fCY * 0.5f; }
}

void CMonster_Wing::Initialize()
{
    __super::Initialize();

    m_iMaxHp = 80;
    m_iHp = m_iMaxHp;
    m_fSpeed = 2.2f;

    m_tIsoInfo.fCX = 114.f;
    m_tIsoInfo.fCY = 84.f;
    m_tIsoInfo.fHeight = HoverHeight(84.f);

    CImg_Manager* pImg = CImg_Manager::Get_Instance();
    pImg->Insert_Png(L"../Resource/Monster/Wing/idle(114X84X12X8).png", L"WING_IDLE");
    pImg->Insert_Png(L"../Resource/Monster/Wing/walk(99X78X8X8).png", L"WING_WALK");
    pImg->Insert_Png(L"../Resource/Monster/Wing/attack0(95X80X10X8).png", L"WING_ATTACK0");
    pImg->Insert_Png(L"../Resource/Monster/Wing/attack1(133X90X14X8).png", L"WING_ATTACK1");
    pImg->Insert_Png(L"../Resource/Monster/Wing/hit(114X86X4X8).png", L"WING_HIT");
    pImg->Insert_Png(L"../Resource/Monster/Wing/dead(161X136X16X8).png", L"WING_DEAD");

    Set_Collider(0.5f, 0.5f);
    Set_MonsterName(L"윙");
    Set_MouseCollider(0.f, 0.f, m_tIsoInfo.fCX, m_tIsoInfo.fCY);
    Motion_Change(MON_IDLE);
}

int CMonster_Wing::Update(float dt)
{
    if (m_bDead) return OBJ_DEAD;

    if (m_bMoving)
        Move_To_Dest(dt);

    __super::Update(dt);

    Check_AnimEnd();
    Update_MouseCollider();

    return OBJ_NOEVENT;
}

void CMonster_Wing::Late_Update(float dt) {}

void CMonster_Wing::Render(ID2D1RenderTarget* pRT)
{
    ID2D1Bitmap* pBitmap = nullptr;
    switch (m_eState)
    {
    case MON_IDLE:     pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"WING_IDLE");    break;
    case MON_WALK:     pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"WING_WALK");    break;
    case MON_ATTACK_0: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"WING_ATTACK0"); break;
    case MON_ATTACK_1: pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"WING_ATTACK1"); break;
    case MON_HIT:      pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"WING_HIT");     break;
    case MON_DEAD:     pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"WING_DEAD");    break;
    default: break;
    }

    Render_Sprite(pRT, pBitmap);
    __super::Render(pRT);

#ifdef GAME_DEBUG
    Debug_Render(pRT);
#endif
}

void CMonster_Wing::Release() {}

void CMonster_Wing::Motion_Change(MONSTER_STATE eState)
{
    m_eState = eState;

    switch (eState)
    {
    case MON_IDLE:
        m_tIsoInfo.fCX = 114.f;
        m_tIsoInfo.fCY = 84.f;
        m_tIsoInfo.fHeight = HoverHeight(84.f);
        m_bLoopAnim = true;
        Set_Frame(11, 70);
        break;

    case MON_WALK:
        m_tIsoInfo.fCX = 99.f;
        m_tIsoInfo.fCY = 78.f;
        m_tIsoInfo.fHeight = HoverHeight(78.f);
        m_bLoopAnim = true;
        Set_Frame(7, 60);
        break;

    case MON_ATTACK_0:
        m_tIsoInfo.fCX = 95.f;
        m_tIsoInfo.fCY = 80.f;
        m_tIsoInfo.fHeight = HoverHeight(80.f);
        m_bLoopAnim = false;
        Set_Frame(9, 30);
        break;

    case MON_ATTACK_1:
        m_tIsoInfo.fCX = 133.f;
        m_tIsoInfo.fCY = 90.f;
        m_tIsoInfo.fHeight = HoverHeight(90.f);
        m_bLoopAnim = false;
        Set_Frame(13, 35);
        break;

    case MON_HIT:
        m_tIsoInfo.fCX = 114.f;
        m_tIsoInfo.fCY = 86.f;
        m_tIsoInfo.fHeight = HoverHeight(86.f);
        m_bLoopAnim = false;
        Set_Frame(3, 40);
        break;

    case MON_DEAD:
        m_tIsoInfo.fCX = 161.f;
        m_tIsoInfo.fCY = 136.f;
        m_tIsoInfo.fHeight = HoverHeight(136.f);
        m_bLoopAnim = false;
        Set_Frame(15, 55);
        break;

    default: break;
    }
}

// ================================================================
//  On_MovePacket - 윙 전용 이동 패킷 처리
// ================================================================
void CMonster_Wing::On_MovePacket(uint8_t nDir)
{
    m_eDir = (DIRECTION)nDir;
    if (m_eState != MON_WALK)
        Motion_Change(MON_WALK);
}

// ================================================================
//  On_StatePacket - 윙 전용 상태 패킷 처리 (공격 모션 2종)
// ================================================================
void CMonster_Wing::On_StatePacket(MONSTER_STATE eState, int32_t nTargetID)
{
    switch (eState)
    {
    case MON_IDLE:
        Motion_Change(MON_IDLE);
        break;

    case MON_WALK:
        if (m_eState != MON_WALK)
            Motion_Change(MON_WALK);
        break;

    case MON_ATTACK_0:
        Motion_Change(MON_ATTACK_0);
        break;

    case MON_ATTACK_1:
        Motion_Change(MON_ATTACK_1);
        break;

    case MON_HIT:
        Motion_Change(MON_HIT);
        break;

    case MON_DEAD:
        Motion_Change(MON_DEAD);
        break;

    default: break;
    }
}

void CMonster_Wing::Check_AnimEnd()
{
    if (m_bLoopAnim) return;
    if (m_tFrame.iFrameStart < m_tFrame.iFrameEnd) return;

    switch (m_eState)
    {
    case MON_HIT:
    case MON_ATTACK_0:
    case MON_ATTACK_1:
        if (m_bMoving)
            Motion_Change(MON_WALK);
        else
            Motion_Change(MON_IDLE);
        break;

    case MON_DEAD:
        m_bDead = true;
        break;

    default: break;
    }
}

// ================================================================
//  Move_To_Dest - 목적지로 이동
// ================================================================
void CMonster_Wing::Move_To_Dest(float dt)
{
    // 피격/공격/사망 중엔 이동·애니메이션을 걷기로 덮어쓰지 않는다
    if (m_eState == MON_HIT || m_eState == MON_ATTACK_0 ||
        m_eState == MON_ATTACK_1 || m_eState == MON_DEAD)
        return;

    float fDX = m_fDestWorldX - m_tIsoInfo.fWorldX;
    float fDZ = m_fDestWorldZ - m_tIsoInfo.fWorldZ;
    float fDist = sqrtf(fDX * fDX + fDZ * fDZ);
    float fStep = m_fSpeed * dt;

    if (fDist <= fStep)
    {
        m_tIsoInfo.fWorldX = m_fDestWorldX;
        m_tIsoInfo.fWorldZ = m_fDestWorldZ;
        m_bMoving = false;

        if (m_eState != MON_IDLE)
            Motion_Change(MON_IDLE);
        return;
    }

    float fNX = fDX / fDist;
    float fNZ = fDZ / fDist;

    m_tIsoInfo.fWorldX += fNX * fStep;
    m_tIsoInfo.fWorldZ += fNZ * fStep;

    if (m_eState != MON_WALK)
        Motion_Change(MON_WALK);
}

void CMonster_Wing::Decide_Direction(float fNX, float fNZ)
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
    else                                             eNewDir = DIR_RT;

    if (eNewDir != m_eDir)
        Direction_Change(eNewDir);
}

void CMonster_Wing::Update_MouseCollider()
{
    POINT tScreen = CCamera::Get_Instance()->IsoWorldToScreen(
        m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ);

    float fLeft = tScreen.x - m_tIsoInfo.fCX * 0.5f;
    float fTop = tScreen.y - m_tIsoInfo.fCY
        - m_tIsoInfo.fHeight + TILE_HALF_H;

    m_tMouseRect.left = (LONG)fLeft;
    m_tMouseRect.top = (LONG)fTop;
    m_tMouseRect.right = (LONG)(fLeft + m_tIsoInfo.fCX);
    m_tMouseRect.bottom = (LONG)(fTop + m_tIsoInfo.fCY);
}

#ifdef GAME_DEBUG
void CMonster_Wing::Debug_Render(ID2D1RenderTarget* pRT)
{
    Debug_DrawCollider(pRT);
    Debug_DrawMouseCollider(pRT);
}

void CMonster_Wing::Debug_DrawCollider(ID2D1RenderTarget* pRT)
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

void CMonster_Wing::Debug_DrawMouseCollider(ID2D1RenderTarget* pRT)
{
    ID2D1SolidColorBrush* pBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 0.f), &pBrush);
    pRT->DrawRectangle(
        D2D1::RectF(
            (float)m_tMouseRect.left,
            (float)m_tMouseRect.top,
            (float)m_tMouseRect.right,
            (float)m_tMouseRect.bottom),
        pBrush, 2.f);
    pBrush->Release();
}

void CMonster_Wing::Debug_DrawText(ID2D1RenderTarget* pRT) {}
#endif
