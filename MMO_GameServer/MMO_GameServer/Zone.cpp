#include "pch.h"
#include "Zone.h"
#include "Player_Manager.h"
#include "Session_Manager.h"
#include "Protocol.h"



CZone::CZone(int32_t nZoneID, const char* pszName,
    int32_t nInnerX, int32_t nInnerZ, const int* pBlockMap)
    : m_nZoneID(nZoneID)
{
    strncpy_s(m_szName, pszName, sizeof(m_szName) - 1);

    const int OUTER = 2;
    m_nTileCountX = OUTER + 1 + nInnerX + 1 + OUTER;
    m_nTileCountZ = OUTER + 1 + nInnerZ + 1 + OUTER;

    const int BORDER_START_X = OUTER;
    const int BORDER_END_X = OUTER + 1 + nInnerX;
    const int BORDER_START_Z = OUTER;
    const int BORDER_END_Z = OUTER + 1 + nInnerZ;

    m_tileMap.resize(m_nTileCountX * m_nTileCountZ, TILE_OUTSIDE);

    for (int z = 0; z < m_nTileCountZ; ++z)
    {
        for (int x = 0; x < m_nTileCountX; ++x)
        {
            bool bBorderX = (x == BORDER_START_X || x == BORDER_END_X);
            bool bBorderZ = (z == BORDER_START_Z || z == BORDER_END_Z);
            bool bInsideX = (x > BORDER_START_X && x < BORDER_END_X);
            bool bInsideZ = (z > BORDER_START_Z && z < BORDER_END_Z);

            TILE_TYPE eType = TILE_OUTSIDE;

            if (bBorderZ && bBorderX) eType = TILE_BLOCK;
            else if (bBorderZ && bInsideX)
                eType = (z == BORDER_START_Z) ? TILE_BORDER_RT : TILE_BORDER_LB;
            else if (bBorderX && bInsideZ)
                eType = (x == BORDER_START_X) ? TILE_BORDER_LT : TILE_BORDER_RB;
            else if (bInsideX && bInsideZ)
            {
                int iInnerX = x - BORDER_START_X - 1;
                int iInnerZ = (nInnerZ - 1) - (z - BORDER_START_Z - 1);
                eType = (pBlockMap[iInnerZ * nInnerX + iInnerX] == 1)
                    ? TILE_BLOCK : TILE_GRASS;
            }

            m_tileMap[z * m_nTileCountX + x] = static_cast<uint8_t>(eType);
        }
    }

    std::cout << "[CZone] '" << m_szName << "' 생성. "
        << m_nTileCountX << "x" << m_nTileCountZ << std::endl;
}

CZone::~CZone() {}

bool CZone::IsMovable(int32_t nTileX, int32_t nTileZ) const
{
    if (nTileX < 0 || nTileX >= m_nTileCountX) return false;
    if (nTileZ < 0 || nTileZ >= m_nTileCountZ) return false;
    return Is_MovableTile(static_cast<TILE_TYPE>(
        m_tileMap[nTileZ * m_nTileCountX + nTileX]));
}

// ================================================================
//  EnterZone
// ================================================================
void CZone::EnterZone(PlayerRef pPlayer, float fSpawnX, float fSpawnZ)
{
    pPlayer->m_nZoneID = m_nZoneID;
    pPlayer->m_fCurX = fSpawnX;
    pPlayer->m_fCurZ = fSpawnZ;
    pPlayer->m_fDestX = fSpawnX;
    pPlayer->m_fDestZ = fSpawnZ;
    pPlayer->m_bMoving = false;
    pPlayer->UpdateTilePos();

    {
        std::lock_guard<std::mutex> lock(m_zoneLock);
        m_playerIDs.insert(pPlayer->m_nPlayerID);
    }

    std::vector<int32_t> vNewView = GetNearPlayers(pPlayer);

    {
        std::lock_guard<std::mutex> lock(pPlayer->m_viewLock);
        pPlayer->m_viewList.clear();
        for (int32_t nID : vNewView)
            pPlayer->m_viewList.insert(nID);
    }

    for (int32_t nNearID : vNewView)
    {
        if (nNearID == pPlayer->m_nPlayerID) continue;

        PlayerRef pNear = CPlayer_Manager::Get_Instance()->Get_Player(nNearID);
        if (!pNear) continue;

        // 나에게 상대방 add
        Send_AddPlayer(pPlayer, pNear);

        // 상대방에게 나 add
        Send_AddPlayer(pNear, pPlayer);

        // 상대방 view_list에 나 추가
        {
            std::lock_guard<std::mutex> lock(pNear->m_viewLock);
            pNear->m_viewList.insert(pPlayer->m_nPlayerID);
        }
    }
    UpdateMonsterView(pPlayer);
}

// ================================================================
//  LeaveZone
// ================================================================
void CZone::LeaveZone(PlayerRef pPlayer)
{
    {
        std::lock_guard<std::mutex> lock(m_zoneLock);
        m_playerIDs.erase(pPlayer->m_nPlayerID);
    }

    std::unordered_set<int32_t> viewCopy;
    {
        std::lock_guard<std::mutex> lock(pPlayer->m_viewLock);
        viewCopy = pPlayer->m_viewList;
        pPlayer->m_viewList.clear();
    }

    for (int32_t nNearID : viewCopy)
    {
        PlayerRef pNear = CPlayer_Manager::Get_Instance()->Get_Player(nNearID);
        if (!pNear) continue;

        Send_RemovePlayer(pNear, pPlayer->m_nPlayerID);

        {
            std::lock_guard<std::mutex> lock(pNear->m_viewLock);
            pNear->m_viewList.erase(pPlayer->m_nPlayerID);
        }
    }
}

