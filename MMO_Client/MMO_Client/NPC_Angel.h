#pragma once
#include "Npc.h"

// ================================================================
//  CNPC_Angel  천사 NPC
//  - 상점 NPC(CNPC_Shop)와 동일한 상호작용 구조(좌클릭 On_Click).
//  - 이동/걷기 없음. 아래(정면) 방향 Idle 스트립만 사용(24프레임).
//  - 클릭 시: 이펙트(빛나는 날개) + 말풍선 + 대형 맵 선택창(CUI_ZoneSelect) 열기.
//    실제 이동은 선택창이 CS_PORTAL 로 처리한다(포탈과 같은 서버 권위 경로).
//    NPC_Shop → Open_Shop, NPC_Market → Open_Auction 과 동일한 패턴.
// ================================================================
class CNPC_Angel : public CNPC
{
public:
    CNPC_Angel();
    virtual ~CNPC_Angel();

public:
    virtual void Initialize()                   override;
    virtual int  Update(float dt)               override;
    virtual void Late_Update(float dt)          override;
    virtual void Render(ID2D1RenderTarget* pRT) override;
    virtual void Release()                      override;

    virtual void On_Click()                     override;
    virtual void On_Interact()                  override;

private:
    void Motion_Change(NPC_STATE eState);
    void Render_Effect(ID2D1RenderTarget* pRT);   // NPC 뒤 이펙트

private:
    // 이펙트(메인 프레임과 독립적으로 재생)
    bool  m_bEffectActive = false;
    float m_fEffectTimer = 0.f;         // 남은 표시 시간(초)
    float m_fEffectFrameTimer = 0.f;
    int   m_iEffectFrame = 0;

    static const int   s_iEffectFrameCnt = 24;
    static const float s_fEffectCX;
    static const float s_fEffectCY;
    static const float s_fEffectFrameSec;
    static const float s_fEffectShowSec;
};
