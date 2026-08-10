#pragma once
#include <vector>
#include <cstdint>

// ================================================================
//      400x400 블록맵 생성기
//    - 장애물 맵 : A* 길찾기 부하 측정용 (+ 레이드 필드)
//    - 평지 맵   : 장애물 X A* 개선(직선 시야 조기반환 등) 
// ================================================================

constexpr int32_t RAID_INNER_X = 400;   // 내부(잔디) 가로
constexpr int32_t RAID_INNER_Z = 400;   // 내부(잔디) 세로
constexpr int32_t RAID_TILE_COUNT = RAID_INNER_X * RAID_INNER_Z;

constexpr int32_t RAID_SPAWN_INNER_X = 200;
constexpr int32_t RAID_SPAWN_INNER_Z = 200;

// 월드 좌표
constexpr float RAID_SPAWN_WORLD_X = RAID_SPAWN_INNER_X + 3 + 0.5f;
constexpr float RAID_SPAWN_WORLD_Z = RAID_SPAWN_INNER_Z + 3 + 0.5f;

// 스폰지 장애물 배치 X
constexpr int32_t RAID_SPAWN_CLEAR_RADIUS = 8;

// 바깥 테두리 안쪽 마진
constexpr int32_t RAID_EDGE_MARGIN = 3;
constexpr uint32_t RAID_MAP_SEED = 20260714u;   // 클라와 똑같은 씨드값
constexpr int32_t  RAID_BLOB_COUNT = 6400;      // 장애물 덩어리 개수

// ---- 대형 맵 몬스터 설정
constexpr float RAID_AGGRO_RANGE = 8.f;         // 몬스터 어그로  범위

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
//  블록맵 생성
// 
//  장애물은 작은 덩어리를 흩뿌리는 방식 
//  작은 덩어리 + 가장자리 여백 = 항상 연결이 유지
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
        int32_t w  = 1 + rng.Range(3);   // 1~3
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

// 최초 1회만 생성해서 캐시 -함수 지역 static은 쓰레드 안전 초기화(C++11)
inline const int* GetRaidBlockMap(bool bFlat)
{
    static const std::vector<int> s_obstacle = MakeRaidBlockMap(false);
    static const std::vector<int> s_flat     = MakeRaidBlockMap(true);
    return bFlat ? s_flat.data() : s_obstacle.data();
}
