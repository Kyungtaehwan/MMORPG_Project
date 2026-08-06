#include "pch.h"
#include "DropItem.h"
#include "Img_Manager.h"
#include "Camera.h"
#include "Input_Manager.h"
#include "Inventory.h"
#include "ItemData.h"
#include "Object_Manager.h"
#include "Player.h"

void CDropItem::Init_Drop(int32_t nDropId, int32_t nItemCode, int32_t nAmount,
    float fX, float fZ)
{
    m_nDropId   = nDropId;
    m_nItemCode = nItemCode;
    m_nAmount   = nAmount;
    Set_WorldPos(fX, fZ);
}

void CDropItem::Initialize()
{
    // 코드 => 아이콘 키 / 이름
    if (m_nItemCode / 1000 == 9)      // 골드
    {
        lstrcpy(m_szIconKey, L"Gold");
        wsprintf(m_szName, L"골드 %d", m_nAmount);
    }
    else
    {
        CItemData* pTemp = CInventory::Create_ItemFromCode(m_nItemCode);
        if (pTemp)
        {
            lstrcpy(m_szIconKey, pTemp->Get_IconKey());
            lstrcpy(m_szName, pTemp->Get_Name());
            delete pTemp;
        }
    }

    m_tIsoInfo.fCX = 32.f;
    m_tIsoInfo.fCY = 32.f;
    m_tIsoInfo.fHeight = 0.f;

    // 접촉(획득) 판정용 콜라이더 (바닥 기준)
    Set_Collider(0.5f, 0.5f);
}

int CDropItem::Update(float dt)
{
    if (Get_Dead()) return OBJ_DEAD;
    return OBJ_NOEVENT;
}

void CDropItem::Late_Update(float dt)
{
}

void CDropItem::Render(ID2D1RenderTarget* pRT)
{
    //바닥 중심에 아이콘 중심을 맞춘다
    POINT tScreen = CCamera::Get_Instance()->IsoWorldToScreen(
        Get_ColliderX(), Get_ColliderZ());

    float fW = m_tIsoInfo.fCX;
    float fH = m_tIsoInfo.fCY;
    float fDrawX = tScreen.x - fW / 2.f;
    float fDrawY = tScreen.y - fH / 2.f;

    ID2D1Bitmap* pBmp = CImg_Manager::Get_Instance()->Find_Png(m_szIconKey);
    if (pBmp)
    {
        pRT->DrawBitmap(pBmp,
            D2D1::RectF(fDrawX, fDrawY, fDrawX + fW, fDrawY + fH));
    }

#ifdef GAME_DEBUG
    Render_Hitbox(pRT);
#endif

    // 아이콘 위에 마우스를 올리면 이름 표시
    Render_HoverName(pRT, fDrawX, fDrawY);
}