// ================================================================
//  OnMoveDest ? 마우스 클릭 시 1번 호출
//
//  1) 목적지 타일이 이동 가능한지 검증
//  2) 플레이어 목적지 저장
//  3) 현재 view_list에 브로드캐스트
//     (다른 클라이언트들이 이 플레이어의 이동을 보간 시작)
// ================================================================
void CZone::OnMoveDest(PlayerRef pPlayer,
    float fDestX, float fDestZ, uint32_t nMoveTime)
{
    if (pPlayer->m_bDead) return;

    int32_t nDestTileX = static_cast<int32_t>(floorf(fDestX));
    int32_t nDestTileZ = static_cast<int32_t>(floorf(fDestZ));

    // 목적지 블록 검증
    if (!IsMovable(nDestTileX, nDestTileZ))
    {
        // 이동 불가 타일 ? 현재 위치로 되돌림
        Send_MovePlayer(pPlayer, pPlayer, nMoveTime);
        return;
    }

    // 목적지 + 이동 시작 정보 저장
    // m_fMoveStartX/Z와 m_nMoveStartTime을 저장해두면
    // 서버가 언제든지 GetCurrentPos()로 정확한 위치 역산 가능
    float fCalcX, fCalcZ;
    pPlayer->GetCurrentPos(nMoveTime, fCalcX, fCalcZ);

    pPlayer->m_fMoveStartX = fCalcX;    // 클릭 시점의 실제 위치
    pPlayer->m_fMoveStartZ = fCalcZ;
    pPlayer->m_nMoveStartTime = nMoveTime;
    pPlayer->m_fDestX = fDestX;
    pPlayer->m_fDestZ = fDestZ;
    pPlayer->m_bMoving = true;
    pPlayer->m_nLastMoveTime = nMoveTime;

    {
        float fMoveDX = fDestX - fCalcX;
        float fMoveDZ = fDestZ - fCalcZ;
        float fMoveDist = sqrtf(fMoveDX * fMoveDX + fMoveDZ * fMoveDZ);
        if (fMoveDist > 0.001f)
        {
            float fNX = fMoveDX / fMoveDist;
            float fNZ = fMoveDZ / fMoveDist;
            constexpr float TILE_HALF_W = 64.f;
            constexpr float TILE_HALF_H = 32.f;
            float fScreenDX = (fNX - fNZ) * TILE_HALF_W;
            float fScreenDY = (fNX + fNZ) * TILE_HALF_H;
            float fAngle = atan2f(fScreenDY, fScreenDX) * 180.f / 3.14159f;
            uint8_t nDir = 0;
            if      (fAngle >= -22.5f  && fAngle <   22.5f)  nDir = 6;
            else if (fAngle >=  22.5f  && fAngle <   67.5f)  nDir = 7;
            else if (fAngle >=  67.5f  && fAngle <  112.5f)  nDir = 0;
            else if (fAngle >= 112.5f  && fAngle <  157.5f)  nDir = 1;
            else if (fAngle >= 157.5f  || fAngle  < -157.5f) nDir = 2;
            else if (fAngle >= -157.5f && fAngle < -112.5f)  nDir = 3;
            else if (fAngle >= -112.5f && fAngle <  -67.5f)  nDir = 4;
            else                                              nDir = 5;
            pPlayer->m_eDir = nDir;
        }
    }

    // 현재 view_list에 브로드캐스트
    // 시야 재계산은 하지 않음 ? 아직 위치 안 바뀌었으니까
    {
        std::lock_guard<std::mutex> lock(pPlayer->m_viewLock);
        for (int32_t nNearID : pPlayer->m_viewList)
        {
            PlayerRef pNear = CPlayer_Manager::Get_Instance()->Get_Player(nNearID);
            if (pNear) Send_MovePlayer(pNear, pPlayer, nMoveTime);
        }
    }
}

// ================================================================
//  OnMovePos ? 클라이언트 타일 변경 시마다 호출
//
//  1) 현재 위치 업데이트
//  2) 타일 좌표 갱신
//  3) 타일이 바뀌었으면 시야 재계산
//     → 새로 보이는 플레이어: SC_ADD_PLAYER 양방향
//     → 시야 밖 플레이어:    SC_REMOVE_PLAYER 양방향
//     → 계속 보이는 플레이어: SC_MOVE_PLAYER
//  4) 타일이 안 바뀌었으면 move 브로드캐스트만
// ================================================================
void CZone::OnMovePos(PlayerRef pPlayer,
    float fCurX, float fCurZ, uint32_t nMoveTime)
{
    // 속도 검증

    if (pPlayer->m_bDead) return;

    if (pPlayer->m_bMoving)
    {
        float fServerX, fServerZ;
        pPlayer->GetCurrentPos(nMoveTime, fServerX, fServerZ);

        float fDiffX = fCurX - fServerX;
        float fDiffZ = fCurZ - fServerZ;
        float fDiff = sqrtf(fDiffX * fDiffX + fDiffZ * fDiffZ);

        constexpr float MAX_TOLERANCE = 2.f;
        if (fDiff > MAX_TOLERANCE)
        {
            fCurX = fServerX;
            fCurZ = fServerZ;
        }
    }

    // 현재 위치 업데이트
    pPlayer->m_fCurX = fCurX;
    pPlayer->m_fCurZ = fCurZ;
    pPlayer->m_nLastMoveTime = nMoveTime;

    // 목적지 도착 여부 체크
    if (pPlayer->IsArrived(nMoveTime))
    {
        pPlayer->m_fCurX = pPlayer->m_fDestX;
        pPlayer->m_fCurZ = pPlayer->m_fDestZ;
        pPlayer->m_bMoving = false;
    }

    bool bTileChanged = pPlayer->UpdateTilePos();

    if (!bTileChanged)
    {
        // 타일 안에서만 움직임 ? 시야 변화 없음
        // move 패킷만 현재 view_list에 브로드캐스트
        std::lock_guard<std::mutex> lock(pPlayer->m_viewLock);
        for (int32_t nNearID : pPlayer->m_viewList)
        {
            PlayerRef pNear = CPlayer_Manager::Get_Instance()->Get_Player(nNearID);
            if (pNear) Send_MovePlayer(pNear, pPlayer, nMoveTime);
        }
        return;
    }

    // ---- 타일 바뀜 → 시야 재계산 ----
    std::vector<int32_t> vOldView;
    {
        std::lock_guard<std::mutex> lock(pPlayer->m_viewLock);
        vOldView.assign(pPlayer->m_viewList.begin(), pPlayer->m_viewList.end());
    }

    std::vector<int32_t> vNewView = GetNearPlayers(pPlayer);

    {
        std::lock_guard<std::mutex> lock(pPlayer->m_viewLock);
        pPlayer->m_viewList.clear();
        for (int32_t nID : vNewView)
            pPlayer->m_viewList.insert(nID);
    }

    UpdateViewAndBroadcast(pPlayer, vOldView, vNewView, nMoveTime);
    UpdateMonsterView(pPlayer);
}

// ================================================================
//  GetNearPlayers ? 존 전체 순회 + 타일 거리로 시야 판정
// ================================================================
std::vector<int32_t> CZone::GetNearPlayers(PlayerRef pPlayer)
{
    std::vector<int32_t> vResult;

    std::lock_guard<std::mutex> lock(m_zoneLock);
    for (int32_t nID : m_playerIDs)
    {
        if (nID == pPlayer->m_nPlayerID) continue;

        PlayerRef pOther = CPlayer_Manager::Get_Instance()->Get_Player(nID);
        if (!pOther) continue;

        if (CanSee(pPlayer, pOther))
            vResult.push_back(nID);
    }
    return vResult;
}

bool CZone::CanSee(PlayerRef pA, PlayerRef pB)
{
    int32_t nDX = abs(pA->m_nTileX - pB->m_nTileX);
    int32_t nDZ = abs(pA->m_nTileZ - pB->m_nTileZ);
    return nDX <= VIEW_RANGE && nDZ <= VIEW_RANGE;
}

