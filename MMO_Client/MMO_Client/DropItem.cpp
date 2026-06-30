#include "pch.h"
#include "DropItem.h"
#include "Img_Manager.h"
#include "Camera.h"
#include "Input_Manager.h"
#include "Inventory.h"
#include "ItemData.h"

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
    // 코드 → 아이콘 키 / 이름
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
    // 콜라이더(바닥) 중심에 아이콘 중심을 맞춘다 → 바닥에 놓인 것처럼
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

    IDWriteTextFormat* pFont = CImg_Manager::Get_Instance()->Get_DebugFont();
    if (!pFont) return;

    float fBoxW = 120.f, fBoxH = 22.f;
    float fBoxX = fIconX + fW / 2.f - fBoxW / 2.f;
    float fBoxY = fIconY - fBoxH - 2.f;

    ID2D1SolidColorBrush* pBg = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.6f), &pBg);
    if (pBg)
    {
        pRT->FillRectangle(
            D2D1::RectF(fBoxX, fBoxY, fBoxX + fBoxW, fBoxY + fBoxH), pBg);
        pBg->Release();
    }

    ID2D1SolidColorBrush* pText = nullptr;
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 0.4f), &pText);
    if (pText)
    {
        pRT->DrawText(m_szName, lstrlen(m_szName), pFont,
            D2D1::RectF(fBoxX, fBoxY, fBoxX + fBoxW, fBoxY + fBoxH), pText);
        pText->Release();
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