// 마우스가 아이콘 위에 있으면 아이템 이름을 띄움
void CDropItem::Render_HoverName(ID2D1RenderTarget* pRT, float fIconX, float fIconY)
{
    POINT tMouse = CInput_Manager::Get_Instance()->Get_MousePos();
    float fW = m_tIsoInfo.fCX;
    float fH = m_tIsoInfo.fCY;

    bool bHover =
        (tMouse.x >= fIconX && tMouse.x <= fIconX + fW &&
         tMouse.y >= fIconY && tMouse.y <= fIconY + fH);
    if (!bHover) return;

    // 인벤이 가득 차 못 줍는 상태면 이름 위에 한 줄 더 띄운다.
    //  골드는 인벤 칸을 안 쓰므로 항상 주울 수 있다.
    //  표시 전용 예측이고, 실제 거부는 서버가 한다(CZone::OnPlayerPickup).
    const TCHAR* pszWarn = nullptr;
    if (m_nItemCode / 1000 != 9)
    {
        CGameObject* pObj = CObject_Manager::Get_Instance()->Get_Player();
        CPlayer* pPlayer = static_cast<CPlayer*>(pObj);   // OBJ_PLAYER 목록엔 CPlayer만 들어간다
        if (pPlayer && pPlayer->Get_Inventory() &&
            !pPlayer->Get_Inventory()->Can_Add_Code(m_nItemCode))
        {
            pszWarn = L"인벤 가득참";
        }
    }

    CImg_Manager* pImg = CImg_Manager::Get_Instance();

    // 두 줄이 될 수 있으므로 더 긴 쪽에 폭을 맞춘다(좌우 패딩 14px)
    float fTextW = pImg->Measure_TextWidth(m_szName);
    if (pszWarn)
    {
        float fWarnW = pImg->Measure_TextWidth(pszWarn);
        if (fWarnW > fTextW) fTextW = fWarnW;
    }
    float fBoxW = fTextW + 14.f;
    if (fBoxW < 28.f) fBoxW = 28.f;   // 최소 폭

    const float fLineH = 22.f;
    float fBoxH = pszWarn ? fLineH * 2.f : fLineH;
    float fBoxX = fIconX + fW / 2.f - fBoxW / 2.f;
    float fBoxY = fIconY - fBoxH - 2.f;
    D2D1_RECT_F rcBox = D2D1::RectF(fBoxX, fBoxY, fBoxX + fBoxW, fBoxY + fBoxH);

    ID2D1SolidColorBrush* pBg = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.6f), &pBg);
    if (pBg)
    {
        pRT->FillRectangle(rcBox, pBg);
        pBg->Release();
    }

    if (pszWarn)
    {
        // 위: 경고(빨강) / 아래: 아이템 이름
        D2D1_RECT_F rcWarn = D2D1::RectF(fBoxX, fBoxY, fBoxX + fBoxW, fBoxY + fLineH);
        D2D1_RECT_F rcName = D2D1::RectF(fBoxX, fBoxY + fLineH, fBoxX + fBoxW, fBoxY + fBoxH);
        pImg->Draw_Text_Center(pRT, pszWarn, rcWarn, D2D1::ColorF(1.f, 0.3f, 0.3f));
        pImg->Draw_Text_Center(pRT, m_szName, rcName, D2D1::ColorF(1.f, 1.f, 0.4f));
    }
    else
    {
        pImg->Draw_Text_Center(pRT, m_szName, rcBox, D2D1::ColorF(1.f, 1.f, 0.4f));
    }
}

// 디버그: 바닥 콜라이더(획득 판정 영역) 표시
void CDropItem::Render_Hitbox(ID2D1RenderTarget* pRT)
{
    float fCX = Get_ColliderX();
    float fCZ = Get_ColliderZ();
    float fRX = m_tCollider.fRadiusX;
    float fRZ = m_tCollider.fRadiusZ;

    CCamera* pCam = CCamera::Get_Instance();
    POINT tTL = pCam->IsoWorldToScreen(fCX - fRX, fCZ - fRZ);
    POINT tTR = pCam->IsoWorldToScreen(fCX + fRX, fCZ - fRZ);
    POINT tBR = pCam->IsoWorldToScreen(fCX + fRX, fCZ + fRZ);
    POINT tBL = pCam->IsoWorldToScreen(fCX - fRX, fCZ + fRZ);

    ID2D1SolidColorBrush* pBrush = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 0.5f, 0.f), &pBrush);
    if (!pBrush) return;

    auto P = [](POINT p) { return D2D1::Point2F((float)p.x, (float)p.y); };
    pRT->DrawLine(P(tTL), P(tTR), pBrush, 2.f);
    pRT->DrawLine(P(tTR), P(tBR), pBrush, 2.f);
    pRT->DrawLine(P(tBR), P(tBL), pBrush, 2.f);
    pRT->DrawLine(P(tBL), P(tTL), pBrush, 2.f);
    pBrush->Release();
}

void CDropItem::Release()
{
}
