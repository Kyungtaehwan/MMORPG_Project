#include "pch.h"
#include "UI_ZoneSelect.h"
#include "RaidMap.h"        // RAID_SPAWN_WORLD_X/Z
#include "Img_Manager.h"
#include "Input_Manager.h"
#include "Network_Manager.h"
#include "Map_Manager.h"
#include "Object_Manager.h"
#include "Player.h"

void CUI_ZoneSelect::Initialize()
{
    m_bVisible = false;
    Set_Pos(WINCX * 0.5f, WINCY * 0.5f);
    Set_Size(BOX_W, BOX_H);
    Update_Rect();
}

// 입력 모드를 UI로 돌려야 CPlayer::Update 가 클릭을 소비하지 않는다(헤더 주석 참고).
void CUI_ZoneSelect::Open()
{
    m_bVisible = true;
    CInput_Manager::Get_Instance()->Set_InputMode(INPUT_MODE_UI);
    CInput_Manager::Get_Instance()->Set_CursorMode(CURSOR_UI);
}

void CUI_ZoneSelect::Close()
{
    m_bVisible = false;
    CInput_Manager::Get_Instance()->Set_InputMode(INPUT_MODE_GAME);
}

int CUI_ZoneSelect::Update(float dt)
{
    if (!m_bVisible) return UI_NOEVENT;

    CInput_Manager* pInput = CInput_Manager::Get_Instance();
    pInput->Set_CursorMode(CURSOR_UI);

    POINT tMouse = pInput->Get_MousePos();

    if (pInput->Key_Down(VK_ESCAPE))   // ESC = 닫기만 (이동 아님)
    {
        Close();
        return UI_NOEVENT;
    }

    if (pInput->Key_Down(VK_LBUTTON))
    {
        RECT rc = Rc_Raid();
        if (PtInRect(&rc, tMouse)) { Enter_Zone(ZONE_RAID);      return UI_NOEVENT; }

        rc = Rc_Flat();
        if (PtInRect(&rc, tMouse)) { Enter_Zone(ZONE_RAID_FLAT); return UI_NOEVENT; }

        rc = Rc_Close();
        if (PtInRect(&rc, tMouse)) { Close();                    return UI_NOEVENT; }
    }
    return UI_NOEVENT;
}

void CUI_ZoneSelect::Late_Update(float dt) {}
void CUI_ZoneSelect::Release() {}

// 포탈과 동일한 서버 권위 경로. 서버가 존을 바꾸고 SC_CHANGE_ZONE 으로 맵 로드를 구동한다.
void CUI_ZoneSelect::Enter_Zone(ZONE_ID eZone)
{
    Close();

    CNetwork_Manager* pNet = CNetwork_Manager::Get_Instance();
    if (pNet->IsConnected())
    {
        pNet->SendPortal(eZone, RAID_SPAWN_WORLD_X, RAID_SPAWN_WORLD_Z);
    }
    else
    {
        // 오프라인 폴백 (서버 미연결)
        CMap_Manager::Get_Instance()->Change_Zone_Async(eZone);
        CPlayer* pPlayer = dynamic_cast<CPlayer*>(
            CObject_Manager::Get_Instance()->Get_Player());
        if (pPlayer)
            pPlayer->Set_WorldPos(RAID_SPAWN_WORLD_X, RAID_SPAWN_WORLD_Z);
    }
}

void CUI_ZoneSelect::Render(ID2D1RenderTarget* pRT)
{
    if (!m_bVisible) return;

    CImg_Manager* pImg = CImg_Manager::Get_Instance();
    POINT tMouse = CInput_Manager::Get_Instance()->Get_MousePos();
    ID2D1SolidColorBrush* pBrush = nullptr;

    // 모달 배경 어둡게
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.45f), &pBrush);
    pRT->FillRectangle(D2D1::RectF(0.f, 0.f, (float)WINCX, (float)WINCY), pBrush);
    pBrush->Release();

    RECT box = Rc_Box();
    D2D1_RECT_F rb = D2D1::RectF((float)box.left, (float)box.top,
                                 (float)box.right, (float)box.bottom);

    pRT->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.93f), &pBrush);
    pRT->FillRoundedRectangle(D2D1::RoundedRect(rb, 10, 10), pBrush);
    pBrush->Release();

    pRT->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.55f, 0.6f, 0.9f), &pBrush);
    pRT->DrawRoundedRectangle(D2D1::RoundedRect(rb, 10, 10), pBrush, 1.5f);
    pBrush->Release();

    pImg->Draw_Text_Center(pRT, L"어디로 인도할까요?",
        D2D1::RectF((float)box.left, (float)box.top + 16.f,
                    (float)box.right, (float)box.top + 44.f),
        D2D1::ColorF(1.f, 0.95f, 0.6f));

    Draw_Btn(pRT, Rc_Raid(), L"시련의 땅",     tMouse, true);
    Draw_Btn(pRT, Rc_Flat(), L"끝없는 평원",   tMouse, true);
    Draw_Btn(pRT, Rc_Close(), L"닫기",         tMouse, false);
}

RECT CUI_ZoneSelect::Rc_Box() const
{
    float l = WINCX * 0.5f - BOX_W * 0.5f;
    float t = WINCY * 0.5f - BOX_H * 0.5f;
    return RECT{ (LONG)l, (LONG)t, (LONG)(l + BOX_W), (LONG)(t + BOX_H) };
}

RECT CUI_ZoneSelect::Rc_Raid() const
{
    RECT b = Rc_Box();
    LONG y = b.top + 62;
    return RECT{ b.left + 40, y, b.right - 40, y + 40 };
}

RECT CUI_ZoneSelect::Rc_Flat() const
{
    RECT b = Rc_Box();
    LONG y = b.top + 112;
    return RECT{ b.left + 40, y, b.right - 40, y + 40 };
}

RECT CUI_ZoneSelect::Rc_Close() const
{
    RECT b = Rc_Box();
    LONG y = b.bottom - 44;
    return RECT{ b.right - 120, y, b.right - 40, y + 30 };
}

void CUI_ZoneSelect::Draw_Btn(ID2D1RenderTarget* pRT, const RECT& r,
    const TCHAR* szLabel, POINT tMouse, bool bAccent)
{
    ID2D1SolidColorBrush* pBrush = nullptr;
    bool bHover = PtInRect(&r, tMouse) ? true : false;

    D2D1_RECT_F rc = D2D1::RectF((float)r.left, (float)r.top,
                                 (float)r.right, (float)r.bottom);

    D2D1::ColorF bg = bAccent
        ? (bHover ? D2D1::ColorF(0.20f, 0.55f, 0.25f) : D2D1::ColorF(0.14f, 0.42f, 0.18f))
        : (bHover ? D2D1::ColorF(0.40f, 0.40f, 0.45f) : D2D1::ColorF(0.24f, 0.24f, 0.28f));

    pRT->CreateSolidColorBrush(bg, &pBrush);
    pRT->FillRoundedRectangle(D2D1::RoundedRect(rc, 6, 6), pBrush);
    pBrush->Release();

    CImg_Manager::Get_Instance()->Draw_Text_Center(pRT, szLabel, rc,
        D2D1::ColorF(1.f, 1.f, 1.f));
}
