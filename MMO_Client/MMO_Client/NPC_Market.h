#pragma once
#include "Npc.h"

// ================================================================
//  CNPC_Market  경매장 NPC
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
