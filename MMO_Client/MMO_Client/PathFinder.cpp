#include "pch.h"
#include "PathFinder.h"
#include <queue>
#include <unordered_map>
#include <cmath>

static const int   DX[8] = { 1,-1, 0, 0, 1,-1, 1,-1 };
static const int   DZ[8] = { 0, 0, 1,-1, 1,-1,-1, 1 };
static const float COST[8] = {
    1.f, 1.f, 1.f, 1.f,
    1.414f, 1.414f, 1.414f, 1.414f
};

float CPathFinder::Heuristic(int32_t x1, int32_t z1,
    int32_t x2, int32_t z2)
{
    float fDX = static_cast<float>(abs(x1 - x2));
    float fDZ = static_cast<float>(abs(z1 - z2));
    return (fDX + fDZ) + (1.414f - 2.f) * std::min<float>(fDX, fDZ);
}

bool CPathFinder::HasLineOfSight(
    float fAX, float fAZ,
    float fBX, float fBZ,
    IsMovableFunc fnIsMovable)
{
    int32_t x0 = static_cast<int32_t>(floorf(fAX));
    int32_t z0 = static_cast<int32_t>(floorf(fAZ));
    int32_t x1 = static_cast<int32_t>(floorf(fBX));
    int32_t z1 = static_cast<int32_t>(floorf(fBZ));

    float fDX = fBX - fAX;
    float fDZ = fBZ - fAZ;

    int32_t stepX = (fDX > 0) ? 1 : -1;
    int32_t stepZ = (fDZ > 0) ? 1 : -1;

    float tMaxX = (fDX != 0) ?
        ((fDX > 0 ? (x0 + 1.f) : x0) - fAX) / fDX : FLT_MAX;
    float tMaxZ = (fDZ != 0) ?
        ((fDZ > 0 ? (z0 + 1.f) : z0) - fAZ) / fDZ : FLT_MAX;

    float tDeltaX = (fDX != 0) ? fabsf(1.f / fDX) : FLT_MAX;
    float tDeltaZ = (fDZ != 0) ? fabsf(1.f / fDZ) : FLT_MAX;

    int32_t curX = x0;
    int32_t curZ = z0;

    while (true)
    {
        if (!fnIsMovable(curX, curZ)) return false;
        if (curX == x1 && curZ == z1) break;

        if (tMaxX < tMaxZ)
        {
            tMaxX += tDeltaX;
            curX += stepX;
        }
        else if (tMaxZ < tMaxX)
        {
            tMaxZ += tDeltaZ;
            curZ += stepZ;
        }
        else
        {
            if (!fnIsMovable(curX + stepX, curZ) ||
                !fnIsMovable(curX, curZ + stepZ))
                return false;
            tMaxX += tDeltaX;
            tMaxZ += tDeltaZ;
            curX += stepX;
            curZ += stepZ;
        }
    }
    return true;
}


std::vector<std::pair<float, float>> CPathFinder::FindPath(
    int32_t nStartX, int32_t nStartZ,
    int32_t nEndX, int32_t nEndZ,
    float   fRealStartX, float   fRealStartZ,
    IsMovableFunc fnIsMovable,
    EPathMode eMode)
{
    if (nStartX == nEndX && nStartZ == nEndZ) return {};
    if (!fnIsMovable(nEndX, nEndZ))           return {};

    switch (eMode)
    {
    case EPathMode::AStar:
    {
        auto rawPath = RunAStar(nStartX, nStartZ, nEndX, nEndZ,
            fRealStartX, fRealStartZ, fnIsMovable);
        if (rawPath.empty()) return {};
        return StringPull_AStar(rawPath, fRealStartX, fRealStartZ, fnIsMovable);
    }
    case EPathMode::ThetaStar:
        return RunThetaStar(nStartX, nStartZ, nEndX, nEndZ,
            fRealStartX, fRealStartZ, fnIsMovable);

    case EPathMode::CornerBased:
    {
        auto rawPath = RunAStar(nStartX, nStartZ, nEndX, nEndZ,
            fRealStartX, fRealStartZ, fnIsMovable);
        if (rawPath.empty()) return {};
        return StringPull_Corner(rawPath, fRealStartX, fRealStartZ, fnIsMovable);
    }
    }
    return {};
}

