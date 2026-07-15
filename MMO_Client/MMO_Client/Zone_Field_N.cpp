#pragma once
#include "pch.h"
#include "Zone_Field_N.h"
#include "Img_Manager.h"
#include "Object_Manager.h"
#include "NPC_Shop.h"
#include "Portal.h"
#include "Monster_Orc.h"

void CZone_Field_N::Build()
{
    // 1. 이 존에서 쓸 이미지 로드
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

    // 2. 타일 배치
    static const int BLOCK_MAP[30][20] =
    {
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,1,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,1,1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,1,1,0,0,0,0, 0,0,0,0,1,1,0,0,0,0 },
        { 0,0,0,0,1,1,0,0,0,0, 0,0,0,0,1,1,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,1,0,0,0,0,0,0, 0,0,0,0,0,0,1,0,0,0 },
        { 0,0,0,1,1,0,0,0,0,0, 0,0,0,0,0,1,1,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,1,1,0, 0,1,1,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,1,1,0,0,0, 0,0,0,1,1,0,0,0,0,0 },
        { 0,1,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,1,0 },
        { 0,1,1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,1,1,0 },
        { 0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0 },
    };

    Build_TileGrid(20, 30, &BLOCK_MAP[0][0]);

    // 3. 타입 → ImgKey 매핑 (이 존 전용 키)
    Apply_ImgKeys({
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
        });

    // NPC는 이제 마을 존에만 존재한다.
}

void CZone_Field_N::Spawn_Objects()
{
    // 북쪽 필드. 마을로 돌아가는 복귀 포탈은 남쪽(아래)에 둔다.
    CPortal* pPortal = new CPortal;
    pPortal->Set_WorldPos(19.f, 29.f);
    pPortal->Set_TargetZone(ZONE_TOWN, 9.f, 9.f);  // 마을 북쪽 입구로 복귀
    pPortal->Initialize();
    CObject_Manager::Get_Instance()->Add_Object(OBJ_PORTAL, pPortal);
}

void CZone_Field_N::Clear_Objects()
{
    CObject_Manager::Get_Instance()->DeleteID(OBJ_NPC);
    CObject_Manager::Get_Instance()->DeleteID(OBJ_PORTAL);
}
