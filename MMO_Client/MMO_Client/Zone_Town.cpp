#include "pch.h"
#include "Zone_Town.h"
#include "Img_Manager.h"
#include "Object_Manager.h"
#include "NPC_Shop.h"
#include "NPC_Market.h"
#include "NPC_OldMan.h"
#include "NPC_Knight.h"
#include "NPC_Angel.h"
#include "StaticObject.h"
#include "BigTent.h"
#include "SmallTent.h"
#include "Smithy.h"
#include "TreeGreen.h"
#include "TreeBare.h"
#include "WagonDrink.h"
#include "WagonFood.h"
#include "WagonShop.h"
#include "WagonEtc.h"
#include "WoodWagon.h"
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
    static const int BLOCK_MAP[30][30] = { 0 };

    Build_TileGrid(30, 30, &BLOCK_MAP[0][0]);

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

    // ── 오브젝트가 실제 가리는 칸 블락 (이동 차단). 렌더는 잔디 유지(오브젝트가 덮음). ──
    //    서버 Zone_Manager 의 townBlock 목록과 반드시 동일해야 함.
    static const int s_block[][2] = {
        { 7,21},{ 7,20},{ 7,19},{ 7,18},{ 7,17},{ 6,19},{ 6,18},{ 6,17},
        { 7,15},{ 7,14},{ 7,13},{ 8,15},{ 8,14},{ 8,13},{ 8,12},
        {12,10},{13,10},{14,10},{12, 9},{13, 9},
        {17,18},{17,17},
        {13,22},{14,22},{15,22},
        {14, 3},{15, 3},{16, 3},{14, 4},{15, 4},{16, 4},{14, 5},{15, 5},{16, 5},
    };
    for (auto& b : s_block)
        Set_TileType(b[0], b[1], TILE_BLOCK);
    // 병사 큰 텐트 3개 영역 (x 18~27, z 3~6)
    for (int x = 18; x <= 27; ++x)
        for (int z = 3; z <= 6; ++z)
            Set_TileType(x, z, TILE_BLOCK);
}

