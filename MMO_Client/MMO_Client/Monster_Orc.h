#pragma once
#include "Monster.h"

class CMonster_Orc : public CMonster
{
public:
    CMonster_Orc() = default;
    virtual ~CMonster_Orc() = default;

public:
    virtual void Initialize()                        override;
    virtual int  Update(float dt)                override;
    virtual void Late_Update(float dt)               override;
    virtual void Render(ID2D1RenderTarget* pRT)  override;
    virtual void Release()                        override;
    virtual void Motion_Change(MONSTER_STATE eState) override;

protected:
    // 패킷 수신 후 오크 전용 처리
    virtual void On_MovePacket(uint8_t nDir)                            override;
    virtual void On_StatePacket(MONSTER_STATE eState, int32_t nTargetID) override;

private:
    void Move_To_Dest(float dt);
    void Decide_Direction(float fNX, float fNZ);
    void Update_MouseCollider();

#ifdef GAME_DEBUG
    void Debug_Render(ID2D1RenderTarget* pRT);
    void Debug_DrawCollider(ID2D1RenderTarget* pRT);
    void Debug_DrawMouseCollider(ID2D1RenderTarget* pRT);
    void Debug_DrawText(ID2D1RenderTarget* pRT);
#endif
};