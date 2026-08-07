#include "pch.h"
#include "Zone.h"
#include "Player_Manager.h"
#include "Session_Manager.h"
#include "Protocol.h"
#include "ServerItem.h"
#include "StressMetrics.h"
#include "DB_Manager.h"   // 거래 로그(EnqueueLog)
#include <atomic>
#include <iostream>
#include <cstring>
#include <cstdlib>

// 부하 테스트 빌드에선 전투/AI 이벤트 로그를 무음
#ifdef STRESS_TEST
struct NullSink {
    template <class T> NullSink& operator<<(const T&) { return *this; }
    NullSink& operator<<(std::ostream& (*)(std::ostream&)) { return *this; }
};
#define SLOG (NullSink{})
#else
#define SLOG std::cout
#endif

#ifdef STRESS_TEST
// 몬스터 리스폰 간격
static int StressMonRespawnMs()
{
    static int ms = [] {
        char buf[32]; size_t len = 0;
        if (getenv_s(&len, buf, sizeof(buf), "STRESS_MON_RESPAWN_MS") == 0 && len > 0)
        {
            int v = atoi(buf);
            if (v >= 1000 && v <= 120000) return v;
        }
        return 15000;
    }();
    return ms;
}
#endif

// 전역 드롭 ID 발급 (존 공통)
static std::atomic<int32_t> g_nextDropId{ 1 };

// 몬스터 브로드캐스트에서 후보 플레이어를 모을 범위
//  시야 이탈을 보내려면 여유 필요
static constexpr int32_t MON_BROADCAST_RANGE = VIEW_RANGE + 3;



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

#if USE_SECTOR_AOI
    // 타일 그리드를 SECTOR_SIZE 로 나눠 올림
    m_nSectorCountX = (m_nTileCountX + SECTOR_SIZE - 1) / SECTOR_SIZE;
    m_nSectorCountZ = (m_nTileCountZ + SECTOR_SIZE - 1) / SECTOR_SIZE;
    m_sectors.resize(static_cast<size_t>(m_nSectorCountX) * m_nSectorCountZ);
#endif

    std::cout << "[CZone] '" << m_szName << "' 생성. "
        << m_nTileCountX << "x" << m_nTileCountZ;
#if USE_SECTOR_AOI
    std::cout << "  섹터 " << m_nSectorCountX << "x" << m_nSectorCountZ
              << "(" << SECTOR_SIZE << "타일)";
#endif
    std::cout << std::endl;
}

CZone::~CZone() {}

#if USE_SECTOR_AOI
int32_t CZone::SectorIndexOf(int32_t nTileX, int32_t nTileZ) const
{
    if (nTileX < 0 || nTileX >= m_nTileCountX) return -1;
    if (nTileZ < 0 || nTileZ >= m_nTileCountZ) return -1;
    return (nTileZ / SECTOR_SIZE) * m_nSectorCountX + (nTileX / SECTOR_SIZE);
}

// 현재 타일에 맞는 섹터로 이동. 섹터가 안 바뀌면 락 X
void CZone::UpdatePlayerSector(PlayerRef pPlayer)
{
    const int32_t nNew = SectorIndexOf(pPlayer->m_nTileX, pPlayer->m_nTileZ);
    const int32_t nOld = pPlayer->m_nSectorIdx;
    if (nNew == nOld) return;

    WRITE_LOCK(m_zoneLock);

    // 이 존 소속일 때만 섹터에 넣는다.
    if (m_playerIDs.find(pPlayer->m_nPlayerID) == m_playerIDs.end())
        return;

    if (nOld >= 0 && nOld < static_cast<int32_t>(m_sectors.size()))
        m_sectors[nOld].erase(pPlayer->m_nPlayerID);
    if (nNew >= 0)
        m_sectors[nNew].insert(pPlayer->m_nPlayerID);
    pPlayer->m_nSectorIdx = nNew;
}

void CZone::CollectPlayersNear(int32_t nTileX, int32_t nTileZ, int32_t nRange,
                               std::vector<int32_t>& outIDs)
{
    outIDs.clear();

    // 반경이 걸치는 섹터 범위를 좌표에서 직접 구함
    int32_t sxMin = (nTileX - nRange) / SECTOR_SIZE;
    int32_t sxMax = (nTileX + nRange) / SECTOR_SIZE;
    int32_t szMin = (nTileZ - nRange) / SECTOR_SIZE;
    int32_t szMax = (nTileZ + nRange) / SECTOR_SIZE;

    // C++ 정수 나눗셈은 음수를 0 방향으로 자르지만(floor 아님),
    // 어차피 0 미만은 아래에서 0으로 잘라내므로 결과가 같다.
    if (sxMin < 0) sxMin = 0;
    if (szMin < 0) szMin = 0;
    if (sxMax >= m_nSectorCountX) sxMax = m_nSectorCountX - 1;
    if (szMax >= m_nSectorCountZ) szMax = m_nSectorCountZ - 1;

    READ_LOCK(m_zoneLock);
    for (int32_t sz = szMin; sz <= szMax; ++sz)
    {
        for (int32_t sx = sxMin; sx <= sxMax; ++sx)
        {
            const auto& sector = m_sectors[sz * m_nSectorCountX + sx];
            outIDs.insert(outIDs.end(), sector.begin(), sector.end());
        }
    }
}

