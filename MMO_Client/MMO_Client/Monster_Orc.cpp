#include "pch.h"
#include "Monster_Orc.h"
#include "Img_Manager.h"
#include "Camera.h"
#include "Input_Manager.h"

void CMonster_Orc::Initialize()
{

    __super::Initialize();

    m_iMaxHp = 100;
    m_fSpeed = 1.f;
    m_tIsoInfo.fCX = 109.f;   // 게이트 프레임 1장 가로 (3270 / 30)
    m_tIsoInfo.fCY = 176.f;   // 게이트 세로
    m_tIsoInfo.fHeight = 0.f;

    CImg_Manager* pImg = CImg_Manager::Get_Instance();
    pImg->Insert_Png(L"../Resource/Monster/Orc/idle(133X130X8X8).png", L"ORC_IDLE");
    pImg->Insert_Png(L"../Resource/Monster/Orc/walk(157X141X8X8).png", L"ORC_WALK");
    pImg->Insert_Png(L"../Resource/Monster/Orc/attack0(151X146X15X8).png", L"ORC_ATTACK0");
    pImg->Insert_Png(L"../Resource/Monster/Orc/attack1(220X216X15X8).png", L"ORC_ATTACK1");
    pImg->Insert_Png(L"../Resource/Monster/Orc/hit(177X163X7X8).png", L"ORC_HIT");
    pImg->Insert_Png(L"../Resource/Monster/Orc/dead(325X206X23X8).png", L"ORC_DEAD");

    Set_Collider(0.6f, 0.6f);
    Set_MonsterName(L"김상효");
    Set_MouseCollider(0.f, 0.f, m_tIsoInfo.fCX, m_tIsoInfo.fCY);
    Motion_Change(MON_IDLE);
}

int CMonster_Orc::Update(float dt)
{
    /*CInput_Manager* pInput = CInput_Manager::Get_Instance();


    if (pInput->Key_Down(VK_RBUTTON))
    {
        POINT tMouse = pInput->Get_MousePos();

        float fWorldX, fWorldZ;
        CCamera::Get_Instance()->ScreenToIsoWorld(
            tMouse.x, tMouse.y,
            fWorldX, fWorldZ);

        m_fDestWorldX = fWorldX;
        m_fDestWorldZ = fWorldZ;
        m_bMoving = true;
    }*/


    if (m_bMoving)
    {
        Move_To_Dest(dt);
    }

    __super::Update(dt);

    Update_MouseCollider();

    return OBJ_NOEVENT;
}

void CMonster_Orc::Move_To_Dest(float dt)
{
    float fDX = m_fDestWorldX - m_tIsoInfo.fWorldX;
    float fDZ = m_fDestWorldZ - m_tIsoInfo.fWorldZ;

    float fDist = sqrtf(fDX * fDX + fDZ * fDZ);
    float fSpeed = m_fSpeed * dt;

    if (fDist <= fSpeed)
    {
        m_tIsoInfo.fWorldX = m_fDestWorldX;
        m_tIsoInfo.fWorldZ = m_fDestWorldZ;
        m_bMoving = false;

        if (m_eState != MON_IDLE)
            Motion_Change(MON_IDLE);

        return;
    }

    // 정규화
    float fNX = fDX / fDist;
    float fNZ = fDZ / fDist;
    // 이동
    m_tIsoInfo.fWorldX += fNX * fSpeed;
    m_tIsoInfo.fWorldZ += fNZ * fSpeed;

    Decide_Direction(fNX, fNZ);

    if (m_eState != MON_WALK)
        Motion_Change(MON_WALK);
}
void CMonster_Orc::Late_Update(float dt)
{
}