std::vector<std::pair<int32_t, int32_t>> CPathFinder::RunAStar(
    int32_t nStartX, int32_t nStartZ,
    int32_t nEndX, int32_t nEndZ,
    float   fRealStartX, float   fRealStartZ,
    IsMovableFunc fnIsMovable)
{
    auto Key = [](int32_t x, int32_t z) -> int64_t {
        return (int64_t)z * 10000 + x;
        };

    std::priority_queue<PathNode, std::vector<PathNode>,
        std::greater<PathNode>> openList;
    std::unordered_map<int64_t, PathNode> allNodes;
    std::unordered_map<int64_t, bool>     closedList;

    PathNode startNode;
    startNode.x = nStartX;
    startNode.z = nStartZ;
    startNode.fG = 0.f;
    startNode.fH = Heuristic(nStartX, nStartZ, nEndX, nEndZ);
    startNode.fF = startNode.fH;
    startNode.nParentX = -1;
    startNode.nParentZ = -1;

    openList.push(startNode);
    allNodes[Key(nStartX, nStartZ)] = startNode;

    while (!openList.empty())
    {
        PathNode cur = openList.top();
        openList.pop();

        int64_t curKey = Key(cur.x, cur.z);
        if (closedList[curKey]) continue;
        closedList[curKey] = true;

        if (cur.x == nEndX && cur.z == nEndZ)
        {
            std::vector<std::pair<int32_t, int32_t>> rawPath;
            PathNode* pNode = &allNodes[curKey];
            while (pNode->nParentX != -1)
            {
                rawPath.push_back({ pNode->x, pNode->z });
                pNode = &allNodes[Key(pNode->nParentX, pNode->nParentZ)];
            }
            rawPath.push_back({ nStartX, nStartZ });
            std::reverse(rawPath.begin(), rawPath.end());
            return rawPath;
        }

        for (int i = 0; i < 8; ++i)
        {
            int32_t nx = cur.x + DX[i];
            int32_t nz = cur.z + DZ[i];

            if (!fnIsMovable(nx, nz)) continue;
            if (i >= 4)
            {
                if (!fnIsMovable(cur.x + DX[i], cur.z) ||
                    !fnIsMovable(cur.x, cur.z + DZ[i]))
                    continue;
            }

            int64_t nextKey = Key(nx, nz);
            if (closedList[nextKey]) continue;

            float fNewG = cur.fG + COST[i];
            auto it = allNodes.find(nextKey);
            if (it != allNodes.end() && it->second.fG <= fNewG) continue;

            PathNode next;
            next.x = nx;
            next.z = nz;
            next.fG = fNewG;
            next.fH = Heuristic(nx, nz, nEndX, nEndZ);
            next.fF = next.fG + next.fH;
            next.nParentX = cur.x;
            next.nParentZ = cur.z;

            allNodes[nextKey] = next;
            openList.push(next);
        }
    }
    return {};
}

