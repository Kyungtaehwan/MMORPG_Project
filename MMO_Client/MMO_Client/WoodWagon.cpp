#include "pch.h"
#include "WoodWagon.h"
#include "Img_Manager.h"

void CWoodWagon::Initialize()
{
    __super::Initialize();
    CImg_Manager::Get_Instance()->Insert_Png(
        L"../Resource/Object/Combined/Wood_Wagon.png", L"PROP_WOOD_WAGON");
    Set_Sprite(L"PROP_WOOD_WAGON", 173.f, 79.f);
}
