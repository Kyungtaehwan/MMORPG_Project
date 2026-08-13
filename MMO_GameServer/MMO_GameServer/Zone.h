#pragma once
#include "Player.h"
#include <vector>
#include <unordered_set>
#include <mutex>
#include "Monster.h"
#include "Monster_Manager.h"
#include "Timer.h"
#include "PathFinder.h"
#include "SendBuffer.h"

enum TILE_TYPE : uint8_t
{
    TILE_GRASS = 0,
    TILE_BLOCK = 1,
    TILE_BORDER_LT = 2,
    TILE_BORDER_RT = 3,
    TILE_BORDER_RB = 4,
    TILE_BORDER_LB = 5,
    TILE_BORDER_T = 6,
    TILE_BORDER_R = 7,
    TILE_BORDER_B = 8,
    TILE_BORDER_L = 9,
    TILE_OUTSIDE = 10,
};

inline bool Is_MovableTile(TILE_TYPE eType)
{
    return eType == TILE_GRASS;
}

constexpr int32_t VIEW_RANGE = 5;

#if USE_SECTOR_AOI
// 섹터가 시야보다 작으면 버그

static_assert(SECTOR_SIZE >= VIEW_RANGE,
              "SECTOR_SIZE 가 VIEW_RANGE 보다 작음");
#endif

// 월드에 떨어진 아이템
struct FDrop
{
    int32_t  id     = 0;
    int32_t  code   = 0;
    int32_t  amount = 0;
    float    x      = 0.f;
    float    z      = 0.f;
    bool     active = false;
    int32_t  srcMon = -1; //이 드롭을 떨군 몬스터 종류
};

class CZone
{
public:
    CZone(int32_t nZoneID, const char* pszName,
        int32_t nInnerX, int32_t nInnerZ, const int* pBlockMap);
    ~CZone();

    // 입/퇴장
    void EnterZone(PlayerRef pPlayer, float fSpawnX, float fSpawnZ);
    void LeaveZone(PlayerRef pPlayer);

    // 이동 처리

    // 마우스 클릭 시 호출 목적지 검증 + 브로드캐스트
    void OnMoveDest(PlayerRef pPlayer,
        float fDestX, float fDestZ, uint32_t nMoveTime);

    // 클라이언트 타일 변경 시 호출 위치 업데이트 + 시야 재계산
    void OnMovePos(PlayerRef pPlayer,
        float fCurX, float fCurZ, uint32_t nMoveTime);

    // UI 진입 등으로 이동 강제 정지. 현재 위치 커밋 + m_bMoving=false.
    void OnMoveStop(PlayerRef pPlayer, float fCurX, float fCurZ);

    // 유틸
    bool IsMovable(int32_t nTileX, int32_t nTileZ) const;
    void SetBlock(int32_t nTileX, int32_t nTileZ);   // 오브젝트 블락(마을) - 클라와 동일해야 함
    int32_t GetZoneID() { return m_nZoneID; }

    // 부하 테스트: 이동 가능한 랜덤 타일을 찾아 월드좌표(타일 중심)로 반환.
    // 봇을 존 전역에 분산 배치하는 용도. 실패 시 outX/outZ 는 그대로 두고 false.
    bool FindWalkableSpawn(float& fOutX, float& fOutZ) const;


public:
    void SpawnMonster(int32_t nID, MONSTER_TYPE eType, float fX, float fZ);
    void OnMonsterAI(int32_t nMonsterID);
    void UpdateMonsterView(PlayerRef pPlayer);
    void OnPlayerAttackMonster(PlayerRef pPlayer,int32_t nMonsterID, float fPlayerX, float fPlayerZ);
    void OnMonsterRespawn(int32_t nMonsterID);
    void OnMonsterAttackHit(int32_t nMonsterID);
    void OnPlayerRespawn(PlayerRef pPlayer);