std::vector<std::pair<float, float>> CPathFinder::RunThetaStar(
    int32_t nStartX, int32_t nStartZ,
    int32_t nEndX, int32_t nEndZ,
    float   fRealStartX, float   fRealStartZ,
    IsMovableFunc fnIsMovable)
{
    auto Key = [](int32_t x, int32_t z) -> int64_t {
        return (int64_t)z * 10000 + x;
        };

    std::priority_queue<PathNode, std::vector<PathNode>,
        std::greater<PathNode>> openList;
    std::unordered_map<int64_t, PathNode> allNodes;
    std::unordered_map<int64_t, bool>     closedList;

    PathNode startNode;
    startNode.x = nStartX;
    startNode.z = nStartZ;
    startNode.fG = 0.f;
    startNode.fH = Heuristic(nStartX, nStartZ, nEndX, nEndZ);
    startNode.fF = startNode.fH;
    startNode.nParentX = -1;
    startNode.nParentZ = -1;
    startNode.fWorldX = fRealStartX;
    startNode.fWorldZ = fRealStartZ;

    openList.push(startNode);
    allNodes[Key(nStartX, nStartZ)] = startNode;

    while (!openList.empty())
    {
        PathNode cur = openList.top();
        openList.pop();

        int64_t curKey = Key(cur.x, cur.z);
        if (closedList[curKey]) continue;
        closedList[curKey] = true;

        if (cur.x == nEndX && cur.z == nEndZ)
        {
            std::vector<std::pair<float, float>> result;
            PathNode* pNode = &allNodes[curKey];
            while (pNode->nParentX != -1)
            {
                result.push_back({ pNode->fWorldX, pNode->fWorldZ });
                pNode = &allNodes[Key(pNode->nParentX, pNode->nParentZ)];
            }
            result.push_back({ fRealStartX, fRealStartZ });
            std::reverse(result.begin(), result.end());
            return result;
        }

        for (int i = 0; i < 8; ++i)
        {
            int32_t nx = cur.x + DX[i];
            int32_t nz = cur.z + DZ[i];

            if (!fnIsMovable(nx, nz)) continue;
            if (i >= 4)
            {
                if (!fnIsMovable(cur.x + DX[i], cur.z) ||
                    !fnIsMovable(cur.x, cur.z + DZ[i]))
                    continue;
            }

            int64_t nextKey = Key(nx, nz);
            if (closedList[nextKey]) continue;

            float fNextWorldX = nx + 0.5f;
            float fNextWorldZ = nz + 0.5f;

            float   fNewG;
            int32_t bestParentX, bestParentZ;
            float   bestWorldX, bestWorldZ;

            int32_t pX = cur.nParentX;
            int32_t pZ = cur.nParentZ;

            if (pX != -1)
            {
                PathNode& pNode = allNodes[Key(pX, pZ)];
                if (HasLineOfSight(pNode.fWorldX, pNode.fWorldZ,
                    fNextWorldX, fNextWorldZ,
                    fnIsMovable))
                {
                    float fDX = fNextWorldX - pNode.fWorldX;
                    float fDZ = fNextWorldZ - pNode.fWorldZ;
                    fNewG = pNode.fG + sqrtf(fDX * fDX + fDZ * fDZ);
                    bestParentX = pX;
                    bestParentZ = pZ;
                    bestWorldX = pNode.fWorldX;
                    bestWorldZ = pNode.fWorldZ;
                }
                else
                {
                    float fDX = fNextWorldX - cur.fWorldX;
                    float fDZ = fNextWorldZ - cur.fWorldZ;
                    fNewG = cur.fG + sqrtf(fDX * fDX + fDZ * fDZ);
                    bestParentX = cur.x;
                    bestParentZ = cur.z;
                    bestWorldX = cur.fWorldX;
                    bestWorldZ = cur.fWorldZ;
                }
            }
            else
            {
                float fDX = fNextWorldX - fRealStartX;
                float fDZ = fNextWorldZ - fRealStartZ;
                fNewG = sqrtf(fDX * fDX + fDZ * fDZ);
                bestParentX = cur.x;
                bestParentZ = cur.z;
                bestWorldX = cur.fWorldX;
                bestWorldZ = cur.fWorldZ;
            }

            auto it = allNodes.find(nextKey);
            if (it != allNodes.end() && it->second.fG <= fNewG) continue;

            PathNode next;
            next.x = nx;
            next.z = nz;
            next.fG = fNewG;
            next.fH = Heuristic(nx, nz, nEndX, nEndZ);
            next.fF = next.fG + next.fH;
            next.nParentX = bestParentX;
            next.nParentZ = bestParentZ;
            next.fWorldX = fNextWorldX;
            next.fWorldZ = fNextWorldZ;

            allNodes[nextKey] = next;
            openList.push(next);
        }
    }
    return {};
}


//  A* 후처리 1 - 타일 중심 String Pulling

std::vector<std::pair<float, float>> CPathFinder::StringPull_AStar(
    const std::vector<std::pair<int32_t, int32_t>>& rawPath,
    float fRealStartX, float fRealStartZ,
    IsMovableFunc fnIsMovable)
{
    if (rawPath.empty()) return {};

    std::vector<std::pair<float, float>> floatPath;
    floatPath.push_back({ fRealStartX, fRealStartZ });
    for (int32_t i = 1; i < (int32_t)rawPath.size(); ++i)
        floatPath.push_back({ rawPath[i].first + 0.5f, rawPath[i].second + 0.5f });

    std::vector<std::pair<float, float>> result;
    result.push_back(floatPath[0]);

    int32_t nFrom = 0;
    while (nFrom < (int32_t)floatPath.size() - 1)
    {
        int32_t nFarthest = nFrom + 1;
        for (int32_t i = (int32_t)floatPath.size() - 1; i > nFrom + 1; --i)
        {
            if (HasLineOfSight(
                floatPath[nFrom].first, floatPath[nFrom].second,
                floatPath[i].first, floatPath[i].second,
                fnIsMovable))
            {
                nFarthest = i;
                break;
            }
        }
        result.push_back(floatPath[nFarthest]);
        nFrom = nFarthest;
    }
    return result;
}


