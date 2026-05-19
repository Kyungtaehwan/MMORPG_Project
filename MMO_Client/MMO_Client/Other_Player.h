#pragma once
#include "GameObject.h"  // CPlayer가 상속받는 베이스 클래스

enum PLAYER_STATE;
enum DIRECTION;

class COther_Player : public CGameObject
{
public:
    COther_Player();
    virtual ~COther_Player();

    virtual void Initialize()        override;
    void Initialize(int32_t nPlayerID, const char* pszName,
        float fX, float fZ, uint8_t nDir = 0);
    virtual int  Update(float dt)    override;
    virtual void Late_Update(float dt) override;
    virtual void Render(ID2D1RenderTarget* pRT) override;
    virtual void Release(void)       override;



    void OnMoveDestPacket(float fCurX, float fCurZ, float fDestX, float fDestZ,
        float fSpeed, uint32_t moveTime);
    void OnMovePosPacket(float fCurX, float fCurZ, uint32_t moveTime);

    void SetDeadState();
    void OnAttackPacket();
    void OnHitPacket(int32_t iHp);
    void OnDeadPacket();

    int32_t GetPlayerID() const { return m_nPlayerID; }


private:
    void    Check_AnimEnd();  // 애니메이션 완료 체크

    void Move_To_Dest(float dt);
    void Decide_Direction(float fNX, float fNZ);
    void Motion_Change(PLAYER_STATE eState);
    void Direction_Change(DIRECTION eDir);
    void Render_Sprite(ID2D1RenderTarget* pRT, ID2D1Bitmap* pBitmap);
    void RenderIDLE(ID2D1RenderTarget* pRT);
    void RenderWALK(ID2D1RenderTarget* pRT);
    void RenderATTACK(ID2D1RenderTarget* pRT);
    void RenderHIT(ID2D1RenderTarget* pRT);
    void RenderDEAD(ID2D1RenderTarget* pRT);

private:
    int32_t      m_nPlayerID = -1;
    char         m_szName[20] = {};

    float        m_fDestX = 0.f;
    float        m_fDestZ = 0.f;
    float        m_fSpeed = 1.f;
    bool         m_bMoving = false;

    PLAYER_STATE m_eCurState;
    DIRECTION    m_eDir;
};