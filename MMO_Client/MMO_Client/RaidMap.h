#pragma once
#include <vector>
#include <cstdint>

// ================================================================
//  RaidMap.h — 대형 맵(150x150) 블록맵 생성기
//
//  클라/서버 양쪽에 동일 복제본을 둔다.
//  두 가지 맵
//    - 장애물 맵 : A* 길찾기 부하 측정용 (레이드 필드)
//    - 평지 맵   : 장애물이 0개. A* 개선(직선 시야 조기반환 등) 전후 비교
// ================================================================

constexpr int32_t RAID_INNER_X = 150;   // 내부(잔디) 가로
constexpr int32_t RAID_INNER_Z = 150;   // 내부(잔디) 세로
constexpr int32_t RAID_TILE_COUNT = RAID_INNER_X * RAID_INNER_Z;

// 두 대형 맵의 공통 스폰 지점(내부 좌표 기준). 존 좌표계는 테두리 2+1칸이 앞에 붙으므로
// 실제 월드 좌표는 +3 (아래 RAID_SPAWN_WORLD_*).
constexpr int32_t RAID_SPAWN_INNER_X = 75;
constexpr int32_t RAID_SPAWN_INNER_Z = 75;

// 월드 좌표 = 내부좌표 + (OUTER 2 + BORDER 1) + 0.5(타일 중심)
constexpr float RAID_SPAWN_WORLD_X = RAID_SPAWN_INNER_X + 3 + 0.5f;
constexpr float RAID_SPAWN_WORLD_Z = RAID_SPAWN_INNER_Z + 3 + 0.5f;

// 스폰 주변 반경만큼은 장애물 X
constexpr int32_t RAID_SPAWN_CLEAR_RADIUS = 8;

// 바깥 테두리
constexpr int32_t RAID_EDGE_MARGIN = 3;

constexpr uint32_t RAID_MAP_SEED = 20260714u;   // 이 값이 같아야 클라,서버 같음
constexpr int32_t  RAID_BLOB_COUNT = 900;       // 장애물 덩어리 개수

// ---- 대형 맵 몬스터 설정-
// 레이드 몬스터 수는 서버가 정한다(Zone_Manager.cpp 의 CFG_MON_COUNT /
// 환경변수 STRESS_MON_COUNT). 클라는 SC_ADD_MONSTER 로 받으므로 상수가 필요 없다.

// 어그로 범위
constexpr float RAID_AGGRO_RANGE = 8.f;       // 해제는 +5 = 13타일 (히스테리시스)

// ----------------------------------------------------------------
//  std::rand는 구현마다 달라서 쓰면 안 된다
// ----------------------------------------------------------------
struct FRaidRand
{
    uint32_t state;
    explicit FRaidRand(uint32_t seed) : state(seed) {}

    uint32_t Next()
    {
        state = state * 1664525u + 1013904223u;   // Numerical Recipes LCG
        return state;
    }
    // [0, n) 범위 정수
    int32_t Range(int32_t n)
    {
        return static_cast<int32_t>((Next() >> 16) % static_cast<uint32_t>(n));
    }
};

// ----------------------------------------------------------------
//  블록맵 생성. 반환 벡터의 인덱스 = z * RAID_INNER_X + x (0=이동가능, 1=장애물)
//  존 생성자가 배치와 동일
//
//  장애물은 작은 덩어리를 흩뿌리는 방식
// ----------------------------------------------------------------
inline std::vector<int> MakeRaidBlockMap(bool bFlat)
{
    std::vector<int> map(RAID_TILE_COUNT, 0);
    if (bFlat) return map;   // 평지: 장애물 0개

    FRaidRand rng(RAID_MAP_SEED);

    for (int32_t i = 0; i < RAID_BLOB_COUNT; ++i)
    {
        int32_t bx = rng.Range(RAID_INNER_X);
        int32_t bz = rng.Range(RAID_INNER_Z);
        int32_t w  = 1 + rng.Range(3);
        int32_t h  = 1 + rng.Range(3);

        for (int32_t z = bz; z < bz + h; ++z)
        {
            for (int32_t x = bx; x < bx + w; ++x)
            {
                if (x < 0 || x >= RAID_INNER_X) continue;
                if (z < 0 || z >= RAID_INNER_Z) continue;

                // 가장자리 여백 — 맵 둘레를 따라 항상 통행 가능하게
                if (x < RAID_EDGE_MARGIN || x >= RAID_INNER_X - RAID_EDGE_MARGIN) continue;
                if (z < RAID_EDGE_MARGIN || z >= RAID_INNER_Z - RAID_EDGE_MARGIN) continue;

                // 스폰 안전지대
                int32_t dx = x - RAID_SPAWN_INNER_X;
                int32_t dz = z - RAID_SPAWN_INNER_Z;
                if (dx * dx + dz * dz <= RAID_SPAWN_CLEAR_RADIUS * RAID_SPAWN_CLEAR_RADIUS)
                    continue;

                map[z * RAID_INNER_X + x] = 1;
            }
        }
    }
    return map;
}

// 최초 1회만 생성해서 캐시
inline const int* GetRaidBlockMap(bool bFlat)
{
    static const std::vector<int> s_obstacle = MakeRaidBlockMap(false);
    static const std::vector<int> s_flat     = MakeRaidBlockMap(true);
    return bFlat ? s_flat.data() : s_obstacle.data();
}