void CZone::RemovePlayerFromSector(PlayerRef pPlayer)
{
    const int32_t nOld = pPlayer->m_nSectorIdx;
    if (nOld < 0) return;

    WRITE_LOCK(m_zoneLock);
    if (nOld < static_cast<int32_t>(m_sectors.size()))
        m_sectors[nOld].erase(pPlayer->m_nPlayerID);
    pPlayer->m_nSectorIdx = -1;
}
#else
void CZone::UpdatePlayerSector(PlayerRef)      {}
void CZone::RemovePlayerFromSector(PlayerRef)  {}

// 섹터가 없으면 존 전원 탐색
void CZone::CollectPlayersNear(int32_t, int32_t, int32_t,
                               std::vector<int32_t>& outIDs)
{
    READ_LOCK(m_zoneLock);
    outIDs.assign(m_playerIDs.begin(), m_playerIDs.end());
}
#endif

bool CZone::IsMovable(int32_t nTileX, int32_t nTileZ) const
{
    if (nTileX < 0 || nTileX >= m_nTileCountX) return false;
    if (nTileZ < 0 || nTileZ >= m_nTileCountZ) return false;
    return Is_MovableTile(static_cast<TILE_TYPE>(
        m_tileMap[nTileZ * m_nTileCountX + nTileX]));
}

// 부하 테스트: 이동 가능한 랜덤 타일을 골라 타일 중심 월드좌표로 반환.
// 스레드마다 독립적인 난수
static uint32_t NextSpawnRand()
{
    thread_local uint32_t s_state = [] {
        static std::atomic<uint32_t> s_seq{ 0 };
        uint32_t v = 0x9E3779B9u * (s_seq.fetch_add(1, std::memory_order_relaxed) + 1)
                   ^ static_cast<uint32_t>(GetTickCount64());
        return v ? v : 1u;      // xorshift 는 0이면 영원히 0이다
    }();

    s_state ^= s_state << 13;
    s_state ^= s_state >> 17;
    s_state ^= s_state << 5;
    return s_state;
}

bool CZone::FindWalkableSpawn(float& fOutX, float& fOutZ) const
{
    if (m_nTileCountX <= 0 || m_nTileCountZ <= 0) return false;
    for (int nTry = 0; nTry < 256; ++nTry)
    {
        int nTx = static_cast<int>(NextSpawnRand() % m_nTileCountX);
        int nTz = static_cast<int>(NextSpawnRand() % m_nTileCountZ);
        if (IsMovable(nTx, nTz))
        {
            fOutX = static_cast<float>(nTx) + 0.5f;
            fOutZ = static_cast<float>(nTz) + 0.5f;
            return true;
        }
    }
    return false;
}

// 오브젝트가 가리는 칸을 이동 불가로 지정 (마을용, 클라 Zone_Town과 동일 목록)
void CZone::SetBlock(int32_t nTileX, int32_t nTileZ)
{
    if (nTileX < 0 || nTileX >= m_nTileCountX) return;
    if (nTileZ < 0 || nTileZ >= m_nTileCountZ) return;
    m_tileMap[nTileZ * m_nTileCountX + nTileX] = static_cast<uint8_t>(TILE_BLOCK);
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
        WRITE_LOCK(m_zoneLock);     // 존 인원 추가 - 배타
        m_playerIDs.insert(pPlayer->m_nPlayerID);
    }
    // 락을 놓은 뒤에 부른다(m_zoneLock 은 재귀 불가). GetNearPlayers 보다 먼저
    // 넣어야 자기 자신이 섹터에 있는 상태로 시야를 계산한다.
    UpdatePlayerSector(pPlayer);

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
    Send_AllDrops(pPlayer);   // 존에 남아있는 드롭 전송
}

// ================================================================
//  LeaveZone
// ================================================================
void CZone::LeaveZone(PlayerRef pPlayer)
{
    {
        WRITE_LOCK(m_zoneLock);     // 존 인원 제거 - 배타
        m_playerIDs.erase(pPlayer->m_nPlayerID);
    }
    RemovePlayerFromSector(pPlayer);   // 락 밖에서(재귀 불가)

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

        // 상대방 클라에서 나를 제거
        Send_RemovePlayer(pNear, pPlayer->m_nPlayerID);
        // 내 클라에서 상대방 제거 (존을 떠날 때 옛 존 플레이어가 남지 않도록)
        Send_RemovePlayer(pPlayer, nNearID);

        {
            std::lock_guard<std::mutex> lock(pNear->m_viewLock);
            pNear->m_viewList.erase(pPlayer->m_nPlayerID);
        }
    }
}

