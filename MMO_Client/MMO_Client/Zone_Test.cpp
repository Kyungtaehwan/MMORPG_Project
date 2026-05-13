#pragma once
#include "pch.h"
#include "Zone_Test.h"
#include "Img_Manager.h"
#include "Object_Manager.h"
#include "NPC_Shop.h"
#include "Portal.h"
#include "Monster_Orc.h"

void CZone_Test::Build()
{
    // 1. 이 존에서 쓸 이미지 로드
    CImg_Manager* pImg = CImg_Manager::Get_Instance();
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/Normal_Grass.png", L"TEST_GRASS");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/Block_Grass.png", L"TEST_BLOCK");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_LT_Grass.png", L"TEST_BORDER_LT");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_RT_Grass.png", L"TEST_BORDER_RT");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_RB_Grass.png", L"TEST_BORDER_RB");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_LB_Grass.png", L"TEST_BORDER_LB");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_LT_Grass.png", L"TEST_BORDER_T");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_RT_Grass.png", L"TEST_BORDER_R");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_RB_Grass.png", L"TEST_BORDER_B");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutlineBlock_LB_Grass.png", L"TEST_BORDER_L");
    pImg->Insert_Png(L"../Resource/Tile/Grassfield/OutBlock_Grass.png", L"TEST_OUTSIDE");

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
        { TILE_GRASS,     L"TEST_GRASS"     },
        { TILE_BLOCK,     L"TEST_BLOCK"     },
        { TILE_BORDER_LT, L"TEST_BORDER_LT" },
        { TILE_BORDER_RT, L"TEST_BORDER_RT" },
        { TILE_BORDER_RB, L"TEST_BORDER_RB" },
        { TILE_BORDER_LB, L"TEST_BORDER_LB" },
        { TILE_BORDER_T,  L"TEST_BORDER_T"  },
        { TILE_BORDER_R,  L"TEST_BORDER_R"  },
        { TILE_BORDER_B,  L"TEST_BORDER_B"  },
        { TILE_BORDER_L,  L"TEST_BORDER_L"  },
        { TILE_OUTSIDE,   L"TEST_OUTSIDE"   },
        });

    // 4. NPC 스폰 정보
    m_vecNpcSpawn.push_back({ 0, NPC_SHOP, 10.f, 10.f });
    //m_vecNpcSpawn.push_back({ 1, NPC_SHOP, 15.f, 12.f });
}

void CZone_Test::Spawn_Objects()
{
    CImg_Manager* pImg = CImg_Manager::Get_Instance();
    pImg->Insert_Png(L"../Resource/NPC/Traders/Trader0_Idle.png", L"TRADER0_IDLE");
    pImg->Insert_Png(L"../Resource/NPC/Traders/Trader0_Talk.png", L"TRADER0_TALK");


    CNPC_Shop* pNPC = new CNPC_Shop;
    pNPC->Set_WorldPos(15.f, 10.f);
    pNPC->Initialize();
    CObject_Manager::Get_Instance()->Add_Object(OBJ_NPC, pNPC);

    CPortal* pPortal = new CPortal;
    pPortal->Set_WorldPos(10.f, 10.f);
    pPortal->Set_TargetZone(ZONE_TOWN, 10.f, 28.f);  // 마을 남쪽 스폰
    pPortal->Initialize();
    CObject_Manager::Get_Instance()->Add_Object(OBJ_PORTAL, pPortal);


}

void CZone_Test::Clear_Objects()
{
    CObject_Manager::Get_Instance()->DeleteID(OBJ_NPC);
}
