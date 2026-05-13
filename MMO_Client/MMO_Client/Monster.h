#pragma once
#include "GameObject.h"

enum MONSTER_STATE : int
{
    MON_IDLE,
    MON_WALK,
    MON_ATTACK_0,
    MON_ATTACK_1,
    MON_HIT,
    MON_DEAD,
    MON_END
};

class CMonster : public CGameObject
{
public:
    CMonster() = default;
    virtual ~CMonster() = default;

public:
    virtual void Initialize()                        override;
    virtual int  Update(float dt)                   override;
    virtual void Late_Update(float dt)               override;
    virtual void Render(ID2D1RenderTarget* pRT)     override;
    virtual void Release()                          override;

public:
    // ---- 순수 가상 - 각 몬스터가 반드시 구현 ----
    virtual void Motion_Change(MONSTER_STATE eState) PURE;

protected:
    // ---- 패킷 수신 후 하위 클래스 처리 - 반드시 구현 ----
    virtual void On_MovePacket(uint8_t nDir)                            PURE;
    virtual void On_StatePacket(MONSTER_STATE eState, int32_t nTargetID) PURE;

public:
    // ================================================================
    //  OnMovePacket
    //  베이스: 위치 보정 + 이동 세팅 (공통)
    //  이후 On_MovePacket 호출 → 하위 클래스 애니메이션 처리
    // ================================================================
    void OnMovePacket(float fCurX, float fCurZ,
        float fDestX, float fDestZ,
        float fSpeed, uint8_t nDir)
    {
        // 위치 오차 보정
        float fDX = fCurX - m_tIsoInfo.fWorldX;
        float fDZ = fCurZ - m_tIsoInfo.fWorldZ;
        float fDiff = sqrtf(fDX * fDX + fDZ * fDZ);

        if (fDiff > 2.f)
        {
            m_tIsoInfo.fWorldX = fCurX;
            m_tIsoInfo.fWorldZ = fCurZ;
        }
        m_fDestWorldX = fDestX;
        m_fDestWorldZ = fDestZ;
        m_fSpeed = fSpeed;
        m_bMoving = true;

        Direction_Change(static_cast<DIRECTION>(nDir));

        // 하위 클래스 처리
        On_MovePacket(nDir);
    }

    // ================================================================
    //  OnStatePacket
    //  베이스: IDLE/DEAD 공통 처리
    //  이후 On_StatePacket 호출 → 하위 클래스 상태별 처리
    // ================================================================
    void OnStatePacket(MONSTER_STATE eState, int32_t nTargetID)
    {
        // 공통 처리
        switch (eState)
        {
        case MON_IDLE:
        case MON_DEAD:
            m_bMoving = false;
            break;
        default:
            break;
        }

        // 하위 클래스 처리
        On_StatePacket(eState, nTargetID);
    }

    // ---- 네트워크 ID ----
    void    Set_MonsterID(int32_t nID) { m_nMonsterID = nID; }
    int32_t Get_MonsterID()      const { return m_nMonsterID; }

    // ---- 이동 세팅 ----
    void Set_Dest(float fX, float fZ) { m_fDestWorldX = fX; m_fDestWorldZ = fZ; }
    void Set_Moving(bool b) { m_bMoving = b; }
    void Set_Speed(float Speed) { m_fSpeed = Speed; }
    void Set_Dir(DIRECTION eDir) {
        m_eDir = eDir;
        m_tFrame.iFrameStart = 0;
    };
    // ---- 기존 함수 유지 ----
    void Set_ServerPos(float fX, float fZ)
    {
#ifdef NO_SERVER
        m_tIsoInfo.fWorldX = fX;
        m_tIsoInfo.fWorldZ = fZ;
#else
        m_fServerX = fX;
        m_fServerZ = fZ;
#endif
    }

    void Set_MonsterState(MONSTER_STATE eState)
    {
        if (m_eState == eState) return;
        Motion_Change(eState);
    }

    void          Set_MonsterName(const TCHAR* pName) { lstrcpy(m_szName, pName); }
    MONSTER_STATE Get_MonsterState()             const { return m_eState; }

protected:
    void Render_Sprite(ID2D1RenderTarget* pRT, ID2D1Bitmap* pBitmap);
    void Render_HpBar(ID2D1RenderTarget* pRT);
    void Render_NameTag(ID2D1RenderTarget* pRT);
    void Update_Cursor();
    void Direction_Change(DIRECTION eDir);

#ifdef GAME_DEBUG
    void Debug_Render(ID2D1RenderTarget* pRT);
    void Debug_DrawCollider(ID2D1RenderTarget* pRT);
    void Debug_DrawMouseCollider(ID2D1RenderTarget* pRT);
    void Debug_DrawText(ID2D1RenderTarget* pRT);
#endif

protected:
    MONSTER_STATE  m_eState = MON_IDLE;
    TCHAR          m_szName[64] = {};
    CURSOR_MODE    m_eHoverCursor = CURSOR_ATTACK;
    // 이동 목적지
    float   m_fDestWorldX = 0.f;
    float   m_fDestWorldZ = 0.f;
    bool    m_bMoving = false;

    // 네트워크 ID
    int32_t m_nMonsterID = -1;
};