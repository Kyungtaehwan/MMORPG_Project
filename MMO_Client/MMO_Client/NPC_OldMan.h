#pragma once
#include "Npc.h"

// ================================================================
//  CNPC_OldMan  노인 NPC

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