// ================================================================
//  UpdateViewAndBroadcast
//
//  old/new 비교:
//  new에 있고 old에 없음 → 새로 보임 → SC_ADD_PLAYER 양방향
//  old에 있고 new에 없음 → 시야 밖   → SC_REMOVE_PLAYER 양방향
//  둘 다 있음            → 계속 보임 → SC_MOVE_PLAYER
// ================================================================
void CZone::UpdateViewAndBroadcast(PlayerRef pPlayer,
    const std::vector<int32_t>& vOldView,
    const std::vector<int32_t>& vNewView,
    uint32_t nMoveTime)
{
    std::unordered_set<int32_t> setOld(vOldView.begin(), vOldView.end());
    std::unordered_set<int32_t> setNew(vNewView.begin(), vNewView.end());

    // 나 자신에게 이동 확인 패킷
    Send_MovePlayer(pPlayer, pPlayer, nMoveTime);

    for (int32_t nNearID : setNew)
    {
        PlayerRef pNear = CPlayer_Manager::Get_Instance()->Get_Player(nNearID);
        if (!pNear) continue;

        if (setOld.count(nNearID) == 0)
        {
            // ---- 새로 시야에 들어옴 ----
            Send_AddPlayer(pPlayer, pNear);   // 나에게 상대방 add
            Send_AddPlayer(pNear, pPlayer);   // 상대방에게 나 add

            {
                std::lock_guard<std::mutex> lock(pNear->m_viewLock);
                pNear->m_viewList.insert(pPlayer->m_nPlayerID);
            }
        }
        else
        {
            // ---- 계속 시야 안 ----
            Send_MovePlayer(pNear, pPlayer, nMoveTime);
        }
    }

    for (int32_t nNearID : setOld)
    {
        if (setNew.count(nNearID) != 0) continue;

        PlayerRef pNear = CPlayer_Manager::Get_Instance()->Get_Player(nNearID);
        if (!pNear) continue;

        // ---- 시야 밖으로 나감 ----
        Send_RemovePlayer(pPlayer, nNearID);           // 나에게 상대방 remove
        Send_RemovePlayer(pNear, pPlayer->m_nPlayerID); // 상대방에게 나 remove

        {
            std::lock_guard<std::mutex> lock(pNear->m_viewLock);
            pNear->m_viewList.erase(pPlayer->m_nPlayerID);
        }
    }
}

// ================================================================
//  패킷 전송 헬퍼
// ================================================================
void CZone::Send_AddPlayer(PlayerRef pTo, PlayerRef pTarget)
{
    auto pSession = CSession_Manager::Get_Instance()->Get_Session(pTo->m_nSessionID);
    if (!pSession) return;

    SC_ADD_PLAYER_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_ADD_PLAYER;
    pkt.playerID = pTarget->m_nPlayerID;
    pkt.fCurX = pTarget->m_fCurX;
    pkt.fCurZ = pTarget->m_fCurZ;
    pkt.fDestX = pTarget->m_fDestX;
    pkt.fDestZ = pTarget->m_fDestZ;
    pkt.fSpeed = pTarget->m_fSpeed;
    pkt.state = static_cast<uint8_t>(pTarget->m_eState);
    pkt.dir = pTarget->m_eDir;
    strncpy_s(pkt.name, pTarget->m_szName, sizeof(pkt.name) - 1);
    pSession->Send(&pkt, sizeof(pkt));
}

void CZone::Send_RemovePlayer(PlayerRef pTo, int32_t nTargetID)
{
    auto pSession = CSession_Manager::Get_Instance()->Get_Session(pTo->m_nSessionID);
    if (!pSession) return;

    SC_REMOVE_PLAYER_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_REMOVE_PLAYER;
    pkt.playerID = nTargetID;
    pSession->Send(&pkt, sizeof(pkt));
}

void CZone::Send_MovePlayer(PlayerRef pTo, PlayerRef pMoved, uint32_t nMoveTime)
{
    auto pSession = CSession_Manager::Get_Instance()->Get_Session(pTo->m_nSessionID);
    if (!pSession) return;

    SC_MOVE_PLAYER_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_MOVE_PLAYER;
    pkt.playerID = pMoved->m_nPlayerID;
    pkt.fCurX = pMoved->m_fCurX;
    pkt.fCurZ = pMoved->m_fCurZ;
    pkt.fDestX = pMoved->m_fDestX;
    pkt.fDestZ = pMoved->m_fDestZ;
    pkt.fSpeed = pMoved->m_fSpeed;
    pkt.moveTime = nMoveTime;
    pSession->Send(&pkt, sizeof(pkt));
}

// ================================================================
//  SpawnMonster
// ================================================================
void CZone::SpawnMonster(int32_t nID, MONSTER_TYPE eType, float fX, float fZ)
{
    MonsterRef pMonster = CMonster_Manager::Get_Instance()
        ->Create(nID, eType, fX, fZ, m_nZoneID);
    if (!pMonster) return;

    {
        std::lock_guard<std::mutex> lock(m_monsterLock);
        m_monsterIDs.insert(nID);
    }

    // 스폰 시 시야 범위 내 플레이어에게 알림
    Broadcast_AddMonster(pMonster);

    std::cout << "[Zone] 몬스터 스폰. ID=" << nID
        << " pos=(" << fX << ", " << fZ << ")" << std::endl;
}

// ================================================================
//  UpdateMonsterView
//  플레이어 타일 변경 시 호출
//  시야 진입 → SC_ADD_MONSTER + CAS 활성화
//  시야 이탈 → SC_REMOVE_MONSTER
// ================================================================
void CZone::UpdateMonsterView(PlayerRef pPlayer)
{
    // 현재 시야 내 몬스터 계산
    std::vector<int32_t> vNewView;
    {
        std::lock_guard<std::mutex> lock(m_monsterLock);
        for (int32_t nID : m_monsterIDs)
        {
            MonsterRef pMonster = CMonster_Manager::Get_Instance()
                ->Get_Monster(nID);
            if (!pMonster) continue;
            if (pMonster->IsDead()) continue;

            // 논리 좌표계 기준 타일 거리
            int32_t nDX = abs(pPlayer->m_nTileX - pMonster->m_nTileX);
            int32_t nDZ = abs(pPlayer->m_nTileZ - pMonster->m_nTileZ);
            if (nDX <= VIEW_RANGE && nDZ <= VIEW_RANGE)
                vNewView.push_back(nID);
        }
    }

    // 이전 시야
    std::vector<int32_t> vOldView;
    {
        std::lock_guard<std::mutex> lock(pPlayer->m_monsterViewLock);
        vOldView.assign(
            pPlayer->m_monsterViewList.begin(),
            pPlayer->m_monsterViewList.end());
    }

    // 시야 갱신
    {
        std::lock_guard<std::mutex> lock(pPlayer->m_monsterViewLock);
        pPlayer->m_monsterViewList.clear();
        for (int32_t nID : vNewView)
            pPlayer->m_monsterViewList.insert(nID);
    }

    std::unordered_set<int32_t> setOld(vOldView.begin(), vOldView.end());
    std::unordered_set<int32_t> setNew(vNewView.begin(), vNewView.end());

    // 새로 시야 진입
    for (int32_t nID : setNew)
    {
        if (setOld.count(nID) != 0) continue;

        MonsterRef pMonster = CMonster_Manager::Get_Instance()
            ->Get_Monster(nID);
        if (!pMonster) continue;

        Send_AddMonster(pPlayer, pMonster);

        // CAS 활성화 - 복귀 중(m_bActive=true)이면 실패 → 중복 방지
        bool expected = false;
        if (pMonster->m_bActive.compare_exchange_strong(expected, true))
            AddTimer(nID, EEventType::MonsterAI, 500);
    }

    // 시야 이탈
    for (int32_t nID : setOld)
    {
        if (setNew.count(nID) != 0) continue;
        Send_RemoveMonster(pPlayer, nID);
    }
}

