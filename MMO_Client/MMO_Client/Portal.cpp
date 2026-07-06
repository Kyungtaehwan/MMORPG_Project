#include "pch.h"
#include "Portal.h"
#include "Map_Manager.h"
#include "Object_Manager.h"
#include "Player.h"
#include "Camera.h"
#include "Img_Manager.h"
#include "Network_Manager.h"


void CPortal::Initialize()
{
    __super::Initialize();

    CImg_Manager* pImg = CImg_Manager::Get_Instance();
    pImg->Insert_Png(L"../Resource/Object/Portal/portal.png", L"PORTAL_FRAME");
    pImg->Insert_Png(L"../Resource/Object/Portal/portaleffect(109X30).png", L"PORTAL_GATE");

    // 게이트 실체 세팅 (NPC의 Render_Sprite 방식과 동일)
    m_tIsoInfo.fCX = 109.f;   // 게이트 프레임 1장 가로 (3270 / 30)
    m_tIsoInfo.fCY = 176.f;   // 게이트 세로
    m_tIsoInfo.fHeight = 0.f;

    // 30프레임 애니메이션
    Set_Frame(29, 80);
    m_bLoopAnim = true;

    // 충돌/클릭은 테두리 기준
    Set_Collider(1.5f, 1.5f);
    Set_MouseCollider(0.f, 0.f, m_tIsoInfo.fCX, m_tIsoInfo.fCY);

    // 렌더 깊이 정렬 보정.
    // 게이트 바닥(땅에 닿는 지점)은 월드 점보다 TILE_HALF_H(약 1타일)
    // 앞/아래에 그려진다. 그래서 정렬 기준을 그 바닥으로 1타일 당겨준다.
    // 이렇게 하면 플레이어가 포탈 바닥보다 앞에 있을 때만 포탈 앞에 그려진다.
    // (포탈에 너무 가려지면 값을 0.7~0.8로 낮추면 됨)
    Set_SortOffset(1.5f);
}

int CPortal::Update(float dt)
{
    if (m_bActivated)
    {
        m_fActivateDelay -= dt;
        if (m_fActivateDelay <= 0.f)
        {
            CNetwork_Manager* pNet = CNetwork_Manager::Get_Instance();
            if (pNet->IsConnected())
            {
                // 서버 권위적: 서버가 우리를 이동시키고
                // SC_CHANGE_ZONE으로 실제 존 전환을 구동
                pNet->SendPortal(m_eTargetZone, m_fSpawnX, m_fSpawnZ);
            }
            else
            {
                // 오프라인 폴백 (서버 미연결)
                CMap_Manager::Get_Instance()->Change_Zone_Async(m_eTargetZone);

                CPlayer* pPlayer = (CPlayer*)CObject_Manager::Get_Instance()
                    ->Get_Player();
                if (pPlayer)
                    pPlayer->Set_WorldPos(m_fSpawnX, m_fSpawnZ);
            }

            m_bActivated = false;
        }
    }
    __super::Update(dt);
    return OBJ_NOEVENT;

}

void CPortal::Late_Update(float dt)
{
    __super::Late_Update(dt);
}

void CPortal::Render(ID2D1RenderTarget* pRT)
{
    POINT tScreen = CCamera::Get_Instance()->IsoWorldToScreen(
        m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ);

    Render_Frame(pRT, tScreen);
    Render_Gate(pRT, tScreen);

    __super::Render(pRT);

#ifdef GAME_DEBUG
    Debug_Render(pRT);
#endif
}