// ================================================================
//  OnMoveDest 마우스 클릭 시 1번 호출
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
    // 섹터 경계를 넘었으면 먼저 옮긴다. 시야 계산 전에 해야 내가 올바른
    // 섹터에 있는 상태로 상대에게도 보인다.
    UpdatePlayerSector(pPlayer);

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
//  OnMoveStop ? UI 진입 등으로 이동 강제 정지
//
//  클라가 UI를 열면 그 자리에 멈추지만 CS_MOVE_POS는 더 안 온다.
//  서버가 이걸 모르면 m_bMoving이 true로 남아 GetCurrentPos가 옛 목적지로
//  오버슈트 → 몬스터 AI가 잘못된 위치를 때린다.
//  그래서 보고된 현재 위치를 커밋하고 이동을 멈춘다.
//  (플레이어는 여전히 월드에 정상 존재 → 몬스터가 제 위치를 때림)
// ================================================================
void CZone::OnMoveStop(PlayerRef pPlayer, float fCurX, float fCurZ)
{
    if (pPlayer->m_bDead) return;

    uint32_t nNow = static_cast<uint32_t>(GetTickCount64());

    // 서버 추정 위치와 비교해 허용 오차(2타일) 내면 보고 위치 채택, 넘으면 서버 위치로 스냅(치트 방지)
    if (pPlayer->m_bMoving)
    {
        float fServerX, fServerZ;
        pPlayer->GetCurrentPos(nNow, fServerX, fServerZ);
        float fDiffX = fCurX - fServerX;
        float fDiffZ = fCurZ - fServerZ;
        if (sqrtf(fDiffX * fDiffX + fDiffZ * fDiffZ) > 2.f)
        {
            fCurX = fServerX;
            fCurZ = fServerZ;
        }
    }

    pPlayer->m_fCurX  = fCurX;
    pPlayer->m_fCurZ  = fCurZ;
    pPlayer->m_fDestX = fCurX;
    pPlayer->m_fDestZ = fCurZ;
    pPlayer->m_bMoving = false;
    pPlayer->m_nLastMoveTime = nNow;
    pPlayer->UpdateTilePos();
    UpdatePlayerSector(pPlayer);   // 위치 커밋으로 섹터가 바뀔 수 있다

    // 다른 플레이어들도 정지(cur=dest)로 보이도록 브로드캐스트
    std::lock_guard<std::mutex> lock(pPlayer->m_viewLock);
    for (int32_t nNearID : pPlayer->m_viewList)
    {
        PlayerRef pNear = CPlayer_Manager::Get_Instance()->Get_Player(nNearID);
        if (pNear) Send_MovePlayer(pNear, pPlayer, nNow);
    }
}