// ================================================================
//  OnMonsterAI Worker Thread에서 500ms마다 호출
// ================================================================
void CZone::OnMonsterAI(int32_t nMonsterID)
{
    MonsterRef pMonster = CMonster_Manager::Get_Instance()
        ->Get_Monster(nMonsterID);
    if (!pMonster || pMonster->IsDead()) return;

    uint32_t nNow = static_cast<uint32_t>(GetTickCount64());

    if (pMonster->m_eState == MON_HIT)
    {
        if (nNow < pMonster->m_nHitStunEndTime)
        {
            // 아직 경직 중 → AI 스킵 + 다음 틱 예약
            AddTimer(nMonsterID, EEventType::MonsterAI, 500);
            return;
        }
        else
        {
            // 경직 종료 → IDLE 복귀
            pMonster->m_eState = MON_IDLE;
            pMonster->m_nHitStunEndTime = 0;
            Broadcast_MonsterState(pMonster);
        }
    }

    if (!PlayerExistNear(pMonster))
    {
        // 타겟이 있었던 경우에만 복귀
        // 타겟 없이 활성화된 경우(첫 활성화 등)는 그냥 대기
        if (pMonster->m_nTargetID != -1)
        {
            pMonster->m_nTargetID = -1;

            float fDX = pMonster->m_fSpawnX - pMonster->m_fCurX;
            float fDZ = pMonster->m_fSpawnZ - pMonster->m_fCurZ;
            float fDist = sqrtf(fDX * fDX + fDZ * fDZ);

            if (fDist >= 0.1f)
            {
                Monster_Patrol(pMonster);
                AddTimer(nMonsterID, EEventType::MonsterAI, 500);
            }
            else
            {
                pMonster->m_fCurX = pMonster->m_fSpawnX;
                pMonster->m_fCurZ = pMonster->m_fSpawnZ;
                if (pMonster->m_eState != MON_IDLE)
                {
                    pMonster->m_eState = MON_IDLE;
                    Broadcast_MonsterState(pMonster);
                }
                pMonster->m_bActive = false;
            }
        }
        else
        {
            // 타겟 없이 활성화된 상태 → IDLE 대기 루프 유지
            if (pMonster->m_eState != MON_IDLE)
            {
                pMonster->m_eState = MON_IDLE;
                Broadcast_MonsterState(pMonster);
            }
            AddTimer(nMonsterID, EEventType::MonsterAI, 500);
        }
        return;
    }

    PlayerRef pTarget = FindNearestPlayer(pMonster);
    if (!pTarget)
    {
        if (pMonster->m_eState != MON_IDLE)
        {
            pMonster->m_eState = MON_IDLE;
            Broadcast_MonsterState(pMonster);
        }
        AddTimer(nMonsterID, EEventType::MonsterAI, 500);
        return;
    }

    pMonster->m_nTargetID = pTarget->m_nPlayerID;

    float fPlayerX, fPlayerZ;
    pTarget->GetCurrentPos(nNow, fPlayerX, fPlayerZ);

    float fDX = fPlayerX - pMonster->m_fCurX;
    float fDZ = fPlayerZ - pMonster->m_fCurZ;
    float fDist = sqrtf(fDX * fDX + fDZ * fDZ);

    if (fDist <= pMonster->m_fAtkRange)
    {
        if (pMonster->m_eState == MON_WALK)
        {
            pMonster->m_fDestX = pMonster->m_fCurX;
            pMonster->m_fDestZ = pMonster->m_fCurZ;
            pMonster->m_eState = MON_IDLE;
            Broadcast_MoveMonster(pMonster);
        }
        Monster_Attack(pMonster);
    }
    else
        Monster_Chase(pMonster, fPlayerX, fPlayerZ);

    if (!pMonster->IsDead())
        AddTimer(nMonsterID, EEventType::MonsterAI, 500);
}

