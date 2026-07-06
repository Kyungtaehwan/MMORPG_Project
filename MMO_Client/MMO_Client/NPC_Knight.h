#pragma once
#include "Npc.h"

// ================================================================
//  CNPC_Knight  기사 NPC
//  - 상점 NPC(CNPC_Shop)와 동일한 상호작용 구조(좌클릭 On_Click).
//  - 이동/걷기 없음.
//    Idle : 아이들 블럭 맨 아래 행(좌하향) 16프레임.
//    Talk : Special 블럭(우하향) 36프레임 → 클릭 시 1회 재생 후 Idle 복귀.
//  - 클릭 시: 특별 창 없이 Talk 모션 + 말풍선. (추후 역할 추가 예정)
// ================================================================
class CNPC_Knight : public CNPC
{
public:
    CNPC_Knight();
    virtual ~CNPC_Knight();

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
};
