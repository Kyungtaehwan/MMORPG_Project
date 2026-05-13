#pragma once
#include <vector>
#include <functional>
#include <cstdint>

using IsMovableFunc = std::function<bool(int32_t, int32_t)>;

// 길찾기 알고리즘 선택
enum class EPathMode
{
    AStar,        // 빠름, 타일 중심 경유
    ThetaStar,    // 중간, Any-Angle (부모 건너뛰기)
    CornerBased,  // 느림, 장애물 모서리 기반 최적 경로
};

struct PathNode
{
    int32_t x, z;
    float   fG = 0.f;
    float   fH = 0.f;
    float   fF = 0.f;
    int32_t nParentX = -1;
    int32_t nParentZ = -1;
    float   fWorldX = 0.f;
    float   fWorldZ = 0.f;

    bool operator>(const PathNode& other) const
    {
        return fF > other.fF;
    }
};

class CPathFinder
{
public:
    // 통합 진입점
    static std::vector<std::pair<float, float>> FindPath(
        int32_t nStartX, int32_t nStartZ,
        int32_t nEndX, int32_t nEndZ,
        float   fRealStartX, float   fRealStartZ,
        IsMovableFunc fnIsMovable,
        EPathMode eMode = EPathMode::AStar);

    static bool HasLineOfSight(
        float fAX, float fAZ,
        float fBX, float fBZ,
        IsMovableFunc fnIsMovable);

private:
    // 공통 A* 탐색 (raw 타일 경로 반환)
    static std::vector<std::pair<int32_t, int32_t>> RunAStar(
        int32_t nStartX, int32_t nStartZ,
        int32_t nEndX, int32_t nEndZ,
        float   fRealStartX, float   fRealStartZ,
        IsMovableFunc fnIsMovable);

    // Theta* 탐색 (float 경로 직접 반환)
    static std::vector<std::pair<float, float>> RunThetaStar(
        int32_t nStartX, int32_t nStartZ,
        int32_t nEndX, int32_t nEndZ,
        float   fRealStartX, float   fRealStartZ,
        IsMovableFunc fnIsMovable);

    // A* 후처리 - 타일 중심 String Pulling
    static std::vector<std::pair<float, float>> StringPull_AStar(
        const std::vector<std::pair<int32_t, int32_t>>& rawPath,
        float fRealStartX, float fRealStartZ,
        IsMovableFunc fnIsMovable);

    // A* 후처리 - 모서리 기반 String Pulling
    static std::vector<std::pair<float, float>> StringPull_Corner(
        const std::vector<std::pair<int32_t, int32_t>>& rawPath,
        float fRealStartX, float fRealStartZ,
        IsMovableFunc fnIsMovable);

    // 장애물 모서리 탐색
    static std::pair<float, float> FindCornerPoint(
        const std::pair<float, float>& from,
        const std::pair<float, float>& to,
        IsMovableFunc fnIsMovable);

    static float Heuristic(int32_t x1, int32_t z1,
        int32_t x2, int32_t z2);
};