// ================================================================
//  MonsterChase
//  타일 변경 or 방향 변경 or 상태 변경 시에만 패킷 전송
// ================================================================
void CZone::Monster_Chase(MonsterRef pMonster, float fPlayerX, float fPlayerZ)
{
    MONSTER_STATE ePrevState = pMonster->m_eState;
    MONSTER_DIR   ePrevDir = pMonster->m_eDir;

    float fDX = fPlayerX - pMonster->m_fCurX;
    float fDZ = fPlayerZ - pMonster->m_fCurZ;
    float fDist = sqrtf(fDX * fDX + fDZ * fDZ);
    if (fDist < 0.001f) return;

    // 이미 공격 범위 내면 방향만 바라보고 종료
    if (fDist <= pMonster->m_fAtkRange)
    {
        MONSTER_DIR eNewDir = pMonster->CalcDirection(fDX / fDist, fDZ / fDist);
        if (pMonster->m_eDir != eNewDir || pMonster->m_eState != MON_IDLE)
        {
            pMonster->m_eDir = eNewDir;
            pMonster->m_eState = MON_IDLE;
            Broadcast_MoveMonster(pMonster);
        }
        return;
    }

    int32_t nSX = static_cast<int32_t>(floorf(pMonster->m_fCurX));
    int32_t nSZ = static_cast<int32_t>(floorf(pMonster->m_fCurZ));
    int32_t nEX = static_cast<int32_t>(floorf(fPlayerX));
    int32_t nEZ = static_cast<int32_t>(floorf(fPlayerZ));

    float fNX = fDX / fDist;
    float fNZ = fDZ / fDist;
    MONSTER_DIR eNewDir = pMonster->CalcDirection(fNX, fNZ);

    if (nSX == nEX && nSZ == nEZ)
    {
        pMonster->m_eState = MON_WALK;
        pMonster->m_eDir = eNewDir;
        pMonster->m_fDestX = fPlayerX;
        pMonster->m_fDestZ = fPlayerZ;

        // 실제 위치 이동
        float fMoveStep = pMonster->m_fSpeed * 0.5f;
        if (fDist <= fMoveStep)
        {
            pMonster->m_fCurX = fPlayerX;
            pMonster->m_fCurZ = fPlayerZ;
        }
        else
        {
            pMonster->m_fCurX += fNX * fMoveStep;
            pMonster->m_fCurZ += fNZ * fMoveStep;
        }

        // 이동 후 공격 범위 진입 체크
        float fAfterDX = fPlayerX - pMonster->m_fCurX;
        float fAfterDZ = fPlayerZ - pMonster->m_fCurZ;
        float fAfterDist = sqrtf(fAfterDX * fAfterDX + fAfterDZ * fAfterDZ);

        if (fAfterDist <= pMonster->m_fAtkRange)
        {
            MONSTER_DIR eFaceDir = pMonster->CalcDirection(
                fAfterDX / fAfterDist, fAfterDZ / fAfterDist);
            pMonster->m_eState = MON_IDLE;
            pMonster->m_eDir = eFaceDir;
            pMonster->m_fDestX = pMonster->m_fCurX;
            pMonster->m_fDestZ = pMonster->m_fCurZ;
            pMonster->UpdateTilePos();
            Broadcast_MoveMonster(pMonster);
            return;
        }

        if (ePrevDir != eNewDir || ePrevState != MON_WALK)
            Broadcast_MoveMonster(pMonster);
        return;
    }

    IsMovableFunc fn = [this](int32_t x, int32_t z) -> bool {
        return IsMovable(x, z);
        };

    auto waypoints = CPathFinder::FindPath(
        nSX, nSZ, nEX, nEZ,
        pMonster->m_fCurX, pMonster->m_fCurZ,
        fn, EPathMode::AStar);

    if (waypoints.empty()) return;

    float fNextX = waypoints.size() > 1
        ? waypoints[1].first : waypoints[0].first;
    float fNextZ = waypoints.size() > 1
        ? waypoints[1].second : waypoints[0].second;

    float fMoveDX = fNextX - pMonster->m_fCurX;
    float fMoveDZ = fNextZ - pMonster->m_fCurZ;
    float fMoveDist = sqrtf(fMoveDX * fMoveDX + fMoveDZ * fMoveDZ);
    if (fMoveDist < 0.001f) return;

    eNewDir = pMonster->CalcDirection(fMoveDX / fMoveDist, fMoveDZ / fMoveDist);

    float fMoveStep = pMonster->m_fSpeed * 0.5f;
    if (fMoveDist <= fMoveStep)
    {
        pMonster->m_fCurX = fNextX;
        pMonster->m_fCurZ = fNextZ;
    }
    else
    {
        pMonster->m_fCurX += (fMoveDX / fMoveDist) * fMoveStep;
        pMonster->m_fCurZ += (fMoveDZ / fMoveDist) * fMoveStep;
    }

    // ← 핵심: 이동 후 플레이어까지 거리 재계산
    float fAfterDX = fPlayerX - pMonster->m_fCurX;
    float fAfterDZ = fPlayerZ - pMonster->m_fCurZ;
    float fAfterDist = sqrtf(fAfterDX * fAfterDX + fAfterDZ * fAfterDZ);

    // 이동 후 공격 범위 진입 → 즉시 멈추고 플레이어 방향 바라봄
    if (fAfterDist <= pMonster->m_fAtkRange)
    {
        MONSTER_DIR eFaceDir = pMonster->CalcDirection(
            fAfterDX / fAfterDist, fAfterDZ / fAfterDist);

        pMonster->m_eState = MON_IDLE;
        pMonster->m_eDir = eFaceDir;
        pMonster->m_fDestX = pMonster->m_fCurX;  // 제자리
        pMonster->m_fDestZ = pMonster->m_fCurZ;
        pMonster->UpdateTilePos();
        Broadcast_MoveMonster(pMonster);
        return;
    }

    // 공격 범위 밖 → 계속 이동
    pMonster->m_fDestX = fNextX;
    pMonster->m_fDestZ = fNextZ;
    pMonster->m_eState = MON_WALK;
    pMonster->m_eDir = eNewDir;

    bool bTileChanged = pMonster->UpdateTilePos();
    bool bDirChanged = (ePrevDir != eNewDir);
    bool bStateChanged = (ePrevState != MON_WALK);

    if (bTileChanged || bDirChanged || bStateChanged)
        Broadcast_MoveMonster(pMonster);
}

// ================================================================
//  MonsterAttack
//  공격 중 반복 방지 상태 변경 시에만 패킷 전송
// ================================================================
void CZone::Monster_Attack(MonsterRef pMonster)
{
    uint32_t nNow = static_cast<uint32_t>(GetTickCount64());

    if (nNow - pMonster->m_nLastAtkTime < pMonster->m_nAtkCoolMs)
        return;

    PlayerRef pTarget = CPlayer_Manager::Get_Instance()
        ->Get_Player(pMonster->m_nTargetID);
    if (!pTarget) return;

    // 방향 계산
    float fPlayerX, fPlayerZ;
    pTarget->GetCurrentPos(nNow, fPlayerX, fPlayerZ);
    float fDX = fPlayerX - pMonster->m_fCurX;
    float fDZ = fPlayerZ - pMonster->m_fCurZ;
    float fDist = sqrtf(fDX * fDX + fDZ * fDZ);
    if (fDist > 0.001f)
        pMonster->m_eDir = pMonster->CalcDirection(fDX / fDist, fDZ / fDist);

    if (pMonster->m_eState == MON_ATTACK_0 || pMonster->m_eState == MON_ATTACK_1)
        pMonster->m_eState = MON_IDLE;

    // 공격 모션 결정
    MONSTER_STATE eAtkState = (rand() % 2 == 0) ? MON_ATTACK_0 : MON_ATTACK_1;
    pMonster->m_eState = eAtkState;
    pMonster->m_nLastAtkTime = nNow;

    // ---- 1. 공격 모션 즉시 전송 ----
    Broadcast_MonsterState(pMonster, pMonster->m_nTargetID);

    // ---- 2. 타격은 딜레이 후 전송 ----
    pMonster->m_nPendingHitTargetID = pMonster->m_nTargetID;

    uint32_t nHitDelay = (eAtkState == MON_ATTACK_0)
        ? pMonster->m_nAtkHitDelayMs_0
        : pMonster->m_nAtkHitDelayMs_1;

    AddTimer(pMonster->m_nMonsterID, EEventType::MonsterAttackHit, nHitDelay);
}

// ================================================================
//  Monster_Patrol
//  스폰 위치로 이동만 담당
//  도착 체크는 OnMonsterAI에서 처리
// ================================================================
void CZone::Monster_Patrol(MonsterRef pMonster)
{
    MONSTER_STATE ePrevState = pMonster->m_eState;
    MONSTER_DIR   ePrevDir = pMonster->m_eDir;

    float fDX = pMonster->m_fSpawnX - pMonster->m_fCurX;
    float fDZ = pMonster->m_fSpawnZ - pMonster->m_fCurZ;
    float fDist = sqrtf(fDX * fDX + fDZ * fDZ);

    float fNX = fDX / fDist;
    float fNZ = fDZ / fDist;

    MONSTER_DIR eNewDir = pMonster->CalcDirection(fNX, fNZ);

    float fStep = pMonster->m_fSpeed * 0.5f;
    if (fDist <= fStep)
    {
        pMonster->m_fCurX = pMonster->m_fSpawnX;
        pMonster->m_fCurZ = pMonster->m_fSpawnZ;
    }
    else
    {
        pMonster->m_fCurX += fNX * fStep;
        pMonster->m_fCurZ += fNZ * fStep;
    }

    pMonster->m_fDestX = pMonster->m_fSpawnX;
    pMonster->m_fDestZ = pMonster->m_fSpawnZ;
    pMonster->m_eState = MON_WALK;
    pMonster->m_eDir = eNewDir;

    bool bTileChanged = pMonster->UpdateTilePos();
    bool bDirChanged = (ePrevDir != eNewDir);
    bool bStateChanged = (ePrevState != MON_WALK);

    if (bTileChanged || bDirChanged || bStateChanged)
        Broadcast_MoveMonster(pMonster);
}