// ================================================================
//  GetNearPlayers 존 전체 순회 + 타일 거리로 시야 판정
//  - 섹터 ON 이면 전체 순회 대신 CollectPlayersNear(범위 계산은 그쪽에 있다)
// ================================================================
std::vector<int32_t> CZone::GetNearPlayers(PlayerRef pPlayer)
{
    // 부하 계측: 락 대기 + O(N) 전체 순회 시간을 기록 (섹터 AOI 개선 전/후 비교용)
    const int64_t nAoiT0 = StressMetrics::Now();
    std::vector<int32_t> vResult;
    uint32_t nScanned = 0;

#if USE_SECTOR_AOI
    // 시야 사각형이 실제로 걸치는 섹터 범위만 탐색
    static thread_local std::vector<int32_t> vCandidates;
    CollectPlayersNear(pPlayer->m_nTileX, pPlayer->m_nTileZ, VIEW_RANGE, vCandidates);

    nScanned = static_cast<uint32_t>(vCandidates.size());
    for (int32_t nID : vCandidates)
    {
        if (nID == pPlayer->m_nPlayerID) continue;

        PlayerRef pOther = CPlayer_Manager::Get_Instance()->Get_Player(nID);
        if (!pOther) continue;

        // 거리 검사
        if (CanSee(pPlayer, pOther))
            vResult.push_back(nID);
    }
#else
    {
        READ_LOCK(m_zoneLock);      // 순회만 - 읽기 (AOI 최다 호출 지점)
        nScanned = static_cast<uint32_t>(m_playerIDs.size());
        for (int32_t nID : m_playerIDs)
        {
            if (nID == pPlayer->m_nPlayerID) continue;

            PlayerRef pOther = CPlayer_Manager::Get_Instance()->Get_Player(nID);
            if (!pOther) continue;

            if (CanSee(pPlayer, pOther))
                vResult.push_back(nID);
        }
    }
#endif

    StressMetrics::RecordAoi(StressMetrics::ElapsedUs(nAoiT0), nScanned);
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
//  new에 있고 old에 없음 => 새로 보임 => SC_ADD_PLAYER 양방향
//  old에 있고 new에 없음 => 시야 밖   => SC_REMOVE_PLAYER 양방향
//  둘 다 있음           => 계속 보임 => SC_MOVE_PLAYER
// ================================================================
void CZone::UpdateViewAndBroadcast(PlayerRef pPlayer,
    const std::vector<int32_t>& vOldView,
    const std::vector<int32_t>& vNewView,
    uint32_t nMoveTime)
{
    std::unordered_set<int32_t> setOld(vOldView.begin(), vOldView.end());
    std::unordered_set<int32_t> setNew(vNewView.begin(), vNewView.end());

    // 이동 패킷은 나와 시야 안 전원에게 내용이 똑같다.
    // 수신자마다 다시 채울 이유가 없어 한 번만 만들어 돌려 쓴다.
    SC_MOVE_PLAYER_PACKET movePkt = {};
    movePkt.header.size = sizeof(movePkt);
    movePkt.header.id = SC_MOVE_PLAYER;
    movePkt.playerID = pPlayer->m_nPlayerID;
    movePkt.fCurX = pPlayer->m_fCurX;
    movePkt.fCurZ = pPlayer->m_fCurZ;
    movePkt.fDestX = pPlayer->m_fDestX;
    movePkt.fDestZ = pPlayer->m_fDestZ;
    movePkt.fSpeed = pPlayer->m_fSpeed;
    movePkt.moveTime = nMoveTime;
    SendPayload movePayload = MAKE_SEND_PAYLOAD(movePkt);

    // 나 자신에게 이동 확인 패킷
    Send_MovePlayer(pPlayer, movePayload);

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
            Send_MovePlayer(pNear, movePayload);
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

// 이미 만들어 둔 이동 패킷을 그대로 보낸다. (UpdateViewAndBroadcast 전용)
void CZone::Send_MovePlayer(PlayerRef pTo, const SendPayload& payload)
{
    auto pSession = CSession_Manager::Get_Instance()->Get_Session(pTo->m_nSessionID);
    if (!pSession) return;

    pSession->Send(payload);
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

    SLOG << "[Zone] 몬스터 스폰. ID=" << nID
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

    // ── 부유 몬스터(WING): 길찾기 없이 플레이어를 향해 직선 이동 (장애물 무시) ──
    if (pMonster->m_eType == MONSTER_WING)
    {
        float fNX = fDX / fDist;
        float fNZ = fDZ / fDist;
        MONSTER_DIR eNewDir = pMonster->CalcDirection(fNX, fNZ);

        pMonster->m_eState = MON_WALK;
        pMonster->m_eDir = eNewDir;
        pMonster->m_fDestX = fPlayerX;
        pMonster->m_fDestZ = fPlayerZ;

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

        // 이동 후 공격 범위 진입 → 멈추고 플레이어 방향 바라봄
        float fAfterDX = fPlayerX - pMonster->m_fCurX;
        float fAfterDZ = fPlayerZ - pMonster->m_fCurZ;
        float fAfterDist = sqrtf(fAfterDX * fAfterDX + fAfterDZ * fAfterDZ);
        if (fAfterDist <= pMonster->m_fAtkRange)
        {
            pMonster->m_eState = MON_IDLE;
            if (fAfterDist > 0.001f)
                pMonster->m_eDir = pMonster->CalcDirection(fAfterDX / fAfterDist, fAfterDZ / fAfterDist);
            pMonster->m_fDestX = pMonster->m_fCurX;
            pMonster->m_fDestZ = pMonster->m_fCurZ;
            pMonster->UpdateTilePos();
            Broadcast_MoveMonster(pMonster);
            return;
        }

        bool bTileChanged = pMonster->UpdateTilePos();
        if (bTileChanged || ePrevDir != eNewDir || ePrevState != MON_WALK)
            Broadcast_MoveMonster(pMonster);
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

    // 반경은 시야(5)가 아니라 어그로 범위
    //  +1 은 부동소수 반경을 타일로 올림하면서 경계에서 놓치지 않기 위한 여유.
    const int32_t nRange = static_cast<int32_t>(pMonster->m_fAggroRange) + 1;

    // 스레드마다 버퍼를 재사용한다. 매 호출 새로 만들면 초당 수백 번 힙을 두들겨
    // 할당자 락 경합으로 꼬리지연(max)이 튄다. clear()는 capacity를 유지한다.
    static thread_local std::vector<int32_t> vCandidates;
    CollectPlayersNear(pMonster->m_nTileX, pMonster->m_nTileZ, nRange, vCandidates);

    for (int32_t nID : vCandidates)
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

    // 여긴 어그로 "해제" 판정이라 어그로보다 반경이 크다
    const int32_t nRange = static_cast<int32_t>(pMonster->m_fDeAggroRange) + 1;

    // 스레드마다 버퍼를 재사용한다. 매 호출 새로 만들면 초당 수백 번 힙을 두들겨
    // 할당자 락 경합으로 꼬리지연(max)이 튄다. clear()는 capacity를 유지한다.
    static thread_local std::vector<int32_t> vCandidates;
    CollectPlayersNear(pMonster->m_nTileX, pMonster->m_nTileZ, nRange, vCandidates);

    for (int32_t nID : vCandidates)
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
    pkt.dir = static_cast<uint8_t>(pMonster->m_eDir);   // 시야 진입 시 방향 동기화
    pkt.fCurX = pMonster->m_fCurX;
    pkt.fCurZ = pMonster->m_fCurZ;
    pkt.fDestX = pMonster->m_fDestX;
    pkt.fDestZ = pMonster->m_fDestZ;
    pkt.fSpeed = pMonster->m_fSpeed;
    pSession->Send(&pkt, sizeof(pkt));
}

// 이미 만들어 둔 몬스터 생성 패킷을 그대로 보낸다.
void CZone::Send_AddMonster(PlayerRef pTo, const SendPayload& payload)
{
    auto pSession = CSession_Manager::Get_Instance()
        ->Get_Session(pTo->m_nSessionID);
    if (!pSession) return;

    pSession->Send(payload);
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
    // 수신자와 무관하게 내용이 같으므로 루프 밖에서 한 번만 만든다.
    SC_ADD_MONSTER_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_ADD_MONSTER;
    pkt.monsterID = pMonster->m_nMonsterID;
    pkt.monsterType = static_cast<uint8_t>(pMonster->m_eType);
    pkt.state = static_cast<uint8_t>(pMonster->m_eState);
    pkt.dir = static_cast<uint8_t>(pMonster->m_eDir);
    pkt.fCurX = pMonster->m_fCurX;
    pkt.fCurZ = pMonster->m_fCurZ;
    pkt.fDestX = pMonster->m_fDestX;
    pkt.fDestZ = pMonster->m_fDestZ;
    pkt.fSpeed = pMonster->m_fSpeed;
    SendPayload addPayload = MAKE_SEND_PAYLOAD(pkt);

    // 스레드마다 버퍼를 재사용한다. 매 호출 새로 만들면 초당 수백 번 힙을 두들겨
    // 할당자 락 경합으로 꼬리지연(max)이 튄다. clear()는 capacity를 유지한다.
    static thread_local std::vector<int32_t> vCandidates;
    CollectPlayersNear(pMonster->m_nTileX, pMonster->m_nTileZ,
                       MON_BROADCAST_RANGE, vCandidates);

    for (int32_t nID : vCandidates)
    {
        PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(nID);
        if (!pPlayer) continue;

        // 시야 리스트 기반
        std::lock_guard<std::mutex> vlock(pPlayer->m_monsterViewLock);
        if (pPlayer->m_monsterViewList.count(pMonster->m_nMonsterID) == 0)
            continue;

        Send_AddMonster(pPlayer, addPayload);
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
    SendPayload movePayload = MAKE_SEND_PAYLOAD(pkt);

    // 몬스터 주변 섹터의 플레이어만 후보
    static thread_local std::vector<int32_t> vCandidates;
    CollectPlayersNear(pMonster->m_nTileX, pMonster->m_nTileZ,
                       MON_BROADCAST_RANGE, vCandidates);

    for (int32_t nID : vCandidates)
    {
        PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(nID);
        if (!pPlayer) continue;

        // 몬스터 이동에 따른 시야 진입/이탈 처리
        int32_t nDX = abs(pPlayer->m_nTileX - pMonster->m_nTileX);
        int32_t nDZ = abs(pPlayer->m_nTileZ - pMonster->m_nTileZ);
        bool bInRange = (nDX <= VIEW_RANGE && nDZ <= VIEW_RANGE);

        bool bWasInView;
        {
            std::lock_guard<std::mutex> vlock(pPlayer->m_monsterViewLock);
            bWasInView = (pPlayer->m_monsterViewList.count(pMonster->m_nMonsterID) != 0);
            if (bInRange && !bWasInView)
                pPlayer->m_monsterViewList.insert(pMonster->m_nMonsterID);
            else if (!bInRange && bWasInView)
                pPlayer->m_monsterViewList.erase(pMonster->m_nMonsterID);
        }

        auto pSession = CSession_Manager::Get_Instance()
            ->Get_Session(pPlayer->m_nSessionID);
        if (!pSession) continue;

        if (bInRange)
        {
            if (!bWasInView)
                Send_AddMonster(pPlayer, pMonster);   // 새로 진입 → 먼저 생성
            pSession->Send(movePayload);               // 이어서 이동 갱신
        }
        else if (bWasInView)
        {
            Send_RemoveMonster(pPlayer, pMonster->m_nMonsterID);  // 시야 이탈 → 제거
        }
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
    SendPayload statePayload = MAKE_SEND_PAYLOAD(pkt);

    // 이 몬스터가 시야에 든 플레이어만 대상이므로 주변 섹터로 좁혀도 안전
    // 스레드마다 버퍼를 재사용한다. 매 호출 새로 만들면 초당 수백 번 힙을 두들겨
    // 할당자 락 경합으로 꼬리지연(max)이 튄다. clear()는 capacity를 유지한다.
    static thread_local std::vector<int32_t> vCandidates;
    CollectPlayersNear(pMonster->m_nTileX, pMonster->m_nTileZ,
                       MON_BROADCAST_RANGE, vCandidates);

    for (int32_t nID : vCandidates)
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
        if (pSession) pSession->Send(statePayload);
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
        SLOG << "[경고] 공격 거리 초과. PlayerID="
            << pPlayer->m_nPlayerID
            << " 거리=" << fDist << std::endl;
        return;
    }

    // 공격 시 플레이어 정지 처리
    pPlayer->m_fCurX  = fPlayerX;
    pPlayer->m_fCurZ  = fPlayerZ;
    pPlayer->m_fDestX = fPlayerX;
    pPlayer->m_fDestZ = fPlayerZ;
    pPlayer->m_bMoving = false;
    pPlayer->m_nLastMoveTime = nNow;
    pPlayer->UpdateTilePos();
    UpdatePlayerSector(pPlayer);   // 위치 커밋으로 섹터가 바뀔 수 있다

    //공격 방향 계산
    {
        float fFaceDX = pMonster->m_fCurX - fPlayerX;
        float fFaceDZ = pMonster->m_fCurZ - fPlayerZ;
        float fFaceLen = sqrtf(fFaceDX * fFaceDX + fFaceDZ * fFaceDZ);
        if (fFaceLen > 0.001f)
        {
            float fNX = fFaceDX / fFaceLen;
            float fNZ = fFaceDZ / fFaceLen;
            constexpr float TILE_HALF_W = 64.f;
            constexpr float TILE_HALF_H = 32.f;
            float fScreenDX = (fNX - fNZ) * TILE_HALF_W;
            float fScreenDY = (fNX + fNZ) * TILE_HALF_H;
            float fAngle = atan2f(fScreenDY, fScreenDX) * 180.f / 3.14159f;
            uint8_t nDir = pPlayer->m_eDir;
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

    // 공격자 모션 브로드캐스트
    Broadcast_PlayerState(pPlayer, PLAYER_ATTACK);

    // HP 감소
    pMonster->m_nHp -= pPlayer->Get_Atk();

    SLOG << "[Zone] 몬스터 피격. ID=" << nMonsterID
        << " HP=" << pMonster->m_nHp
        << "/" << pMonster->m_nMaxHp << std::endl;

    // 사망 처리 
    if (pMonster->m_nHp <= 0)
    {
        pMonster->m_nHp = 0;
        pMonster->m_eState = MON_DEAD;
        pMonster->m_bActive = false;
        pMonster->m_nHitStunEndTime = 0;

        Broadcast_MonsterState(pMonster);

#ifdef STRESS_TEST
        AddTimer(nMonsterID, EEventType::MonsterRespawn, StressMonRespawnMs());
#else
        AddTimer(nMonsterID, EEventType::MonsterRespawn, 5000);
#endif

        // 사망 위치에 아이템 드롭 추첨
        FDropRoll roll = RollDrop();
        if (roll.bDrop)
            SpawnDrop(roll.code, roll.amount,
                pMonster->m_fCurX, pMonster->m_fCurZ,
                static_cast<int32_t>(pMonster->m_eType));

        // 경험치 지급 (막타플레이어에게만)
        int32_t nLevelUp = pPlayer->AddExp(pMonster->m_nExpReward);
        Send_PlayerExp(pPlayer, nLevelUp > 0);
        if (nLevelUp > 0)
        {
            Send_PlayerHp(pPlayer);
            SLOG << "[Zone] 레벨업! PlayerID=" << pPlayer->m_nPlayerID
                << " Lv=" << pPlayer->m_nLevel
                << " (Atk=" << pPlayer->Get_Atk()
                << " Def=" << pPlayer->Get_Def()
                << " MaxHp=" << pPlayer->m_iMaxHp << ")" << std::endl;
        }

        SLOG << "[Zone] 몬스터 사망. ID=" << nMonsterID
            << " - PlayerID=" << pPlayer->m_nPlayerID
            << " 경험치 +" << pMonster->m_nExpReward
            << " (Lv" << pPlayer->m_nLevel
            << " " << pPlayer->m_nExp
            << "/" << CPlayer::ExpToNext(pPlayer->m_nLevel) << ")" << std::endl;
        return;
    }

    // ---- 피격 처리 ----
    // 몬스터가 공격자 방향 바라봄
    float fLen = sqrtf(fDX * fDX + fDZ * fDZ);
    if (fLen > 0.001f)
        pMonster->m_eDir = pMonster->CalcDirection(fDX / fLen, fDZ / fLen);

    pMonster->m_eState = MON_HIT;

    // 피격 시 이동 취소 (제자리 경직). 스턴이 풀리면 AI가 다시 사거리 판단.
    pMonster->m_fDestX = pMonster->m_fCurX;
    pMonster->m_fDestZ = pMonster->m_fCurZ;
    pMonster->UpdateTilePos();

    // 경직 시간 세팅
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

#ifdef STRESS_TEST
    // 부하 테스트: 대형(레이드) 맵은 죽은 자리에 다시 살지 않고 랜덤 위치로 부활.
    if (m_nTileCountX >= 100)
    {
        float rx = pMonster->m_fSpawnX, rz = pMonster->m_fSpawnZ;
        if (FindWalkableSpawn(rx, rz))
        {
            pMonster->m_fSpawnX = rx;   // 순찰 홈도 새 위치로
            pMonster->m_fSpawnZ = rz;
        }
    }
#endif

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

    SLOG << "[Zone] 몬스터 리스폰. ID=" << nMonsterID << std::endl;

    bool bActivated = false;  // AI 중복 활성화 방지

    // 스레드마다 버퍼를 재사용한다. 매 호출 새로 만들면 초당 수백 번 힙을 두들겨
    // 할당자 락 경합으로 꼬리지연(max)이 튄다. clear()는 capacity를 유지한다.
    static thread_local std::vector<int32_t> vCandidates;
    CollectPlayersNear(pMonster->m_nTileX, pMonster->m_nTileZ,
                       MON_BROADCAST_RANGE, vCandidates);

    for (int32_t nPlayerID : vCandidates)
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

    // 공격 모션 지연 동안 포탈로 다른 존에 갔을 수 있다.
    // 좌표는 존마다 따로
    if (pTarget->m_nZoneID != m_nZoneID) return;

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
        UpdatePlayerSector(pTarget);   // 피격 정지로 섹터가 바뀔 수 있다
    }

    // 무적 버프 중이면 피해 없음
    if (pTarget->IsInvincible()) return;

    // HP 감소 + 피격 패킷 (받는 피해 = max(1, 몬스터공격 - 방어력))
    constexpr int32_t MONSTER_ATK = 10;
    int32_t nDmg = MONSTER_ATK - pTarget->Get_Def();
    if (nDmg < 1) nDmg = 1;
    pTarget->m_iHp -= nDmg;
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

    SLOG << "[Zone] 몬스터 타격 적용. 몬스터ID=" << nMonsterID
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
    pkt.dir = pPlayer->m_eDir;      // 공격자가 바라보는 방향
    pkt.fCurX = pPlayer->m_fCurX;   // 공격 확정 위치(관찰자 오버슈트 방지)
    pkt.fCurZ = pPlayer->m_fCurZ;
    SendPayload statePayload = MAKE_SEND_PAYLOAD(pkt);

    std::lock_guard<std::mutex> lock(pPlayer->m_viewLock);
    for (int32_t nNearID : pPlayer->m_viewList)
    {
        PlayerRef pNear = CPlayer_Manager::Get_Instance()->Get_Player(nNearID);
        if (!pNear) continue;

        auto pSession = CSession_Manager::Get_Instance()
            ->Get_Session(pNear->m_nSessionID);
        if (pSession) pSession->Send(statePayload);
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
    pkt.fCurX = pPlayer->m_fCurX;   // OnMonsterAttackHit에서 커밋한 정지 위치
    pkt.fCurZ = pPlayer->m_fCurZ;
    SendPayload hitPayload = MAKE_SEND_PAYLOAD(pkt);

    // 피격 당사자에게
    auto pSession = CSession_Manager::Get_Instance()
        ->Get_Session(pPlayer->m_nSessionID);
    if (pSession) pSession->Send(hitPayload);

    // viewList 내 다른 플레이어들에게
    std::lock_guard<std::mutex> lock(pPlayer->m_viewLock);
    for (int32_t nNearID : pPlayer->m_viewList)
    {
        PlayerRef pNear = CPlayer_Manager::Get_Instance()->Get_Player(nNearID);
        if (!pNear) continue;

        auto pNearSession = CSession_Manager::Get_Instance()
            ->Get_Session(pNear->m_nSessionID);
        if (pNearSession) pNearSession->Send(hitPayload);
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
    SendPayload hitPayload = MAKE_SEND_PAYLOAD(pkt);

    // 스레드마다 버퍼를 재사용한다. 매 호출 새로 만들면 초당 수백 번 힙을 두들겨
    // 할당자 락 경합으로 꼬리지연(max)이 튄다. clear()는 capacity를 유지한다.
    static thread_local std::vector<int32_t> vCandidates;
    CollectPlayersNear(pMonster->m_nTileX, pMonster->m_nTileZ,
                       MON_BROADCAST_RANGE, vCandidates);

    for (int32_t nID : vCandidates)
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
        if (pSession) pSession->Send(hitPayload);
    }
}
void CZone::OnPlayerRespawn(PlayerRef pPlayer)
{
    float SPAWN_X = 10.f;
    float SPAWN_Z = 10.f;

#ifdef STRESS_TEST
    // 부하 봇(bot_*)은 랜덤 이동가능 타일로 부활 → 오픈 필드에 계속 흩어진 상태 유지.
    // (실제 플레이어는 그대로 (10,10) 고정)
    if (strncmp(pPlayer->m_szName, "bot_", 4) == 0)
    {
        float rx = SPAWN_X, rz = SPAWN_Z;
        if (FindWalkableSpawn(rx, rz)) { SPAWN_X = rx; SPAWN_Z = rz; }
    }
#endif

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
    // 리스폰은 좌표가 멀리 튀는 대표적인 경우. GetNearPlayers 전에 옮겨야
    // 옛 섹터에 남아 유령으로 보이지 않는다.
    UpdatePlayerSector(pPlayer);

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

// ================================================================
//  아이템 드롭 / 획득
// ================================================================
void CZone::SpawnDrop(int32_t nCode, int32_t nAmount, float fX, float fZ,
                      int32_t nSrcMon)
{
    FDrop made;
    {
        std::lock_guard<std::mutex> lock(m_dropLock);
        int slot = -1;
        for (int i = 0; i < MAX_DROPS; ++i)
            if (!m_drops[i].active) { slot = i; break; }
        if (slot < 0) return;  // 풀 가득 참

        FDrop& d = m_drops[slot];
        d.id     = g_nextDropId.fetch_add(1);
        d.code   = nCode;
        d.amount = nAmount;
        d.x      = fX;
        d.z      = fZ;
        d.active = true;
        d.srcMon = nSrcMon;
        made = d;
    }
    Broadcast_AddDrop(made);

    SLOG << "[Zone] 아이템 드롭. id=" << made.id
        << " code=" << made.code << " amount=" << made.amount << std::endl;
}

void CZone::OnPlayerPickup(PlayerRef pPlayer, uint32_t nDropId)
{
    if (pPlayer->m_bDead) return;

    int32_t nCode = 0, nAmount = 0, nSrcMon = -1;
    bool bFound = false;
    {
        std::lock_guard<std::mutex> lock(m_dropLock);
        for (int i = 0; i < MAX_DROPS; ++i)
        {
            FDrop& d = m_drops[i];
            if (!d.active || d.id != static_cast<int32_t>(nDropId)) continue;

            // 거리 검증 (치트 방지)
            uint32_t nNow = static_cast<uint32_t>(GetTickCount64());
            float fDist = pPlayer->GetDistanceTo(d.x, d.z, nNow);
            if (fDist > 2.0f) return;

            // 인벤 여유를 선점 전에 확인

            if (d.code != ICODE_GOLD && !pPlayer->CanAddItem(d.code, d.amount))
                return;   // 드롭은 바닥에 그대로 남는다

            nCode   = d.code;
            nAmount = d.amount;
            nSrcMon = d.srcMon;
            d.active = false;   // 선점
            bFound = true;
            break;
        }
    }
    if (!bFound) return;

    // 인벤토리 반영
    //  위에서 여유를 확인하고 선점했으므로 여기서는 실패 X
    int32_t nBalance = 0;
    if (nCode == ICODE_GOLD)
        pPlayer->AddGold(nAmount, &nBalance);
    else
    {
        pPlayer->AddItem(nCode, nAmount);
        nBalance = pPlayer->GetGold();
    }

    // 거래 로그: 재화가 경제에 새로 생겨나는 지점(source).
    char szDetail[32] = "mon=";
    GameLog_SetStr(szDetail + 4, sizeof(szDetail) - 4,
        (nSrcMon >= 0) ? MonsterTypeName(static_cast<MONSTER_TYPE>(nSrcMon))
                       : "unknown");

    CDB_Manager::Get_Instance()->EnqueueLog(MakeGameLog(
        LogType::DROP_GAIN, pPlayer->m_szName,
        nCode,
        (nCode == ICODE_GOLD) ? 0 : nAmount,
        (nCode == ICODE_GOLD) ? nAmount : 0,
        nBalance,
        nullptr,        // target 없음
        szDetail));

    Broadcast_RemoveDrop(static_cast<int32_t>(nDropId));
    Send_InvenUpdate(pPlayer);
}

void CZone::Send_AddDrop(PlayerRef pTo, const FDrop& drop)
{
    auto pSession = CSession_Manager::Get_Instance()->Get_Session(pTo->m_nSessionID);
    if (!pSession) return;

    SC_ADD_DROP_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_ADD_DROP;
    pkt.dropId   = drop.id;
    pkt.itemCode = drop.code;
    pkt.amount   = drop.amount;
    pkt.fX       = drop.x;
    pkt.fZ       = drop.z;
    pSession->Send(&pkt, sizeof(pkt));
}

// 이미 만들어 둔 드롭 생성 패킷을 그대로 보낸다.
void CZone::Send_AddDrop(PlayerRef pTo, const SendPayload& payload)
{
    auto pSession = CSession_Manager::Get_Instance()->Get_Session(pTo->m_nSessionID);
    if (!pSession) return;

    pSession->Send(payload);
}

// 드롭은 시야와 무관하게 존 전원에게 보냄
void CZone::Broadcast_AddDrop(const FDrop& drop)
{
    // 존 전원에게 같은 내용이 나가므로 루프 밖에서 한 번만 만든다.
    SC_ADD_DROP_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_ADD_DROP;
    pkt.dropId   = drop.id;
    pkt.itemCode = drop.code;
    pkt.amount   = drop.amount;
    pkt.fX       = drop.x;
    pkt.fZ       = drop.z;
    SendPayload dropPayload = MAKE_SEND_PAYLOAD(pkt);

    static thread_local std::vector<int32_t> vAll;   // 버퍼 재사용(할당 제거)
    {
        READ_LOCK(m_zoneLock);
        vAll.assign(m_playerIDs.begin(), m_playerIDs.end());
    }

    for (int32_t nID : vAll)
    {
        PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(nID);
        if (pPlayer) Send_AddDrop(pPlayer, dropPayload);
    }
}

void CZone::Broadcast_RemoveDrop(int32_t nDropId)
{
    SC_REMOVE_DROP_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_REMOVE_DROP;
    pkt.dropId = nDropId;
    SendPayload removePayload = MAKE_SEND_PAYLOAD(pkt);

    static thread_local std::vector<int32_t> vAll;   // 버퍼 재사용(할당 제거)
    {
        READ_LOCK(m_zoneLock);
        vAll.assign(m_playerIDs.begin(), m_playerIDs.end());
    }

    for (int32_t nID : vAll)
    {
        PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(nID);
        if (!pPlayer) continue;
        auto pSession = CSession_Manager::Get_Instance()->Get_Session(pPlayer->m_nSessionID);
        if (pSession) pSession->Send(removePayload);
    }
}

void CZone::Send_AllDrops(PlayerRef pTo)
{
    std::lock_guard<std::mutex> lock(m_dropLock);
    for (int i = 0; i < MAX_DROPS; ++i)
        if (m_drops[i].active)
            Send_AddDrop(pTo, m_drops[i]);
}

void CZone::Send_InvenUpdate(PlayerRef pPlayer)
{
    auto pSession = CSession_Manager::Get_Instance()->Get_Session(pPlayer->m_nSessionID);
    if (!pSession) return;

    SC_INVEN_UPDATE_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_INVEN_UPDATE;
    pkt.gold = pPlayer->m_gold;
    for (int i = 0; i < CPlayer::INVEN_SIZE; ++i)
    {
        pkt.codes[i]  = pPlayer->m_invenCode[i];
        pkt.counts[i] = pPlayer->m_invenCount[i];
    }
    for (int i = 0; i < CPlayer::EQUIP_SLOTS; ++i)
        pkt.equip[i] = pPlayer->m_equipCode[i];
    pSession->Send(&pkt, sizeof(pkt));
}

void CZone::Send_PlayerHp(PlayerRef pPlayer)
{
    auto pSession = CSession_Manager::Get_Instance()->Get_Session(pPlayer->m_nSessionID);
    if (!pSession) return;

    SC_PLAYER_HP_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_PLAYER_HP;
    pkt.playerID = pPlayer->m_nPlayerID;
    pkt.nHp = pPlayer->m_iHp;
    pkt.nMaxHp = pPlayer->m_iMaxHp;
    pkt.nMp = pPlayer->m_iMp;
    pkt.nMaxMp = pPlayer->m_iMaxMp;
    pSession->Send(&pkt, sizeof(pkt));
}

void CZone::Send_PlayerExp(PlayerRef pPlayer, bool bLevelUp)
{
    auto pSession = CSession_Manager::Get_Instance()->Get_Session(pPlayer->m_nSessionID);
    if (!pSession) return;

    SC_PLAYER_EXP_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_PLAYER_EXP;
    pkt.playerID = pPlayer->m_nPlayerID;
    pkt.level = pPlayer->m_nLevel;
    pkt.exp = pPlayer->m_nExp;
    pkt.maxExp = CPlayer::ExpToNext(pPlayer->m_nLevel);   // 만렙이면 0
    pkt.levelUp = bLevelUp ? 1 : 0;
    pSession->Send(&pkt, sizeof(pkt));
}
