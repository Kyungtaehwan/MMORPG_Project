#include "pch.h"
#include "Zone_Raid.h"
#include "RaidMap.h"       // 블록맵 생성 (서버와 동일 복제본)
#include "Img_Manager.h"
#include "Object_Manager.h"
#include "Portal.h"

// 타일셋은 다른 필드들과 같은 잔디셋(GRASS_* 키)을 그대로 쓴다.
static void Load_GrassTiles()
{
    CImg_Manager* pImg = CImg_Manager::Get_Instance();
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/Normal_Grass.png", L"GRASS_GRASS");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/Block_Grass.png", L"GRASS_BLOCK");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_LT_Grass.png", L"GRASS_BORDER_LT");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_RT_Grass.png", L"GRASS_BORDER_RT");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_RB_Grass.png", L"GRASS_BORDER_RB");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_LB_Grass.png", L"GRASS_BORDER_LB");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_LT_Grass.png", L"GRASS_BORDER_T");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_RT_Grass.png", L"GRASS_BORDER_R");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_RB_Grass.png", L"GRASS_BORDER_B");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_LB_Grass.png", L"GRASS_BORDER_L");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutBlock_Grass.png", L"GRASS_OUTSIDE");
}

static const std::unordered_map<TILE_TYPE, const TCHAR*> s_GrassKeys = {
    { TILE_GRASS,     L"GRASS_GRASS"     },
    { TILE_BLOCK,     L"GRASS_BLOCK"     },
    { TILE_BORDER_LT, L"GRASS_BORDER_LT" },
    { TILE_BORDER_RT, L"GRASS_BORDER_RT" },
    { TILE_BORDER_RB, L"GRASS_BORDER_RB" },
    { TILE_BORDER_LB, L"GRASS_BORDER_LB" },
    { TILE_BORDER_T,  L"GRASS_BORDER_T"  },
    { TILE_BORDER_R,  L"GRASS_BORDER_R"  },
    { TILE_BORDER_B,  L"GRASS_BORDER_B"  },
    { TILE_BORDER_L,  L"GRASS_BORDER_L"  },
    { TILE_OUTSIDE,   L"GRASS_OUTSIDE"   },
};

// 스폰 옆 마을 복귀 포탈 (두 존 공통)
static void Spawn_ReturnPortal()
{
    CPortal* pPortal = new CPortal;
    pPortal->Set_WorldPos(RAID_SPAWN_WORLD_X + 2.f, RAID_SPAWN_WORLD_Z + 2.f);
    pPortal->Set_TargetZone(ZONE_TOWN, 25.f, 24.f);   // 마을 천사 NPC 근처로 복귀
    pPortal->Initialize();
    CObject_Manager::Get_Instance()->Add_Object(OBJ_PORTAL, pPortal);
}

// ----------------------------------------------------------------
//  장애물 있는 대형 맵 (A* 길찾기 부하)
// ----------------------------------------------------------------
void CZone_Raid::Build()
{
    Load_GrassTiles();
    // 서버 Zone_Manager 도 GetRaidBlockMap(false) 를 호출한다 — 두 맵이 반드시 같다.
    Build_TileGrid(RAID_INNER_X, RAID_INNER_Z, GetRaidBlockMap(false));
    Apply_ImgKeys(s_GrassKeys);
}

void CZone_Raid::Spawn_Objects() { Spawn_ReturnPortal(); }

void CZone_Raid::Clear_Objects()
{
    CObject_Manager::Get_Instance()->DeleteID(OBJ_NPC);
    CObject_Manager::Get_Instance()->DeleteID(OBJ_PORTAL);
}

// ----------------------------------------------------------------
//  평지 대형 맵 (대조군) — 장애물만 없고 나머지는 동일
// ----------------------------------------------------------------
void CZone_Raid_Flat::Build()
{
    Load_GrassTiles();
    Build_TileGrid(RAID_INNER_X, RAID_INNER_Z, GetRaidBlockMap(true));
    Apply_ImgKeys(s_GrassKeys);
}

void CZone_Raid_Flat::Spawn_Objects() { Spawn_ReturnPortal(); }

void CZone_Raid_Flat::Clear_Objects()
{
    CObject_Manager::Get_Instance()->DeleteID(OBJ_NPC);
    CObject_Manager::Get_Instance()->DeleteID(OBJ_PORTAL);
}
