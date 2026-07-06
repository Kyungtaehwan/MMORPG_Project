#include "pch.h"
#include "NPC_Market.h"
#include "Img_Manager.h"
#include "Camera.h"
#include "UI_Manager.h"

CNPC_Market::CNPC_Market() {}
CNPC_Market::~CNPC_Market() { Release(); }

void CNPC_Market::Initialize()
{
    __super::Initialize();

    m_eType = NPC_MARKET;
    Set_NpcName(L"경매장");

    // Market.png = 233x153 단일 정적 이미지
    m_fScale = 1.0f;
    m_tIsoInfo.fCX = 233.f;
    m_tIsoInfo.fCY = 153.f;
    m_tIsoInfo.fHeight = 20.f;

    Set_Collider(1.5f, 1.5f);
    Set_MouseCollider(0.f, 0.f, m_tIsoInfo.fCX, m_tIsoInfo.fCY);

    // 단일 프레임(0번) 고정 → 정적 렌더
    Set_Frame(0, 1000);
    m_bLoopAnim = false;
}

int CNPC_Market::Update(float dt)
{
    __super::Update(dt);   // 좌클릭 상호작용/커서/말풍선 처리(베이스)
    return OBJ_NOEVENT;
}

void CNPC_Market::Late_Update(float dt)
{
    __super::Late_Update(dt);
}

void CNPC_Market::Render(ID2D1RenderTarget* pRT)
{
    ID2D1Bitmap* pBitmap = CImg_Manager::Get_Instance()->Find_Png(L"MARKET_IDLE");
    Render_Sprite(pRT, pBitmap);   // 스프라이트
    __super::Render(pRT);          // 이름표/인디케이터/말풍선/디버그
}

void CNPC_Market::On_Click()
{
    m_bClick = true;

    // 말풍선
    m_bShowBubble = true;
    m_fBubbleTimer = 3.f;
    lstrcpy(m_szBubbleText, L"경매장에 오신 걸 환영합니다!");

    // 경매장 UI 열기 (INPUT_MODE_UI 전환 → 플레이어 정지)
    CUI_Manager::Get_Instance()->Open_Auction();
}

void CNPC_Market::On_Interact()
{
    // 추후: 경매장 UI 표시
}

void CNPC_Market::Release() {}
