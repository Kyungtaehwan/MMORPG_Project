#include "pch.h"
#include "WagonDrink.h"
#include "Img_Manager.h"

void CWagonDrink::Initialize()
{
    __super::Initialize();
    CImg_Manager::Get_Instance()->Insert_Png(
        L"../Resource/Object/Combined/Wagon_Drink.png", L"PROP_WAGON_DRINK");
    Set_Sprite(L"PROP_WAGON_DRINK", 412.f, 299.f);
}
