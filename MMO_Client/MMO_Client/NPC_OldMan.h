#pragma once
#include "Npc.h"

// ================================================================
//  CNPC_OldMan  노인 NPC
//  - 상점 NPC(CNPC_Shop)와 동일한 상호작용 구조(좌클릭 On_Click).
//  - 이동/걷기 없음. 아래(정면) 방향 Idle 스트립만 사용(16프레임).
//  - 클릭 시: 특별 창 없이 말풍선만. (추후 역할 추가 예정)
// ================================================================
class CNPC_OldMan : public CNPC
{
public:
    CNPC_OldMan();
    virtual ~CNPC_OldMan();

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
