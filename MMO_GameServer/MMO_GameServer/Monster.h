#pragma once
#include <atomic>
#include <memory>
#include <cstdint>
#include <cmath>

enum MONSTER_STATE : uint8_t
{
    MON_IDLE = 0,
    MON_WALK = 1,
    MON_ATTACK_0 = 2,
    MON_ATTACK_1 = 3,
    MON_HIT = 4,
    MON_DEAD = 5,
};

enum MONSTER_TYPE : uint8_t
{
    MONSTER_ORC = 0,
};

enum MONSTER_DIR : uint8_t
{
    MON_DIR_B = 0,
    MON_DIR_LB = 1,
    MON_DIR_L = 2,
    MON_DIR_LT = 3,
    MON_DIR_T = 4,
    MON_DIR_RT = 5,
    MON_DIR_R = 6,
    MON_DIR_RB = 7,
};

class CMonster
{
public:
    // ---- 기본 정보 ----
    int32_t       m_nMonsterID = -1;
    int32_t       m_nZoneID = -1;
    MONSTER_TYPE  m_eType = MONSTER_ORC;
    MONSTER_STATE m_eState = MON_IDLE;
    MONSTER_DIR   m_eDir = MON_DIR_B;

    // ---- 위치 ----
    float    m_fCurX = 0.f;
    float    m_fCurZ = 0.f;
    float    m_fDestX = 0.f;
    float    m_fDestZ = 0.f;
    float    m_fSpawnX = 0.f;
    float    m_fSpawnZ = 0.f;
    float    m_fSpeed = 0.7f;

    // ---- 타일 좌표 (논리 좌표계 기준) ----
    int32_t  m_nTileX = 0;
    int32_t  m_nTileZ = 0;

    // ---- 스탯 ----
    int32_t             m_nHp = 100;
    int32_t             m_nMaxHp = 100;
    int32_t             m_nAtk = 10;
    float               m_fAtkRange = 1.0f;   // 공격 범위a
    float               m_fAggroRange = 3.f;    // 어그로 시작 범위
    float               m_fDeAggroRange = 4.f;   // 어그로 해제 범위
    uint32_t            m_nLastAtkTime = 0;    // 마지막 공격 시간
    uint32_t            m_nAtkCoolMs = 2000; // 공격 쿨타임 (ms)
    uint32_t            m_nHitStunEndTime = 100; //스턴 시간

    uint32_t m_nAtkHitDelayMs_0 = 350;
    uint32_t m_nAtkHitDelayMs_1 = 400;

    int32_t  m_nPendingHitTargetID = -1;  // 타격 대기 중인 타겟

    // ---- AI ----
    int32_t            m_nTargetID = -1;
    std::atomic<bool>  m_bActive{ false };

    // ---- 헬퍼 ----
    bool IsDead() const { return m_eState == MON_DEAD; }

    bool UpdateTilePos()
    {
        int32_t nx = static_cast<int32_t>(floorf(m_fCurX));
        int32_t nz = static_cast<int32_t>(floorf(m_fCurZ));
        if (m_nTileX == nx && m_nTileZ == nz) return false;
        m_nTileX = nx;
        m_nTileZ = nz;
        return true;
    }

    // 클라이언트 Decide_Direction과 동일한 로직
    MONSTER_DIR CalcDirection(float fNX, float fNZ) const
    {
        constexpr float TILE_HALF_W = 64.f;
        constexpr float TILE_HALF_H = 32.f;

        float fScreenDX = (fNX - fNZ) * TILE_HALF_W;
        float fScreenDY = (fNX + fNZ) * TILE_HALF_H;
        float fAngle = atan2f(fScreenDY, fScreenDX) * 180.f / 3.14159f;

        if (fAngle >= -22.5f && fAngle < 22.5f)  return MON_DIR_R;
        else if (fAngle >= 22.5f && fAngle < 67.5f)  return MON_DIR_RB;
        else if (fAngle >= 67.5f && fAngle < 112.5f)  return MON_DIR_B;
        else if (fAngle >= 112.5f && fAngle < 157.5f)  return MON_DIR_LB;
        else if (fAngle >= 157.5f || fAngle < -157.5f) return MON_DIR_L;
        else if (fAngle >= -157.5f && fAngle < -112.5f) return MON_DIR_LT;
        else if (fAngle >= -112.5f && fAngle < -67.5f) return MON_DIR_T;
        else                                             return MON_DIR_RT;
    }
};

using MonsterRef = std::shared_ptr<CMonster>;