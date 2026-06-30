#include "pch.h"
#include "Zone_Town.h"
#include "Img_Manager.h"
#include "Object_Manager.h"
#include "NPC_Shop.h"
#include "Portal.h"

void CZone_Town::Build()
{
    // 마을은 같은 타일셋이지만 키를 분리해서 관리
    // 추후 마을 전용 타일로 교체 시 이미지 경로만 바꾸면 됨
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

    // 마을: 맵 테두리만 유지하고 가운데 장애물은 전부 제거 (전부 잔디).
    // 서버 BLOCK_MAP_TOWN(전부 0)과 일치해야 한다.
    static const int BLOCK_MAP[30][20] = { 0 };

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

void CZone_Town::Spawn_Objects()
{
    // NPC는 마을 존에만 존재한다.
    CImg_Manager* pImg = CImg_Manager::Get_Instance();
    pImg->Insert_Png(L"../Resource/NPC/Traders/Trader0_Idle.png", L"TRADER0_IDLE");
    pImg->Insert_Png(L"../Resource/NPC/Traders/Trader0_Talk.png", L"TRADER0_TALK");

    CNPC_Shop* pNPC = new CNPC_Shop;
    pNPC->Set_WorldPos(12.f, 17.f);  // 마을 중앙
    pNPC->Initialize();
    CObject_Manager::Get_Instance()->Add_Object(OBJ_NPC, pNPC);

    // 동서남북 4방향 필드로 가는 포탈.
    // 각 포탈은 마을의 해당 방향 모서리에 두고, 도착 지점은
    // 그 필드의 복귀 포탈(반대편) 근처로 잡는다.
    auto AddPortal = [](float fX, float fZ, ZONE_ID eTarget,
        float fSpawnX, float fSpawnZ)
    {
        CPortal* pPortal = new CPortal;
        pPortal->Set_WorldPos(fX, fZ);
        pPortal->Set_TargetZone(eTarget, fSpawnX, fSpawnZ);
        pPortal->Initialize();
        CObject_Manager::Get_Instance()->Add_Object(OBJ_PORTAL, pPortal);
    };

    AddPortal(6.f, 6.f, ZONE_TEST, 16.f, 26.f);     // 북쪽 → 북쪽 필드 남쪽 입구
    AddPortal(19.f, 6.f, ZONE_FIELD_E, 9.f, 26.f);  // 동쪽 → 동쪽 필드 서쪽 입구
    AddPortal(19.f, 29.f, ZONE_FIELD_S, 9.f, 9.f);  // 남쪽 → 남쪽 필드 북쪽 입구
    AddPortal(6.f, 29.f, ZONE_FIELD_W, 16.f, 9.f);  // 서쪽 → 서쪽 필드 동쪽 입구
}

void CZone_Town::Clear_Objects()
{
    CObject_Manager::Get_Instance()->DeleteID(OBJ_NPC);
    CObject_Manager::Get_Instance()->DeleteID(OBJ_PORTAL);
}
