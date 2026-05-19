#pragma once
#include "GameObject.h"
#include "Inventory.h"
#include "Equipment.h"
#include "PathFinder.h"

class CItemData_UseItem;

#define PLAYER_FOOT_OFFSET_Y 0.5f



struct CLICK_EFFECT
{
    float    fWorldX;
    float    fWorldZ;
    float    fScale;
    bool     bActive;
    D2D1_COLOR_F color;
};

class CPlayer : public CGameObject
{
public:
    CPlayer();
    virtual ~CPlayer();
public:
    virtual void Initialize()        override;
    virtual int  Update(float dt)    override;
    virtual void Late_Update(float dt) override;
    virtual void Render(ID2D1RenderTarget* pRT) override; // HDC → RT
    virtual void Release(void)       override;


private:
    void Render_Sprite(ID2D1RenderTarget* pRT, ID2D1Bitmap* pBitmap);
    void RenderIDLE(ID2D1RenderTarget* pRT);
    void RenderWALK(ID2D1RenderTarget* pRT);
    void RenderHIT(ID2D1RenderTarget* pRT);
    void RenderATTACK(ID2D1RenderTarget* pRT);
    void RenderDEAD(ID2D1RenderTarget* pRT);
    void Check_AnimEnd();
    void Render_ClickEffect(ID2D1RenderTarget* pRT);

    void Motion_Change(PLAYER_STATE eState);
    void Direction_Change(DIRECTION eDir);
    void Key_Input(float dt);
    void Move_To_Dest(float dt);
    void Decide_Direction(float fNX, float fNZ);
    void Update_ClickEffect(float dt);

public:
    void Set_AttackTarget(int32_t nMonsterID) { m_nAttackTargetID = nMonsterID; }
    void Update_AttackTarget();

    void Hit();
    void Die();
    void Respawn(float fX, float fZ);
private:
    PLAYER_STATE m_eCurState;
    CLICK_EFFECT m_tClickEffect = {};
    float        m_fDestWorldX = 0.f;
    float        m_fDestWorldZ = 0.f;
    bool         m_bMoving = false;
   
    int32_t m_nLastTileX;
    int32_t m_nLastTileZ;
    
    int32_t  m_nAttackTargetID = -1;
    float    m_fAttackRange = 1.5f;
    uint32_t m_nLastAtkTime = 0;
    uint32_t m_nAtkCoolMs   = 500;

    bool m_bHit = false;


public:
    // 아이템 관련
    void        Use_Item(int iSlot);
    void        Equip_Item(int iSlot);
    void        UnEquip_Item(EQUIP_SLOT eSlot);
    void        Use_QuickSlot(int iSlot, CItemData_UseItem* pItem);
    CInventory* Get_Inventory() const { return m_pInventory; }
    CEquipment* Get_Equipment() const { return m_pEquipment; }

    int         Get_TotalAtk() const { return m_iAttack + m_pEquipment->Get_TotalAtk(); }
    int         Get_TotalDef() const { return m_iDef + m_pEquipment->Get_TotalDef(); }

private:

    CInventory* m_pInventory = nullptr;
    CEquipment* m_pEquipment = nullptr;

private:
    std::vector<std::pair<float, float>> m_waypoints;
    int32_t m_nCurWaypoint = 0;
#ifdef GAME_DEBUG
private:
    void Debug_Render(ID2D1RenderTarget* pRT);
    void Debug_DrawClickedTile(ID2D1RenderTarget* pRT);
    void Debug_DrawClickPoint(ID2D1RenderTarget* pRT);
    void Debug_DrawCollider(ID2D1RenderTarget* pRT);
    void Debug_DrawText(ID2D1RenderTarget* pRT);
    void Debug_DrawPath(ID2D1RenderTarget* pRT);

    int   m_iDebugTileX = 0;
    int   m_iDebugTileZ = 0;
    float m_fDebugLocalX = 0.f;
    float m_fDebugLocalZ = 0.f;
#endif

};