void CMonster_Orc::Render(ID2D1RenderTarget* pRT)
{

    ID2D1Bitmap* pBitmap = nullptr;
    switch (m_eState)
    {
    case MON_IDLE:      pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"ORC_IDLE");   break;
    case MON_WALK:      pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"ORC_WALK");   break;
    case MON_ATTACK_0:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"ORC_ATTACK0"); break;
    case MON_ATTACK_1:  pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"ORC_ATTACK1"); break;
    case MON_HIT:       pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"ORC_HIT");    break;
    case MON_DEAD:      pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"ORC_DEAD");   break;
    }

    Render_Sprite(pRT, pBitmap);
    __super::Render(pRT);

#ifdef GAME_DEBUG
    Debug_Render(pRT);
#endif
}

void CMonster_Orc::Release()
{
}

void CMonster_Orc::Motion_Change(MONSTER_STATE eState)
{
    m_eState = eState;
    switch (eState)
    {
    case MON_IDLE: {
        Set_Frame(7, 200);
        m_bLoopAnim = true;
        m_tIsoInfo.fCX = 133.f;
        m_tIsoInfo.fCY = 130.f;
        m_tIsoInfo.fHeight = 30.f;
    }
                 break;
    case MON_WALK:
    {
        Set_Frame(7, 120);
        m_bLoopAnim = true;
        m_tIsoInfo.fCX = 157.f;
        m_tIsoInfo.fCY = 141.f;
        m_tIsoInfo.fHeight = 30.f;
    }
    break;
    case MON_ATTACK_0: {
        Set_Frame(14, 100);
        m_tIsoInfo.fCX = 151.f;
        m_tIsoInfo.fCY = 146.f;
        m_tIsoInfo.fHeight = 30.f;
        m_bLoopAnim = false;
    }
                     break;
    case MON_ATTACK_1:
    {
        Set_Frame(14, 100);
        m_tIsoInfo.fCX = 220.f;
        m_tIsoInfo.fCY = 216.f;
        m_tIsoInfo.fHeight = 30.f;
        m_bLoopAnim = false;
    }
    break;
    case MON_HIT: {

        Set_Frame(6, 80);
        m_tIsoInfo.fCX = 177.f;
        m_tIsoInfo.fCY = 163.f;
        m_tIsoInfo.fHeight = 30.f;
        m_bLoopAnim = false;
    }
                break;
    case MON_DEAD: {
        Set_Frame(22, 150);
        m_tIsoInfo.fCX = 325.f;
        m_tIsoInfo.fCY = 206.f;
        m_tIsoInfo.fHeight = 30.f;
        m_bLoopAnim = false;
    }

                 break;
    default:
        break;
    }
}

void CMonster_Orc::Decide_Direction(float fNX, float fNZ)
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

void CMonster_Orc::Update_MouseCollider()
{
    POINT tScreen = CCamera::Get_Instance()->IsoWorldToScreen(
        m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ);

    float fLeft = tScreen.x - m_tIsoInfo.fCX * 0.5f;
    float fTop = tScreen.y - m_tIsoInfo.fCY - m_tIsoInfo.fHeight + TILE_HALF_H;

    m_tMouseRect.left = (LONG)fLeft;
    m_tMouseRect.top = (LONG)fTop;
    m_tMouseRect.right = (LONG)(fLeft + m_tIsoInfo.fCX);
    m_tMouseRect.bottom = (LONG)(fTop + m_tIsoInfo.fCY);
}

void CMonster_Orc::Direction_Change(DIRECTION eDir) {

    m_eDir = eDir;
    m_tFrame.iFrameStart = 0;
}

#ifdef GAME_DEBUG



void CMonster_Orc::Debug_Render(ID2D1RenderTarget* pRT)
{
    Debug_DrawCollider(pRT);
    Debug_DrawMouseCollider(pRT);

}

void CMonster_Orc::Debug_DrawCollider(ID2D1RenderTarget* pRT) {

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

void CMonster_Orc::Debug_DrawText(ID2D1RenderTarget* pRT) {


}

void CMonster_Orc::Debug_DrawMouseCollider(ID2D1RenderTarget* pRT) {

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
#endif 