#include "pch.h"
#include "WagonEtc.h"
#include "Img_Manager.h"

void CWagonEtc::Initialize()
{
    __super::Initialize();
    CImg_Manager::Get_Instance()->Insert_Png(
        L"../Resource/Object/Combined/Wagon_etc.png", L"PROP_WAGON_ETC");
    Set_Sprite(L"PROP_WAGON_ETC", 248.f, 184.f);
}
