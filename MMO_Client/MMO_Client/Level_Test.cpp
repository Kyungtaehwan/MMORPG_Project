#include "pch.h"
#include "Level_Test.h"
#include "Img_Manager.h"
#include "Object_Manager.h"
#include "Network_Manager.h"
#include "Player.h"
#include "NPC_Shop.h"
#include "Map_Manager.h"
#include "Camera.h"
#include "Input_Manager.h"
#include "UI_Manager.h"
#include "UI_Inventory.h"
#include "UI_QuickSlot.h"
#include "UI_HUD.h"
#include "UI_Shop.h"
#include "UI_Auction.h"


CLevel_Test::CLevel_Test()
{
}

CLevel_Test::~CLevel_Test()
{
	Release();
}

void CLevel_Test::Initialize()
{
	ShowCursor(FALSE);

	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/background/Background1.png", L"BACKGROUND");

	CMap_Manager::Get_Instance()->Initialize();
	Ready_Item_Resource();
	Ready_Player();

}

int CLevel_Test::Update(float dt)
{
	CMap_Manager::Get_Instance()->Update();
	CCamera::Get_Instance()->Update(dt);
	CObject_Manager::Get_Instance()->Update(dt);
	CUI_Manager::Get_Instance()->Update(dt);

	return OBJ_NOEVENT;
}

void CLevel_Test::Late_Update(float dt)
{
	CObject_Manager::Get_Instance()->Late_Update(dt);
	CUI_Manager::Get_Instance()->Late_Update(dt);
}

void CLevel_Test::Render(ID2D1RenderTarget* pRT)
{

	if (CMap_Manager::Get_Instance()->Is_Loading())
	{
		Render_Loading(pRT);
		return;
	}

	ID2D1Bitmap* pBG = CImg_Manager::Get_Instance()->Find_Png(L"BACKGROUND");
	if (pBG)
	{
		pRT->DrawBitmap(
			pBG,
			D2D1::RectF(0.f, 0.f, WINCX, WINCY)
		);
	}

	CMap_Manager::Get_Instance()->Render(pRT);
	CObject_Manager::Get_Instance()->Render(pRT);
	CUI_Manager::Get_Instance()->Render(pRT);
	CInput_Manager::Get_Instance()->Render_Cursor(pRT);

}

void CLevel_Test::Render_Loading(ID2D1RenderTarget* pRT)
{
	pRT->Clear(D2D1::ColorF(0.f, 0.f, 0.f));

	float fProgress = CMap_Manager::Get_Instance()->Get_LoadProgress();
	float barW = 400.f, barH = 20.f;
	float barX = (WINCX - barW) / 2.f;
	float barY = WINCY / 2.f;

	ID2D1SolidColorBrush* pBrush = nullptr;

	// 배경 바
	pRT->CreateSolidColorBrush(D2D1::ColorF(0.3f, 0.3f, 0.3f), &pBrush);
	pRT->FillRectangle(D2D1::RectF(barX, barY, barX + barW, barY + barH), pBrush);
	pBrush->Release();

	// 진행 바
	pRT->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.8f, 0.2f), &pBrush);
	pRT->FillRectangle(D2D1::RectF(barX, barY, barX + barW * fProgress, barY + barH), pBrush);
	pBrush->Release();
}

void CLevel_Test::Release(void)
{
}

void CLevel_Test::Ready_Player()
{
	float fX = 5.f, fZ = 5.f;  // 기본값

	CNetwork_Manager* pNet = CNetwork_Manager::Get_Instance();
	if (pNet->IsSpawnReady())
	{
		fX = pNet->GetSpawnX();
		fZ = pNet->GetSpawnZ();
		pNet->ClearSpawn();
	}

	CPlayer* pPlayer = new CPlayer;
	pPlayer->Initialize();              // Initialize가 위치를 (10,10)으로 하드코딩하므로
	pPlayer->Set_WorldPos(fX, fZ);      // 반드시 그 뒤에 서버 스폰 위치를 적용해야 함

	CObject_Manager::Get_Instance()->Add_Object(OBJ_PLAYER, pPlayer);
	CCamera::Get_Instance()->Set_Target(pPlayer);


	CUI_HUD* pHUD = new CUI_HUD;
	pHUD->Set_Player(pPlayer);
	pHUD->Initialize();
	CUI_Manager::Get_Instance()->Add_UI(UI_HUD, pHUD);


	CUI_Inventory* pInvenUI = new CUI_Inventory;
	pInvenUI->Set_References(pPlayer, pPlayer->Get_Inventory(), pPlayer->Get_Equipment());
	pInvenUI->Initialize();
	CUI_Manager::Get_Instance()->Add_UI(UI_INVENTORY, pInvenUI);

	CUI_QuickSlot* pQuick = new CUI_QuickSlot;
	pQuick->Set_Player(pPlayer);
	pQuick->Initialize();
	CUI_Manager::Get_Instance()->Add_UI(UI_QUICKSLOT, pQuick);

	CUI_Shop* pShop = new CUI_Shop;
	pShop->Set_References(pPlayer, pPlayer->Get_Inventory());
	pShop->Initialize();
	CUI_Manager::Get_Instance()->Add_UI(UI_SHOP, pShop);

	CUI_Auction* pAuction = new CUI_Auction;
	pAuction->Set_References(pPlayer, pPlayer->Get_Inventory());
	pAuction->Initialize();
	CUI_Manager::Get_Instance()->Add_UI(UI_AUCTION, pAuction);
}

