#pragma once
#include "Player.h"
#include <vector>
#include <unordered_set>
#include <mutex>
#include "Monster.h"
#include "Monster_Manager.h"
#include "Timer.h"
#include "PathFinder.h"

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

// 월드에 떨어진 아이템 (값 타입, new 없이 풀로 관리)
struct FDrop
{
    int32_t  id     = 0;
    int32_t  code   = 0;
    int32_t  amount = 0;
    float    x      = 0.f;
    float    z      = 0.f;
    bool     active = false;
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
    void SpawnDrop(int32_t nCode, int32_t nAmount, float fX, float fZ);
    void Send_AllDrops(PlayerRef pTo);
    void Send_AddDrop(PlayerRef pTo, const FDrop& drop);
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
    bool CanSee(PlayerRef pA, PlayerRef pB);
    void UpdateViewAndBroadcast(PlayerRef pPlayer,
        const std::vector<int32_t>& vOldView,
        const std::vector<int32_t>& vNewView,
        uint32_t nMoveTime);


    // ---- 패킷 전송 헬퍼 ----
    void Send_AddPlayer(PlayerRef pTo, PlayerRef pTarget);
    void Send_RemovePlayer(PlayerRef pTo, int32_t nTargetID);
    void Send_MovePlayer(PlayerRef pTo, PlayerRef pMoved, uint32_t nMoveTime);

private:
    int32_t  m_nZoneID = -1;
    char     m_szName[32] = {};

    int32_t              m_nTileCountX = 0;
    int32_t              m_nTileCountZ = 0;
    std::vector<uint8_t> m_tileMap;

    std::unordered_set<int32_t> m_playerIDs;

    // 읽기/쓰기 락. 실제 타입은 ServerConfig.h의 USE_RW_LOCK이 정한다.
    // - 쓰기는 EnterZone/LeaveZone 2곳뿐이고 나머지 10곳은 전부 순회(읽기)다.
    // - 주의: 이 락은 브로드캐스트가 Send()를 쥔 채로 잡는다. 임계구역이 길어서
    //   RW_LOCK_SPIN(스핀락)에는 불리하다. SHARED와 비교하는 편이 낫다.
    FRWLock                     m_zoneLock;

    // ---- 드롭 풀 (고정 배열, new 없음) ----
    static constexpr int32_t MAX_DROPS = 256;
    FDrop      m_drops[MAX_DROPS];
    std::mutex m_dropLock;
};