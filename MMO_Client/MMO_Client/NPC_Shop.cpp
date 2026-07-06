#include "pch.h"
#include "Npc_Shop.h"
#include "Img_Manager.h"
#include "Camera.h"
#include "UI_Manager.h"

// 트레이더별 스프라이트 정보
// TRADER_0: Idle 107x71x6ea / Talk 107x71x9ea
// TRADER_1: Idle 108x68x6ea / Talk 108x68x9ea

CNPC_Shop::CNPC_Shop() {}
CNPC_Shop::~CNPC_Shop() { Release(); }

void CNPC_Shop::Initialize()
{
    __super::Initialize();

    m_eType = NPC_SHOP;
    m_eShopState = SHOP_IDLE;
    m_fScale = 1.3f;
    // 트레이더 타입에 따라 스프라이트 크기 설정
    switch (m_eShopType)
    {
    case SHOP_TRADER_0:
        Set_NpcName(L"상인");
        m_tIsoInfo.fCX = 107.f;
        m_tIsoInfo.fCY = 71.f;
        m_tIsoInfo.fHeight = 20.f;
        break;

    case SHOP_TRADER_1:
        Set_NpcName(L"상인 1");
        m_tIsoInfo.fCX = 108.f;
        m_tIsoInfo.fCY = 68.f;
        m_tIsoInfo.fHeight = 20.f;
        break;
    }

    Set_Collider(1.5f, 1.5f);
    Set_MouseCollider(
        0.f,
        0.f,
        m_tIsoInfo.fCX,
        m_tIsoInfo.fCY
    );

    Motion_Change(SHOP_IDLE);
}


void CNPC_Shop::Motion_Change(SHOP_STATE eState)
{
    m_eShopState = eState;

    switch (m_eShopType)
    {
    case SHOP_TRADER_0:
        switch (eState)
        {
            case SHOP_IDLE:
            {
                Set_Frame(5, 120);
                m_bLoopAnim = true;
            }
                break;  // 6ea → 0~5
            case SHOP_TALK:
            {
                Set_Frame(8, 100);  // 9ea → 0~8
                m_bLoopAnim = false;
                
            }
            break;
        }
        break;

    case SHOP_TRADER_1:
        switch (eState)
        {
        case SHOP_IDLE: Set_Frame(5, 120); break;  // 6ea → 0~5
        case SHOP_TALK: Set_Frame(8, 100); break;  // 9ea → 0~8
        }
        break;
    }
}

const TCHAR* CNPC_Shop::Get_IdleKey() const
{
    switch (m_eShopType)
    {
    case SHOP_TRADER_0: return L"TRADER0_IDLE";
    case SHOP_TRADER_1: return L"TRADER1_IDLE";
    }
    return nullptr;
}

const TCHAR* CNPC_Shop::Get_TalkKey() const
{
    switch (m_eShopType)
    {
    case SHOP_TRADER_0: return L"TRADER0_TALK";
    case SHOP_TRADER_1: return L"TRADER1_TALK";
    }
    return nullptr;
}

int CNPC_Shop::Update(float dt)
{
    __super::Update(dt);
    if (m_eShopState == SHOP_TALK && !m_bLoopAnim)
    {
        if (m_tFrame.iFrameStart == m_tFrame.iFrameEnd)
        {
            Motion_Change(SHOP_IDLE);
        }
    }
    return OBJ_NOEVENT;
}

void CNPC_Shop::Late_Update(float dt)
{
    __super::Late_Update(dt);
}

void CNPC_Shop::Render(ID2D1RenderTarget* pRT)
{
    // 현재 상태에 따라 스프라이트 선택
    ID2D1Bitmap* pBitmap = nullptr;
    switch (m_eShopState)
    {
    case SHOP_IDLE:
        pBitmap = CImg_Manager::Get_Instance()->Find_Png(Get_IdleKey());
        break;
    case SHOP_TALK:
        pBitmap = CImg_Manager::Get_Instance()->Find_Png(Get_TalkKey());
        break;
    }

    Render_Sprite(pRT, pBitmap);  // 스프라이트 출력
    __super::Render(pRT);         // 이름표, 인디케이터, 디버그

#ifdef GAME_DEBUG
    Debug_Render(pRT);
#endif
}

void CNPC_Shop::On_Click()
{
    // Talk 상태로 전환
    Motion_Change(SHOP_TALK);
    m_bClick = true;
    // 말풍선
    m_bShowBubble = true;
    m_fBubbleTimer = 3.f;
    lstrcpy(m_szBubbleText, L"어서오세요!");

    // 상점 UI 열기 (INPUT_MODE_UI 전환 → 플레이어 정지)
    CUI_Manager::Get_Instance()->Open_Shop(m_eShopType);
}




void CNPC_Shop::On_Interact()
{
    // 추후: 상점 UI 표시
}

void CNPC_Shop::Release() {}

// ===================== 디버그 =====================
#ifdef GAME_DEBUG
void CNPC_Shop::Debug_Render(ID2D1RenderTarget* pRT)
{
    Debug_DrawCollider(pRT);
    Debug_DrawMouseCollider(pRT);
    Debug_DrawText(pRT);
}

// 상호작용 범위 - 아이소메트릭 마름모
// 평소 초록, 인터렉티블이면 빨강
void CNPC_Shop::Debug_DrawCollider(ID2D1RenderTarget* pRT)
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
    if (m_bInteractable)
        pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 0.f, 0.f), &pBrush); // 빨강
    else
        pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 1.f, 0.f), &pBrush); // 초록

    auto P = [](POINT p) { return D2D1::Point2F((float)p.x, (float)p.y); };

    pRT->DrawLine(P(tTL), P(tTR), pBrush, 2.f);
    pRT->DrawLine(P(tTR), P(tBR), pBrush, 2.f);
    pRT->DrawLine(P(tBR), P(tBL), pBrush, 2.f);
    pRT->DrawLine(P(tBL), P(tTL), pBrush, 2.f);

    pBrush->Release();
}

// 마우스 클릭 박스 - 직교 사각형 (노란색)
void CNPC_Shop::Debug_DrawMouseCollider(ID2D1RenderTarget* pRT)
{
    ID2D1SolidColorBrush* pBrush = nullptr;
    if (m_bClick)
        pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 0.f, 0.f), &pBrush); // 빨강
    else
        pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 1.f, 0.f), &pBrush); // 초록


    pRT->DrawRectangle(
        D2D1::RectF(
            (float)m_tMouseRect.left,
            (float)m_tMouseRect.top,
            (float)m_tMouseRect.right,
            (float)m_tMouseRect.bottom
        ),
        pBrush, 2.f
    );

    pBrush->Release();
}

void CNPC_Shop::Debug_DrawText(ID2D1RenderTarget* pRT)
{
    TCHAR szBuf[256];
    swprintf_s(szBuf, 256,
        L"NPC[%s] 월드:[%.2f, %.2f] 상태:%s 인터렉티블:%s",
        m_szName,
        m_tIsoInfo.fWorldX, m_tIsoInfo.fWorldZ,
        m_eShopState == SHOP_IDLE ? L"IDLE" : L"TALK",
        m_bInteractable ? L"ON" : L"OFF"
    );

    ID2D1SolidColorBrush* pBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 0.5f, 0.f), &pBrush); // 주황

    pRT->DrawText(szBuf, wcslen(szBuf),
        CImg_Manager::Get_Instance()->Get_DebugFont(),
        D2D1::RectF(800.f, 60.f, 1200.f, 80.f),
        pBrush);

    pBrush->Release();
}
#endif