// ================================================================
//  FindNearestPlayer - 어그로 범위 내 가장 가까운 플레이어
// ================================================================
PlayerRef CZone::FindNearestPlayer(MonsterRef pMonster)
{
    PlayerRef pNearest = nullptr;
    float     fMinDist = pMonster->m_fAggroRange;

    uint32_t nNow = static_cast<uint32_t>(GetTickCount64());

    std::lock_guard<std::mutex> lock(m_zoneLock);
    for (int32_t nID : m_playerIDs)
    {
        PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(nID);
        if (!pPlayer) continue;
        if (pPlayer->m_bDead) continue;

        float fPlayerX, fPlayerZ;
        pPlayer->GetCurrentPos(nNow, fPlayerX, fPlayerZ);

        float fDX = fPlayerX - pMonster->m_fCurX;
        float fDZ = fPlayerZ - pMonster->m_fCurZ;
        float fDist = sqrtf(fDX * fDX + fDZ * fDZ);

        if (fDist < fMinDist)
        {
            fMinDist = fDist;
            pNearest = pPlayer;
        }
    }
    return pNearest;
}

// ================================================================
//  PlayerExistNear - 해제 범위 내 플레이어 존재 여부
// ================================================================
bool CZone::PlayerExistNear(MonsterRef pMonster)
{
    uint32_t nNow = static_cast<uint32_t>(GetTickCount64());

    std::lock_guard<std::mutex> lock(m_zoneLock);
    for (int32_t nID : m_playerIDs)
    {
        PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(nID);
        if (!pPlayer) continue;
        if (pPlayer->m_bDead) continue;

        float fPlayerX, fPlayerZ;
        pPlayer->GetCurrentPos(nNow, fPlayerX, fPlayerZ);

        float fDX = fPlayerX - pMonster->m_fCurX;
        float fDZ = fPlayerZ - pMonster->m_fCurZ;
        float fDist = sqrtf(fDX * fDX + fDZ * fDZ);

        if (fDist <= pMonster->m_fDeAggroRange)
            return true;
    }
    return false;
}

// ================================================================
//  패킷 전송 헬퍼
// ================================================================
void CZone::Send_AddMonster(PlayerRef pTo, MonsterRef pMonster)
{
    auto pSession = CSession_Manager::Get_Instance()
        ->Get_Session(pTo->m_nSessionID);
    if (!pSession) return;

    SC_ADD_MONSTER_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_ADD_MONSTER;
    pkt.monsterID = pMonster->m_nMonsterID;
    pkt.monsterType = static_cast<uint8_t>(pMonster->m_eType);
    pkt.state = static_cast<uint8_t>(pMonster->m_eState);
    pkt.fCurX = pMonster->m_fCurX;
    pkt.fCurZ = pMonster->m_fCurZ;
    pkt.fDestX = pMonster->m_fDestX;
    pkt.fDestZ = pMonster->m_fDestZ;
    pkt.fSpeed = pMonster->m_fSpeed;
    pSession->Send(&pkt, sizeof(pkt));
}

void CZone::Send_RemoveMonster(PlayerRef pTo, int32_t nMonsterID)
{
    auto pSession = CSession_Manager::Get_Instance()
        ->Get_Session(pTo->m_nSessionID);
    if (!pSession) return;

    SC_REMOVE_MONSTER_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_REMOVE_MONSTER;
    pkt.monsterID = nMonsterID;
    pSession->Send(&pkt, sizeof(pkt));
}

void CZone::Broadcast_AddMonster(MonsterRef pMonster)
{
    std::lock_guard<std::mutex> lock(m_zoneLock);
    for (int32_t nID : m_playerIDs)
    {
        PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(nID);
        if (!pPlayer) continue;

        // 시야 리스트 기반
        std::lock_guard<std::mutex> vlock(pPlayer->m_monsterViewLock);
        if (pPlayer->m_monsterViewList.count(pMonster->m_nMonsterID) == 0)
            continue;

        Send_AddMonster(pPlayer, pMonster);
    }
}

void CZone::Broadcast_MoveMonster(MonsterRef pMonster)
{
    SC_MOVE_MONSTER_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_MOVE_MONSTER;
    pkt.monsterID = pMonster->m_nMonsterID;
    pkt.fCurX = pMonster->m_fCurX;
    pkt.fCurZ = pMonster->m_fCurZ;
    pkt.fDestX = pMonster->m_fDestX;
    pkt.fDestZ = pMonster->m_fDestZ;
    pkt.fSpeed = pMonster->m_fSpeed;
    pkt.dir = static_cast<uint8_t>(pMonster->m_eDir);
    pkt.moveTime = static_cast<uint32_t>(GetTickCount64());

    std::lock_guard<std::mutex> lock(m_zoneLock);
    for (int32_t nID : m_playerIDs)
    {
        PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(nID);
        if (!pPlayer) continue;

        {
            std::lock_guard<std::mutex> vlock(pPlayer->m_monsterViewLock);
            if (pPlayer->m_monsterViewList.count(pMonster->m_nMonsterID) == 0)
                continue;
        }

        auto pSession = CSession_Manager::Get_Instance()
            ->Get_Session(pPlayer->m_nSessionID);
        if (pSession) pSession->Send(&pkt, sizeof(pkt));
    }
}

void CZone::Broadcast_MonsterState(MonsterRef pMonster, int32_t nTargetID)
{
    SC_MONSTER_STATE_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_MONSTER_STATE;
    pkt.monsterID = pMonster->m_nMonsterID;
    pkt.state = static_cast<uint8_t>(pMonster->m_eState);
    pkt.dir = static_cast<uint8_t>(pMonster->m_eDir);
    pkt.targetID = nTargetID;

    std::lock_guard<std::mutex> lock(m_zoneLock);
    for (int32_t nID : m_playerIDs)
    {
        PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(nID);
        if (!pPlayer) continue;

        {
            std::lock_guard<std::mutex> vlock(pPlayer->m_monsterViewLock);
            if (pPlayer->m_monsterViewList.count(pMonster->m_nMonsterID) == 0)
                continue;
        }

        auto pSession = CSession_Manager::Get_Instance()
            ->Get_Session(pPlayer->m_nSessionID);
        if (pSession) pSession->Send(&pkt, sizeof(pkt));
    }
}