    // ---- 아이템 드롭 / 획득 / 인벤·스탯 동기화 ----
    void OnPlayerPickup(PlayerRef pPlayer, uint32_t nDropId);
    void Send_InvenUpdate(PlayerRef pPlayer);   // 인벤+장비 스냅샷
    void Send_PlayerHp(PlayerRef pPlayer);      // HP/MP 동기화
    // 레벨/경험치 동기화. bLevelUp=true면 클라가 레벨업 연출을 띄운다.
    void Send_PlayerExp(PlayerRef pPlayer, bool bLevelUp);
private:
    void SpawnDrop(int32_t nCode, int32_t nAmount, float fX, float fZ,
                   int32_t nSrcMon = -1);
    void Send_AllDrops(PlayerRef pTo);
    void Send_AddDrop(PlayerRef pTo, const FDrop& drop);
    // 브로드캐스트용. 한 번 만든 페이로드를 여러 명에게 재사용한다.
    void Send_AddDrop(PlayerRef pTo, const SendPayload& payload);
    void Broadcast_AddDrop(const FDrop& drop);
    void Broadcast_RemoveDrop(int32_t nDropId);
private:
    // AI 상태별 함수
    void Monster_Chase(MonsterRef pMonster, float fPlayerX, float fPlayerZ);
    void Monster_Attack(MonsterRef pMonster);
    void Monster_Patrol(MonsterRef pMonster);

    // 헬퍼
    PlayerRef FindNearestPlayer(MonsterRef pMonster);
    bool      PlayerExistNear(MonsterRef pMonster);

    // 패킷 전송
    void Broadcast_PlayerState(PlayerRef pPlayer, PLAYER_STATE eState);
    void Broadcast_PlayerHit(PlayerRef pPlayer);
    
    void Send_AddMonster(PlayerRef pTo, MonsterRef pMonster);
    void Send_AddMonster(PlayerRef pTo, const SendPayload& payload);
    void Send_RemoveMonster(PlayerRef pTo, int32_t nMonsterID);
    void Broadcast_AddMonster(MonsterRef pMonster);
    void Broadcast_MoveMonster(MonsterRef pMonster);
    void Broadcast_MonsterState(MonsterRef pMonster, int32_t nTargetID = -1);
    void Broadcast_MonsterHit(MonsterRef pMonster);

    // 몬스터 ID 목록
    std::unordered_set<int32_t> m_monsterIDs;
    std::mutex                  m_monsterLock;

private:
    //시야
    std::vector<int32_t> GetNearPlayers(PlayerRef pPlayer);


    // 섹터로 1차 거르기 이후 cansee
    void CollectPlayersNear(int32_t nTileX, int32_t nTileZ, int32_t nRange,
                            std::vector<int32_t>& outIDs);

    // 플레이어를 현재 타일 좌표에 맞는 섹터로 옮김
    void UpdatePlayerSector(PlayerRef pPlayer);

    void RemovePlayerFromSector(PlayerRef pPlayer);
    bool CanSee(PlayerRef pA, PlayerRef pB);
    void UpdateViewAndBroadcast(PlayerRef pPlayer, const std::vector<int32_t>& vOldView,
        const std::vector<int32_t>& vNewView,uint32_t nMoveTime);


    // ---- 패킷 전송 헬퍼 ----
    void Send_AddPlayer(PlayerRef pTo, PlayerRef pTarget);
    void Send_RemovePlayer(PlayerRef pTo, int32_t nTargetID);
    void Send_MovePlayer(PlayerRef pTo, PlayerRef pMoved, uint32_t nMoveTime);
    void Send_MovePlayer(PlayerRef pTo, const SendPayload& payload);

private:
    int32_t  m_nZoneID = -1;
    char     m_szName[32] = {};

    int32_t              m_nTileCountX = 0;
    int32_t              m_nTileCountZ = 0;
    std::vector<uint8_t> m_tileMap;

    std::unordered_set<int32_t> m_playerIDs;

#if USE_SECTOR_AOI
    int32_t m_nSectorCountX = 0;
    int32_t m_nSectorCountZ = 0;
    std::vector<std::unordered_set<int32_t>> m_sectors;

    // 타일 좌표 -> 섹터 인덱스. 맵 밖이면 -1.
    int32_t SectorIndexOf(int32_t nTileX, int32_t nTileZ) const;
#endif

    // 읽기/쓰기 락
    FRWLock                     m_zoneLock;

   
    static constexpr int32_t MAX_DROPS = 256;

    FDrop      m_drops[MAX_DROPS]; // 드롭 풀
    std::mutex m_dropLock;

};