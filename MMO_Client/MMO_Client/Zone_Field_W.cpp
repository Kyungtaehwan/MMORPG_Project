#include "pch.h"
#include "Zone_Field_W.h"
#include "Img_Manager.h"
#include "Object_Manager.h"
#include "Portal.h"

void CZone_Field_W::Build()
{
    // 잔디 타일셋 (테스트 필드와 동일 키 재사용)
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

    // 세로 막대 2개 (서버 BLOCK_MAP_FIELD_W와 반드시 동일)
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
        { 0,0,0,0,0,0,0,1,0,0, 0,0,1,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,1,0,0, 0,0,1,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,1,0,0, 0,0,1,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,1,0,0, 0,0,1,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,1,0,0, 0,0,1,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,1,0,0, 0,0,1,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,1,0,0, 0,0,1,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,1,0,0, 0,0,1,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,1,0,0, 0,0,1,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,1,0,0, 0,0,1,0,0,0,0,0,0,0 },
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
    };

    Build_TileGrid(20, 30, &BLOCK_MAP[0][0]);

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
}

void CZone_Field_W::Spawn_Objects()
{
    // 마을로 돌아가는 복귀 포탈은 동쪽(오른쪽)에 둔다.
    CPortal* pPortal = new CPortal;
    pPortal->Set_WorldPos(19.f, 6.f);
    pPortal->Set_TargetZone(ZONE_TOWN, 9.f, 26.f);  // 마을 남서(서쪽 필드) 입구로 복귀
    pPortal->Initialize();
    CObject_Manager::Get_Instance()->Add_Object(OBJ_PORTAL, pPortal);
}

void CZone_Field_W::Clear_Objects()
{
    CObject_Manager::Get_Instance()->DeleteID(OBJ_NPC);
    CObject_Manager::Get_Instance()->DeleteID(OBJ_PORTAL);
}
