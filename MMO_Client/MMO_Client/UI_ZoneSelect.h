#pragma once
#include "UI.h"

// ================================================================
//  CUI_ZoneSelect — 천사 NPC의 대형 맵 선택창
//
//  천사를 클릭하면 열리고, 두 대형 맵 중 하나를 고르면 CS_PORTAL 을 보낸다.
//  (포탈과 동일한 서버 권위 경로 — 서버가 존을 바꾸고 SC_CHANGE_ZONE 으로 클라를 구동)
// ================================================================
class CUI_ZoneSelect : public CUI
{
public:
    CUI_ZoneSelect() = default;
    virtual ~CUI_ZoneSelect() = default;

public:
    virtual void    Initialize()                    override;
    virtual int     Update(float dt)                override;
    virtual void    Late_Update(float dt)           override;
    virtual void    Render(ID2D1RenderTarget* pRT)  override;
    virtual void    Release()                       override;
    virtual void    Process_Event()                 override {}

public:
    void    Open();
    void    Close();
    bool    Is_Open() const { return m_bVisible; }

private:
    RECT    Rc_Box()   const;
    RECT    Rc_Raid()  const;   // 시련의 땅 (장애물)
    RECT    Rc_Flat()  const;   // 끝없는 평원 (평지)
    RECT    Rc_Close() const;
    void    Draw_Btn(ID2D1RenderTarget* pRT, const RECT& r,
                     const TCHAR* szLabel, POINT tMouse, bool bAccent);
    void    Enter_Zone(ZONE_ID eZone);

private:
    bool    m_bVisible = false;

    static constexpr float BOX_W = 420.f;
    static constexpr float BOX_H = 210.f;
};