//  A* 후처리 2 - 장애물 모서리 String Pulling
std::pair<float, float> CPathFinder::FindCornerPoint(
    const std::pair<float, float>& from,
    const std::pair<float, float>& to,
    IsMovableFunc fnIsMovable)
{
    float fDX = to.first - from.first;
    float fDZ = to.second - from.second;
    float fDist = sqrtf(fDX * fDX + fDZ * fDZ);
    if (fDist < 0.001f) return to;

    float fNX = fDX / fDist;
    float fNZ = fDZ / fDist;

    float fStep = 0.1f;
    float fT = 0.f;
    int32_t lastBlockX = -1, lastBlockZ = -1;

    while (fT < fDist)
    {
        float fx = from.first + fNX * fT;
        float fz = from.second + fNZ * fT;
        int32_t tx = static_cast<int32_t>(floorf(fx));
        int32_t tz = static_cast<int32_t>(floorf(fz));

        if (!fnIsMovable(tx, tz))
        {
            lastBlockX = tx;
            lastBlockZ = tz;
            break;
        }
        fT += fStep;
    }

    if (lastBlockX == -1) return to;

    constexpr float MARGIN = 0.05f;
    std::pair<float, float> corners[4] = {
        { lastBlockX - MARGIN,       lastBlockZ - MARGIN       },
        { lastBlockX + 1.f + MARGIN, lastBlockZ - MARGIN       },
        { lastBlockX - MARGIN,       lastBlockZ + 1.f + MARGIN },
        { lastBlockX + 1.f + MARGIN, lastBlockZ + 1.f + MARGIN }
    };

    float fBestDist = FLT_MAX;
    std::pair<float, float> bestCorner = to;

    for (auto& corner : corners)
    {
        int32_t cx = static_cast<int32_t>(floorf(corner.first));
        int32_t cz = static_cast<int32_t>(floorf(corner.second));
        if (!fnIsMovable(cx, cz)) continue;

        if (!HasLineOfSight(from.first, from.second,
            corner.first, corner.second, fnIsMovable))
            continue;

        float fdx = corner.first - from.first;
        float fdz = corner.second - from.second;
        float fCornerDist = sqrtf(fdx * fdx + fdz * fdz);

        if (fCornerDist < fBestDist)
        {
            fBestDist = fCornerDist;
            bestCorner = corner;
        }
    }
    return bestCorner;
}

std::vector<std::pair<float, float>> CPathFinder::StringPull_Corner(
    const std::vector<std::pair<int32_t, int32_t>>& rawPath,
    float fRealStartX, float fRealStartZ,
    IsMovableFunc fnIsMovable)
{
    if (rawPath.empty()) return {};

    std::vector<std::pair<float, float>> floatPath;
    floatPath.push_back({ fRealStartX, fRealStartZ });
    for (int32_t i = 1; i < (int32_t)rawPath.size(); ++i)
        floatPath.push_back({ rawPath[i].first + 0.5f, rawPath[i].second + 0.5f });

    std::vector<std::pair<float, float>> result;
    result.push_back(floatPath[0]);

    int32_t nFrom = 0;
    while (nFrom < (int32_t)floatPath.size() - 1)
    {
        int32_t nFarthest = nFrom + 1;
        for (int32_t i = (int32_t)floatPath.size() - 1; i > nFrom + 1; --i)
        {
            if (HasLineOfSight(
                floatPath[nFrom].first, floatPath[nFrom].second,
                floatPath[i].first, floatPath[i].second,
                fnIsMovable))
            {
                nFarthest = i;
                break;
            }
        }

        if (nFarthest == nFrom + 1)
        {
            // 직선 불가 → 모서리 경유
            auto corner = FindCornerPoint(
                floatPath[nFrom],
                floatPath[nFrom + 1],
                fnIsMovable);
            result.push_back(corner);
        }
        else
        {
            result.push_back(floatPath[nFarthest]);
        }

        nFrom = nFarthest;
    }
    return result;
}