void CZone::OnPlayerAttackMonster(PlayerRef pPlayer,
    int32_t nMonsterID, float fPlayerX, float fPlayerZ)
{
    if (pPlayer->m_bDead) return;

    MonsterRef pMonster = CMonster_Manager::Get_Instance()
        ->Get_Monster(nMonsterID);
    if (!pMonster || pMonster->IsDead()) return;

    // 경직 중인 몬스터도 추가 피격 가능 (HIT 중첩)
    // 단 사망한 몬스터는 불가 (위에서 체크)

    // ---- 거리 검증 ----
    // 클라이언트 공격 범위(1.5f)의 2배로 넉넉하게 허용
    uint32_t nNow = static_cast<uint32_t>(GetTickCount64());
    if (nNow - pPlayer->m_nLastAtkTime < pPlayer->m_nAtkCoolMs) return;
    pPlayer->m_nLastAtkTime = nNow;

    constexpr float PLAYER_ATK_TOLERANCE = 3.f;

    float fDX = fPlayerX - pMonster->m_fCurX;
    float fDZ = fPlayerZ - pMonster->m_fCurZ;
    float fDist = sqrtf(fDX * fDX + fDZ * fDZ);

    if (fDist > PLAYER_ATK_TOLERANCE)
    {
        std::cout << "[경고] 공격 거리 초과. PlayerID="
            << pPlayer->m_nPlayerID
            << " 거리=" << fDist << std::endl;
        return;
    }

    // ---- 공격자 모션 브로드캐스트 ----
    // viewList 내 다른 플레이어들에게 공격 모션 전송
    Broadcast_PlayerState(pPlayer, PLAYER_ATTACK);

    // ---- HP 감소 ----
    constexpr int32_t PLAYER_ATK = 20;
    pMonster->m_nHp -= PLAYER_ATK;

    std::cout << "[Zone] 몬스터 피격. ID=" << nMonsterID
        << " HP=" << pMonster->m_nHp
        << "/" << pMonster->m_nMaxHp << std::endl;

    // ---- 사망 처리 ----
    if (pMonster->m_nHp <= 0)
    {
        pMonster->m_nHp = 0;
        pMonster->m_eState = MON_DEAD;
        pMonster->m_bActive = false;
        pMonster->m_nHitStunEndTime = 0;

        Broadcast_MonsterState(pMonster);

        AddTimer(nMonsterID, EEventType::MonsterRespawn, 5000);

        std::cout << "[Zone] 몬스터 사망. ID=" << nMonsterID << std::endl;
        return;
    }

    // ---- 피격 처리 ----
    // 몬스터가 공격자 방향 바라봄
    float fLen = sqrtf(fDX * fDX + fDZ * fDZ);
    if (fLen > 0.001f)
        pMonster->m_eDir = pMonster->CalcDirection(fDX / fLen, fDZ / fLen);

    pMonster->m_eState = MON_HIT;

    // 경직 시간 세팅 (HIT 애니메이션 = 7프레임 × 80ms = 560ms → 600ms)
    uint32_t nNow2 = static_cast<uint32_t>(GetTickCount64());
    pMonster->m_nHitStunEndTime = nNow2 + 600;

    // 피격 패킷 브로드캐스트
    Broadcast_MonsterHit(pMonster);
}

void CZone::OnMonsterRespawn(int32_t nMonsterID)
{
    MonsterRef pMonster = CMonster_Manager::Get_Instance()
        ->Get_Monster(nMonsterID);
    if (!pMonster) return;

    pMonster->m_nHp = pMonster->m_nMaxHp;
    pMonster->m_eState = MON_IDLE;
    pMonster->m_eDir = MON_DIR_B;
    pMonster->m_fCurX = pMonster->m_fSpawnX;
    pMonster->m_fCurZ = pMonster->m_fSpawnZ;
    pMonster->m_fDestX = pMonster->m_fSpawnX;
    pMonster->m_fDestZ = pMonster->m_fSpawnZ;
    pMonster->m_nTargetID = -1;
    pMonster->m_nLastAtkTime = static_cast<uint32_t>(GetTickCount64());
    pMonster->m_nHitStunEndTime = 0;
    pMonster->m_nPendingHitTargetID = -1;
    pMonster->m_bActive = false;
    pMonster->UpdateTilePos();

    std::cout << "[Zone] 몬스터 리스폰. ID=" << nMonsterID << std::endl;

    bool bActivated = false;  // AI 중복 활성화 방지

    std::lock_guard<std::mutex> lock(m_zoneLock);
    for (int32_t nPlayerID : m_playerIDs)
    {
        PlayerRef pPlayer = CPlayer_Manager::Get_Instance()
            ->Get_Player(nPlayerID);
        if (!pPlayer) continue;

        int32_t nDX = abs(pPlayer->m_nTileX - pMonster->m_nTileX);
        int32_t nDZ = abs(pPlayer->m_nTileZ - pMonster->m_nTileZ);
        if (nDX > VIEW_RANGE || nDZ > VIEW_RANGE) continue;

        {
            std::lock_guard<std::mutex> vlock(pPlayer->m_monsterViewLock);
            pPlayer->m_monsterViewList.insert(nMonsterID);
        }

        Send_AddMonster(pPlayer, pMonster);

        // ← 시야 내 플레이어가 있으면 AI 활성화 (한 번만)
        if (!bActivated)
        {
            bool expected = false;
            if (pMonster->m_bActive.compare_exchange_strong(expected, true))
            {
                AddTimer(nMonsterID, EEventType::MonsterAI, 500);
                bActivated = true;
            }
        }
    }
}

