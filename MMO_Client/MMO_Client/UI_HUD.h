#pragma once
#include "UI.h"

class CPlayer;

class CUI_HUD : public CUI
{
public:
    CUI_HUD() = default;
    virtual ~CUI_HUD() = default;

public:
    virtual void    Initialize()                    override;
    virtual int     Update(float dt)                override;
    virtual void    Late_Update(float dt)           override;
    virtual void    Render(ID2D1RenderTarget* pRT)  override;
    virtual void    Release()                       override;
    virtual void    Process_Event()                 override {}

public:
    void    Set_Player(CPlayer* pPlayer) { m_pPlayer = pPlayer; }

private:
    void    Render_Bar(ID2D1RenderTarget* pRT, const TCHAR* szKey,
        float fX, float fY, float fW, float fH, float fRatio);
    // 버프 아이콘 박스 (경과 시간만큼 검은색으로 덮임)
    void    Render_Buffs(ID2D1RenderTarget* pRT, float fBaseX, float fBaseY);

private:
    CPlayer* m_pPlayer = nullptr;

    // 리소스 원본 사이즈에 맞게 조정
    static constexpr float PANEL_W = 537.f;
    static constexpr float PANEL_H = 172.f;
    static constexpr float BAR_W = 315.f;  // 실제 바 길이
    static constexpr float BAR_H = 9.f;

    // 프레임 내 바 위치 (이미지 보고 조정)
    static constexpr float EXP_X = 128.f;
    static constexpr float EXP_Y = 56.f;
    static constexpr float HP_X = 204.f;
    static constexpr float HP_Y = 104.f;
    static constexpr float MP_X = 204.f;
    static constexpr float MP_Y = 140.f;
};