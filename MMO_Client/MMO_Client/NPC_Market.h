#pragma once
#include "Npc.h"

// ================================================================
//  CNPC_Market  경매장 NPC
//  - 상점 NPC(CNPC_Shop)와 동일한 상호작용 구조(좌클릭 On_Click).
//  - Market.png(233x153) 단일 정적 이미지(애니메이션 없음).
//  - On_Click: 지금은 말풍선만. 경매장 UI는 다음 단계에서 연결.
// ================================================================
class CNPC_Market : public CNPC
{
public:
    CNPC_Market();
    virtual ~CNPC_Market();

public:
    virtual void Initialize()                   override;
    virtual int  Update(float dt)               override;
    virtual void Late_Update(float dt)          override;
    virtual void Render(ID2D1RenderTarget* pRT) override;
    virtual void Release()                      override;

    virtual void On_Click()                     override;
    virtual void On_Interact()                  override;
};