void CLevel_Test::Ready_Item_Resource()
{
#pragma region Equipment
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Armor/Armor_0.png", L"Armor_0");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Armor/Armor_1.png", L"Armor_1");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Armor/Armor_2.png", L"Armor_2");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Armor/Armor_3.png", L"Armor_3");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Armor/Armor_4.png", L"Armor_4");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Armor/Armor_5.png", L"Armor_5");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Armor/Armor_6.png", L"Armor_6");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Armor/Armor_7.png", L"Armor_7");
#pragma endregion Armor

#pragma region Equipment
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Helmet/Helmet_0.png", L"Helmet_0");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Helmet/Helmet_1.png", L"Helmet_1");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Helmet/Helmet_2.png", L"Helmet_2");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Helmet/Helmet_3.png", L"Helmet_3");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Helmet/Helmet_4.png", L"Helmet_4");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Helmet/Helmet_5.png", L"Helmet_5");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Helmet/Helmet_6.png", L"Helmet_6");
#pragma endregion Helmet

#pragma region Equipment
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Pendant/Pendant_0.png", L"Pendant_0");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Pendant/Pendant_1.png", L"Pendant_1");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Pendant/Pendant_2.png", L"Pendant_2");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Pendant/Pendant_3.png", L"Pendant_3");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Pendant/Pendant_4.png", L"Pendant_4");
#pragma endregion Pendant

#pragma region Equipment

	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Ring/Ring_0.png", L"Ring_0");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Ring/Ring_1.png", L"Ring_1");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Ring/Ring_2.png", L"Ring_2");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Ring/Ring_3.png", L"Ring_3");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Ring/Ring_4.png", L"Ring_4");

#pragma endregion Ring

#pragma region Equipment

	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Shield/shield_0.png", L"shield_0");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Shield/shield_1.png", L"shield_1");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Shield/shield_2.png", L"shield_2");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Shield/shield_3.png", L"shield_3");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Shield/shield_4.png", L"shield_4");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Shield/shield_5.png", L"shield_5");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Shield/shield_6.png", L"shield_6");

#pragma endregion Shield

#pragma region Equipment
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Sword/Sword_0.png", L"Sword_0");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Sword/Sword_1.png", L"Sword_1");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Sword/Sword_2.png", L"Sword_2");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Sword/Sword_3.png", L"Sword_3");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Sword/Sword_4.png", L"Sword_4");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Sword/Sword_5.png", L"Sword_5");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Equipment/Sword/Sword_6.png", L"Sword_6");
#pragma endregion Sword

#pragma region Use
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Use/Potion_Atk.png", L"Potion_Atk");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Use/Potion_Invincible.png", L"Potion_Invincible");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Use/Potion_lHP.png", L"Potion_lHP");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Use/Potion_lMp.png", L"Potion_lMp");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Use/Potion_mHP.png", L"Potion_mHP");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Use/Potion_mMp.png", L"Potion_mMp");
#pragma endregion Potion

#pragma region Use
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Use/Scroll_0.png", L"Scroll_0");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Use/Scroll_1.png", L"Scroll_1");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/Use/Scroll_2.png", L"Scroll_2");
#pragma endregion Scroll

#pragma region Item

	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/etc/Gold.png", L"Gold");

	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/etc/BloodStone.png", L"BloodStone");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/etc/BlueGem.png", L"BlueGem");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/etc/PrizmGem.png", L"PrizmGem");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/etc/RedGem.png", L"RedGem");

	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/etc/Brain.png", L"Brain");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/etc/Claw.png", L"Claw");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/etc/Ear_0.png", L"Ear_0");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/etc/Ear_1.png", L"Ear_1");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/etc/Ear_2.png", L"Ear_2");
	CImg_Manager::Get_Instance()->Insert_Png(
		L"../Resource/Item/etc/Tooth.png", L"Tooth");
#pragma endregion etc

}