// 게이트 - NPC Render_Sprite 방식과 동일
void CPortal::Render_Gate(ID2D1RenderTarget* pRT, POINT tScreen)
{
    ID2D1Bitmap* pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PORTAL_GATE");
    if (!pBitmap) return;

    float fWidth = m_tIsoInfo.fCX;
    float fHeight = m_tIsoInfo.fCY;

    // 테두리 중앙에 맞추기
    float fDrawX = tScreen.x - fWidth / 2.f;
    float fDrawY = tScreen.y - fHeight - m_tIsoInfo.fHeight + TILE_HALF_H;

    // 현재 프레임 소스 영역
    float fSrcX = m_tIsoInfo.fCX * m_tFrame.iFrameStart;

    pRT->DrawBitmap(pBitmap,
        D2D1::RectF(fDrawX, fDrawY, fDrawX + fWidth, fDrawY + fHeight),
        0.7f,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
        D2D1::RectF(fSrcX, 0.f, fSrcX + m_tIsoInfo.fCX, m_tIsoInfo.fCY)
    );
}

// 테두리 - 그냥 게이트 위에 얹는 껍데기
void CPortal::Render_Frame(ID2D1RenderTarget* pRT, POINT tScreen)
{
    ID2D1Bitmap* pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"PORTAL_FRAME");
    if (!pBitmap) return;

    // 테두리도 게이트 기준으로 중앙 정렬
    float fDrawX = tScreen.x - BORDER_CX / 2.f+5.f;
    float fDrawY = tScreen.y - BORDER_CY - m_tIsoInfo.fHeight + TILE_HALF_H + 50.f;

    pRT->DrawBitmap(pBitmap,
        D2D1::RectF(fDrawX, fDrawY, fDrawX + BORDER_CX, fDrawY + BORDER_CY),
        1.0f,
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR
    );
}

void CPortal::On_Click()
{
    if (m_bActivated)              return;
    if (m_eTargetZone == ZONE_MAX) return;

    m_bActivated = true;
    m_fActivateDelay = 0.5f;
}

void CPortal::Render_Indicator(ID2D1RenderTarget* pRT)
{
    if (!m_bInteractable) return;

    POINT tScreen = CCamera::Get_Instance()->IsoWorldToScreen(
        m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ);

    TCHAR szText[] = L"[좌클릭] 이동";
    ID2D1SolidColorBrush* pBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 0.f), &pBrush);
    pRT->DrawText(szText, lstrlen(szText),
        CImg_Manager::Get_Instance()->Get_DebugFont(),
        D2D1::RectF(
            (float)tScreen.x - 40.f,
            (float)tScreen.y - BORDER_CY - 20.f,
            (float)tScreen.x + 40.f,
            (float)tScreen.y - BORDER_CY),
        pBrush);
    pBrush->Release();
}

void CPortal::Release() {}

void CPortal::On_Interact()
{
    // On_Click과 동일하게 처리하거나
    // 추후 E키 상호작용 등 별도 처리
    On_Click();
}

void CPortal::On_PlayerNear()
{
    // 플레이어 접근 시 포탈 연출
    // 추후: 포탈 빛나기, 사운드 등
}

void CPortal::On_PlayerFar()
{
    // 플레이어 멀어질 시
    // 추후: 연출 끄기
}


#ifdef GAME_DEBUG
void CPortal::Debug_Render(ID2D1RenderTarget* pRT)
{
    Debug_DrawCollider(pRT);
    Debug_DrawMouseCollider(pRT);
}

void CPortal::Debug_DrawCollider(ID2D1RenderTarget* pRT)
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
    pRT->CreateSolidColorBrush(
        m_bInteractable
        ? D2D1::ColorF(1.f, 0.f, 0.f)
        : D2D1::ColorF(0.f, 0.8f, 1.f),
        &pBrush);

    auto P = [](POINT p) { return D2D1::Point2F((float)p.x, (float)p.y); };
    pRT->DrawLine(P(tTL), P(tTR), pBrush, 2.f);
    pRT->DrawLine(P(tTR), P(tBR), pBrush, 2.f);
    pRT->DrawLine(P(tBR), P(tBL), pBrush, 2.f);
    pRT->DrawLine(P(tBL), P(tTL), pBrush, 2.f);
    pBrush->Release();
}

void CPortal::Debug_DrawMouseCollider(ID2D1RenderTarget* pRT)
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
#endif