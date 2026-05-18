#pragma once

enum PLAYER_STATE {
    PLAYER_IDLE, PLAYER_WALK, PLAYER_HIT, PLAYER_ATTACK, PLAYER_DEAD, PLAYER_END
};

class CPlayer
{
public:
    CPlayer();
    ~CPlayer() = default;

    // ---- 기본 정보 ----
    int32_t  m_nPlayerID = -1;
    int32_t  m_nSessionID = -1;
    char     m_szName[20] = {};

    // ---- 현재 위치 ----
    // CS_MOVE_로 업데이트되는 마지막 확인 위치
    float    m_fCurX = 0.f;
    float    m_fCurZ = 0.f;

    // ---- 이동 정보 ----
    float    m_fDestX = 0.f;       // 목적지
    float    m_fDestZ = 0.f;
    float    m_fMoveStartX = 0.f;  // 이동 시작 위치
    float    m_fMoveStartZ = 0.f;
    uint32_t m_nMoveStartTime = 0; // 이동 시작 시간 (ms)
    bool     m_bMoving = false;
    PLAYER_STATE m_eState = PLAYER_IDLE;
    // ---- 속도 ----
    float    m_fSpeed = 1.f;
    // ---- 전투 -----
    uint32_t      m_iHp;
    uint32_t      m_iMaxHp;


    // ---- 타일 좌표 (시야 계산용) ----
    int32_t  m_nTileX = 0;
    int32_t  m_nTileZ = 0;

    // ---- 존 정보 ----
    int32_t  m_nZoneID = 0;

    // ---- 시야 리스트 ----
    std::unordered_set<int32_t> m_viewList;
    std::mutex                  m_viewLock;

    std::unordered_set<int32_t> m_monsterViewList;
    std::mutex                  m_monsterViewLock;
    // ---- 마지막 패킷 시간 ----
    uint32_t m_nLastMoveTime = 0;

    // ================================================================
    //  GetCurrentPos 
    // 현재 시간 기준 실제 위치 계산
    //
    //  CS_MOVE_DEST를 받은 순간부터
    //  "시작위치 + 방향 * 속도 * 경과시간" 으로 역산
    //
    //  몬스터 AI, 전투 사정거리 계산에서 이걸 쓰면
    //  서버가 항상 정확한 float 위치를 알 수 있음
    // ================================================================
    void GetCurrentPos(uint32_t nCurrentTime, float& fOutX, float& fOutZ) const
    {
        if (!m_bMoving)
        {
            fOutX = m_fCurX;
            fOutZ = m_fCurZ;
            return;
        }

        float fDX = m_fDestX - m_fMoveStartX;
        float fDZ = m_fDestZ - m_fMoveStartZ;
        float fDist = sqrtf(fDX * fDX + fDZ * fDZ);

        if (fDist < 0.001f)
        {
            fOutX = m_fDestX;
            fOutZ = m_fDestZ;
            return;
        }

        // 경과 시간 (ms → 초)
        float fElapsed = (nCurrentTime - m_nMoveStartTime) / 1000.f;
        float fMoved = m_fSpeed * fElapsed;

        if (fMoved >= fDist)
        {
            // 이미 목적지 도착
            fOutX = m_fDestX;
            fOutZ = m_fDestZ;
            return;
        }

        // 시작위치 + 방향 * 이동거리
        fOutX = m_fMoveStartX + (fDX / fDist) * fMoved;
        fOutZ = m_fMoveStartZ + (fDZ / fDist) * fMoved;
    }

    // ================================================================
    //  GetDistanceTo  다른 위치까지 float 거리
    //  몬스터 사정거리 계산에 사용
    // ================================================================
    float GetDistanceTo(float fTargetX, float fTargetZ,
        uint32_t nCurrentTime) const
    {
        float fMyX, fMyZ;
        GetCurrentPos(nCurrentTime, fMyX, fMyZ);

        float fDX = fTargetX - fMyX;
        float fDZ = fTargetZ - fMyZ;
        return sqrtf(fDX * fDX + fDZ * fDZ);
    }

    // ---- 타일 좌표 갱신 ----
    bool UpdateTilePos()
    {
        int32_t nNewTileX = static_cast<int32_t>(floorf(m_fCurX));
        int32_t nNewTileZ = static_cast<int32_t>(floorf(m_fCurZ));

        if (m_nTileX == nNewTileX && m_nTileZ == nNewTileZ)
            return false;

        m_nTileX = nNewTileX;
        m_nTileZ = nNewTileZ;
        return true;
    }

    // ---- 도착 여부 확인 ----
    // 타이머에서 이동 완료 체크에 사용
    bool IsArrived(uint32_t nCurrentTime) const
    {
        if (!m_bMoving) return true;

        float fDX = m_fDestX - m_fMoveStartX;
        float fDZ = m_fDestZ - m_fMoveStartZ;
        float fDist = sqrtf(fDX * fDX + fDZ * fDZ);

        float fElapsed = (nCurrentTime - m_nMoveStartTime) / 1000.f;
        return (m_fSpeed * fElapsed) >= fDist;
    }
};

using PlayerRef = std::shared_ptr<CPlayer>;