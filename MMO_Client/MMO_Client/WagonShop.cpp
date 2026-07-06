#include "pch.h"
#include "WagonShop.h"
#include "Img_Manager.h"

void CWagonShop::Initialize()
{
    __super::Initialize();
    CImg_Manager::Get_Instance()->Insert_Png(
        L"../Resource/Object/Combined/Wagon_Shop.png", L"PROP_WAGON_SHOP");
    Set_Sprite(L"PROP_WAGON_SHOP", 445.f, 200.f);
}