void CZone::OnMonsterAttackHit(int32_t nMonsterID)
{
    MonsterRef pMonster = CMonster_Manager::Get_Instance()
        ->Get_Monster(nMonsterID);
    if (!pMonster)
        return;
    if (pMonster->IsDead())
    {
        pMonster->m_nPendingHitTargetID = -1;
        return;
    }

    // 타격 대기 타겟 없으면 스킵 (공격 취소된 경우)
    if (pMonster->m_nPendingHitTargetID == -1) return;

    PlayerRef pTarget = CPlayer_Manager::Get_Instance()
        ->Get_Player(pMonster->m_nPendingHitTargetID);
    pMonster->m_nPendingHitTargetID = -1;

    if (!pTarget) return;
    if (pTarget->m_iHp <= 0) return;

    // 실제 거리 재확인 (도망갔을 수도 있음)
    uint32_t nNow = static_cast<uint32_t>(GetTickCount64());
    float fPlayerX, fPlayerZ;
    pTarget->GetCurrentPos(nNow, fPlayerX, fPlayerZ);
    float fDX = fPlayerX - pMonster->m_fCurX;
    float fDZ = fPlayerZ - pMonster->m_fCurZ;
    float fDist = sqrtf(fDX * fDX + fDZ * fDZ);

    // 타격 범위를 조금 넉넉하게 (공격 범위 × 2)
    if (fDist > pMonster->m_fAtkRange * 2.f) return;

    // Stop player movement on hit
    if (pTarget->m_bMoving)
    {
        float fStopX, fStopZ;
        pTarget->GetCurrentPos(nNow, fStopX, fStopZ);
        pTarget->m_fCurX  = fStopX;
        pTarget->m_fCurZ  = fStopZ;
        pTarget->m_fDestX = fStopX;
        pTarget->m_fDestZ = fStopZ;
        pTarget->m_bMoving = false;
        pTarget->UpdateTilePos();
    }

    // HP 감소 + 피격 패킷
    constexpr int32_t MONSTER_ATK = 10;
    pTarget->m_iHp -= MONSTER_ATK;
    if (pTarget->m_iHp < 0) pTarget->m_iHp = 0;

    Broadcast_PlayerHit(pTarget);

    if (pTarget->m_iHp <= 0)
    {
        pTarget->m_bDead = true;
        pTarget->m_eState = PLAYER_DEAD;
        pTarget->m_bMoving = false;
        pTarget->m_fDestX = pTarget->m_fCurX;
        pTarget->m_fDestZ = pTarget->m_fCurZ;
        Broadcast_PlayerState(pTarget, PLAYER_DEAD);
    }

    std::cout << "[Zone] 몬스터 타격 적용. 몬스터ID=" << nMonsterID
        << " 플레이어ID=" << pTarget->m_nPlayerID
        << " HP=" << pTarget->m_iHp << std::endl;
}

void CZone::Broadcast_PlayerState(PlayerRef pPlayer, PLAYER_STATE eState)
{
    SC_PLAYER_STATE_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_PLAYER_STATE;
    pkt.playerID = pPlayer->m_nPlayerID;
    pkt.state = static_cast<uint8_t>(eState);

    std::lock_guard<std::mutex> lock(pPlayer->m_viewLock);
    for (int32_t nNearID : pPlayer->m_viewList)
    {
        PlayerRef pNear = CPlayer_Manager::Get_Instance()->Get_Player(nNearID);
        if (!pNear) continue;

        auto pSession = CSession_Manager::Get_Instance()
            ->Get_Session(pNear->m_nSessionID);
        if (pSession) pSession->Send(&pkt, sizeof(pkt));
    }
}

void CZone::Broadcast_PlayerHit(PlayerRef pPlayer)
{
    SC_PLAYER_HIT_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_PLAYER_HIT;
    pkt.playerID = pPlayer->m_nPlayerID;
    pkt.nHp = pPlayer->m_iHp;
    pkt.nMaxHp = pPlayer->m_iMaxHp;

    // 피격 당사자에게
    auto pSession = CSession_Manager::Get_Instance()
        ->Get_Session(pPlayer->m_nSessionID);
    if (pSession) pSession->Send(&pkt, sizeof(pkt));

    // viewList 내 다른 플레이어들에게
    std::lock_guard<std::mutex> lock(pPlayer->m_viewLock);
    for (int32_t nNearID : pPlayer->m_viewList)
    {
        PlayerRef pNear = CPlayer_Manager::Get_Instance()->Get_Player(nNearID);
        if (!pNear) continue;

        auto pNearSession = CSession_Manager::Get_Instance()
            ->Get_Session(pNear->m_nSessionID);
        if (pNearSession) pNearSession->Send(&pkt, sizeof(pkt));
    }
}

void CZone::Broadcast_MonsterHit(MonsterRef pMonster)
{
    SC_MONSTER_HIT_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_MONSTER_HIT;
    pkt.monsterID = pMonster->m_nMonsterID;
    pkt.nHp = pMonster->m_nHp;
    pkt.nMaxHp = pMonster->m_nMaxHp;
    pkt.dir = static_cast<uint8_t>(pMonster->m_eDir);

    std::lock_guard<std::mutex> lock(m_zoneLock);
    for (int32_t nID : m_playerIDs)
    {
        PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(nID);
        if (!pPlayer) continue;

        {
            std::lock_guard<std::mutex> vlock(pPlayer->m_monsterViewLock);
            if (pPlayer->m_monsterViewList.count(pMonster->m_nMonsterID) == 0)
                continue;
        }

        auto pSession = CSession_Manager::Get_Instance()
            ->Get_Session(pPlayer->m_nSessionID);
        if (pSession) pSession->Send(&pkt, sizeof(pkt));
    }
}
void CZone::OnPlayerRespawn(PlayerRef pPlayer)
{
    constexpr float SPAWN_X = 10.f;
    constexpr float SPAWN_Z = 10.f;

    std::unordered_set<int32_t> viewCopy;
    {
        std::lock_guard<std::mutex> lock(pPlayer->m_viewLock);
        viewCopy = pPlayer->m_viewList;
        pPlayer->m_viewList.clear();
    }
    for (int32_t nNearID : viewCopy)
    {
        PlayerRef pNear = CPlayer_Manager::Get_Instance()->Get_Player(nNearID);
        if (!pNear) continue;
        Send_RemovePlayer(pNear, pPlayer->m_nPlayerID);
        {
            std::lock_guard<std::mutex> lock(pNear->m_viewLock);
            pNear->m_viewList.erase(pPlayer->m_nPlayerID);
        }
    }

    pPlayer->m_iHp = pPlayer->m_iMaxHp;
    pPlayer->m_bDead = false;
    pPlayer->m_eState = PLAYER_IDLE;
    pPlayer->m_fCurX = SPAWN_X;
    pPlayer->m_fCurZ = SPAWN_Z;
    pPlayer->m_fDestX = SPAWN_X;
    pPlayer->m_fDestZ = SPAWN_Z;
    pPlayer->m_bMoving = false;
    pPlayer->UpdateTilePos();

    SC_RESPAWN_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_RESPAWN;
    pkt.fCurX = SPAWN_X;
    pkt.fCurZ = SPAWN_Z;
    pkt.nHp = pPlayer->m_iHp;
    pkt.nMaxHp = pPlayer->m_iMaxHp;
    auto pSession = CSession_Manager::Get_Instance()->Get_Session(pPlayer->m_nSessionID);
    if (pSession) pSession->Send(&pkt, sizeof(pkt));

    std::vector<int32_t> vNewView = GetNearPlayers(pPlayer);
    {
        std::lock_guard<std::mutex> vlock(pPlayer->m_viewLock);
        for (int32_t nID : vNewView)
            pPlayer->m_viewList.insert(nID);
    }
    for (int32_t nNearID : vNewView)
    {
        if (nNearID == pPlayer->m_nPlayerID) continue;
        PlayerRef pNear = CPlayer_Manager::Get_Instance()->Get_Player(nNearID);
        if (!pNear) continue;
        Send_AddPlayer(pPlayer, pNear);
        Send_AddPlayer(pNear, pPlayer);
        {
            std::lock_guard<std::mutex> lock(pNear->m_viewLock);
            pNear->m_viewList.insert(pPlayer->m_nPlayerID);
        }
    }
    UpdateMonsterView(pPlayer);
}