void CZone_Town::Spawn_Objects()
{
    // NPC는 마을 존에만 존재한다.
    CImg_Manager* pImg = CImg_Manager::Get_Instance();
    pImg->Insert_Png(L"../Resource/NPC/Traders/Trader0_Idle.png", L"TRADER0_IDLE");
    pImg->Insert_Png(L"../Resource/NPC/Traders/Trader0_Talk.png", L"TRADER0_TALK");

    CNPC_Shop* pNPC = new CNPC_Shop;
    pNPC->Set_WorldPos(10.f, 15.f);  // 상인: 경매장 오른쪽 + 수레 상점 앞
    pNPC->Initialize();
    CObject_Manager::Get_Instance()->Add_Object(OBJ_NPC, pNPC);

    // 경매장 NPC (상점 옆). Market.png 단일 정적 이미지.
    pImg->Insert_Png(L"../Resource/NPC/Market/Market.png", L"MARKET_IDLE");

    CNPC_Market* pMarket = new CNPC_Market;
    pMarket->Set_WorldPos(9.f, 21.f);  // 경매장(시장 안)
    pMarket->Initialize();
    CObject_Manager::Get_Instance()->Add_Object(OBJ_NPC, pMarket);

    // 장식/대화 NPC (움직이지 않음). 클릭 시 모션/말풍선만.
    pImg->Insert_Png(L"../Resource/NPC/OldMan/OldMan_Idle.png", L"OLDMAN_IDLE");
    pImg->Insert_Png(L"../Resource/NPC/Knight/Knight_Idle.png", L"KNIGHT_IDLE");
    pImg->Insert_Png(L"../Resource/NPC/Knight/Knight_Talk.png", L"KNIGHT_TALK");
    pImg->Insert_Png(L"../Resource/NPC/Angel/Angel_Idle.png", L"ANGEL_IDLE");
    pImg->Insert_Png(L"../Resource/NPC/Angel/Angel_Effect.png", L"ANGEL_EFFECT");

    CNPC_OldMan* pOldMan = new CNPC_OldMan;
    pOldMan->Set_WorldPos(23.f, 20.f);  // 중심 우하단 빈 공간(천사와 함께)
    pOldMan->Initialize();
    CObject_Manager::Get_Instance()->Add_Object(OBJ_NPC, pOldMan);

    CNPC_Knight* pKnight = new CNPC_Knight;
    pKnight->Set_WorldPos(16.f, 8.f);  // 작은 텐트(지휘관) 앞
    pKnight->Initialize();
    CObject_Manager::Get_Instance()->Add_Object(OBJ_NPC, pKnight);

    CNPC_Angel* pAngel = new CNPC_Angel;
    pAngel->Set_WorldPos(25.f, 22.f);  // 중심 우하단 빈 공간(노인과 함께)
    pAngel->Initialize();
    CObject_Manager::Get_Instance()->Add_Object(OBJ_NPC, pAngel);

    // ── 마을 장식/구조물(정적 오브젝트) 배치 : 클라 시각 전용, 블락 타일은 아직 없음 ──
    // 각 오브젝트는 CStaticObject 상속 개별 클래스. 나무는 통과 가능(블락 X).
    auto Add = [](CStaticObject* p, float x, float z)
    {
        p->Set_WorldPos(x, z);
        p->Initialize();
        CObject_Manager::Get_Instance()->Add_Object(OBJ_OBJECT, p);
    };

    // 병사 큰 텐트 3개(우측 줄지어) + 지휘관(작은) 텐트
    Add(new CSmallTent, 16.f,  5.f);
    Add(new CBigTent,   21.f,  6.f);
    Add(new CBigTent,   24.f, 6.f);
    Add(new CBigTent,   27.f, 6.f);
    // 시장(중앙 좌상): 수레들 + 대장간
    Add(new CWagonDrink, 8.f, 20.f);
    Add(new CWagonShop, 8.f, 14.f);
    Add(new CWagonFood, 18.f, 18.f);
    Add(new CSmithy,    14.f, 11.f);
    // 중앙 하단 수레 2개
    Add(new CWoodWagon, 27.f, 6.f);
    Add(new CWagonEtc,  15.f, 23.f);
    // 나무 (통과 가능, 임의 배치)
    Add(new CTreeGreen,  5.f, 10.f);
    Add(new CTreeBare,   5.f, 22.f);
    Add(new CTreeGreen,  9.f, 27.f);
    Add(new CTreeBare,  15.f, 28.f);
    Add(new CTreeGreen, 21.f, 27.f);
    Add(new CTreeBare,  26.f, 24.f);
    Add(new CTreeGreen, 28.f, 10.f);
    Add(new CTreeBare,  12.f,  5.f);
    Add(new CTreeGreen, 18.f,  4.f);
    Add(new CTreeBare,   8.f, 25.f);

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

    // 정사각형 마을(30x30)의 네 모서리에 배치
    AddPortal(3.f,  3.f,  ZONE_TEST,    16.f, 26.f);  // 북서 → 북쪽 필드
    AddPortal(32.f, 3.f,  ZONE_FIELD_E,  9.f, 26.f);  // 북동 → 동쪽 필드
    AddPortal(32.f, 32.f, ZONE_FIELD_S,  9.f,  9.f);  // 남동 → 남쪽 필드
    AddPortal(3.f,  32.f, ZONE_FIELD_W, 16.f,  9.f);  // 남서 → 서쪽 필드
}

void CZone_Town::Clear_Objects()
{
    CObject_Manager::Get_Instance()->DeleteID(OBJ_NPC);
    CObject_Manager::Get_Instance()->DeleteID(OBJ_PORTAL);
    CObject_Manager::Get_Instance()->DeleteID(OBJ_OBJECT);
}
