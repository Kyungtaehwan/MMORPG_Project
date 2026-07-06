#include "pch.h"
#include "WagonFood.h"
#include "Img_Manager.h"

void CWagonFood::Initialize()
{
    __super::Initialize();
    CImg_Manager::Get_Instance()->Insert_Png(
        L"../Resource/Object/Combined/Wagon_Food.png", L"PROP_WAGON_FOOD");
    Set_Sprite(L"PROP_WAGON_FOOD", 317.f, 242.f);
}
