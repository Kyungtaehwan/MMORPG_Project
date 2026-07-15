#pragma once
#include <vector>
#include <cstdint>

// ================================================================
//  RaidMap.h — 대형 맵(150x150) 블록맵 생성기
//
//  ★ 이 파일은 Protocol.h 와 마찬가지로 클라/서버 양쪽에 동일 복제본을 둔다.
//    한쪽만 고치면 두 맵이 달라져 이동 검증이 깨진다(벽에 끼거나 계속 튕김).
//    반드시 양쪽을 동시에 수정할 것.
//
//  왜 하드코딩 배열이 아니라 생성 함수인가:
//    150x150 = 22,500칸을 손으로 찍을 수 없다. 고정 시드 LCG로 절차 생성하면
//    클라와 서버가 같은 코드로 같은 맵을 만들므로 두 벌을 손으로 맞출 필요가 없다.
//
//  두 가지 맵을 만든다 (장애물 유무만 다르고 크기/스폰은 동일):
//    - 장애물 맵 : A* 길찾기 부하 측정용 (+ 레이드 필드)
//    - 평지 맵   : 장애물이 0개. A* 개선(직선 시야 조기반환 등) 전후 비교의 대조군.
//                  현재 CPathFinder는 평지에서도 A*를 그대로 돌리므로,
//                  "장애물이 없으면 얼마나 싼가"를 숫자로 확인할 수 있다.
// ================================================================

constexpr int32_t RAID_INNER_X = 150;   // 내부(잔디) 가로
constexpr int32_t RAID_INNER_Z = 150;   // 내부(잔디) 세로
constexpr int32_t RAID_TILE_COUNT = RAID_INNER_X * RAID_INNER_Z;

// 두 대형 맵의 공통 스폰 지점(내부 좌표 기준). 존 좌표계는 테두리 2+1칸이 앞에 붙으므로
// 실제 월드 좌표는 +3 해서 쓴다(아래 RAID_SPAWN_WORLD_*).
constexpr int32_t RAID_SPAWN_INNER_X = 75;
constexpr int32_t RAID_SPAWN_INNER_Z = 75;

// 월드 좌표 = 내부좌표 + (OUTER 2 + BORDER 1) + 0.5(타일 중심)
constexpr float RAID_SPAWN_WORLD_X = RAID_SPAWN_INNER_X + 3 + 0.5f;
constexpr float RAID_SPAWN_WORLD_Z = RAID_SPAWN_INNER_Z + 3 + 0.5f;

// 스폰 주변 이 반경만큼은 장애물을 두지 않는다(입장하자마자 벽에 끼는 것 방지).
constexpr int32_t RAID_SPAWN_CLEAR_RADIUS = 8;

// 바깥 테두리 안쪽 이 폭만큼은 비워 둔다(가장자리를 따라 항상 돌아갈 수 있게 = 연결성 보험).
constexpr int32_t RAID_EDGE_MARGIN = 3;

constexpr uint32_t RAID_MAP_SEED = 20260714u;   // 이 값이 같아야 클라/서버 맵이 같다
constexpr int32_t  RAID_BLOB_COUNT = 900;       // 장애물 덩어리 개수 (실측 밀도 13.4%)

// ---- 대형 맵 몬스터 설정 (서버 전용. 두 맵이 동일해야 비교가 성립한다) ----
constexpr int32_t RAID_MONSTER_COUNT = 120;   // 존당 몬스터 수 (MAX_MONSTER=500 안에서)

// 길찾기 부하는 두 축으로 만들어진다:
//   경로 길이 → A* 1회당 비용,  몬스터 수 → A* 호출 빈도.
// 대형 맵은 몬스터를 촘촘히 깔아 "호출 빈도" 쪽으로 부하를 만든다. 그래서 어그로를
// 크게 늘릴 필요는 없다(늘리면 한 플레이어에게 수십 마리가 몰려 상황만 지저분해짐).
// 다만 기본값 3타일은 경로가 4칸 이하라 A*가 노드를 거의 안 펼친다 — 그 정도로는
// 장애물 유무 차이가 드러나지 않으므로 적당히만 늘린다.
constexpr float RAID_AGGRO_RANGE = 8.f;       // 해제는 +5 = 13타일 (히스테리시스)

// ----------------------------------------------------------------
//  결정적 난수 (LCG). std::rand는 구현마다 달라서 쓰면 안 된다 —
//  클라와 서버가 다른 수열을 뽑으면 맵이 달라진다.
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
//  존 생성자가 기대하는 배치와 동일하다.
//
//  장애물은 작은 덩어리(1~3칸)를 흩뿌리는 방식이다. 큰 벽을 세우면 맵이
//  두 조각으로 갈려 몬스터가 영영 못 오는 구역이 생길 수 있는데,
//  작은 덩어리 + 가장자리 여백이면 사실상 항상 연결이 유지된다.
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

// 최초 1회만 생성해서 캐시. (함수 지역 static은 C++11부터 스레드 안전하게 초기화된다)
inline const int* GetRaidBlockMap(bool bFlat)
{
    static const std::vector<int> s_obstacle = MakeRaidBlockMap(false);
    static const std::vector<int> s_flat     = MakeRaidBlockMap(true);
    return bFlat ? s_flat.data() : s_obstacle.data();
}
