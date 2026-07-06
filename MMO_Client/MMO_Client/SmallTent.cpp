#include "pch.h"
#include "SmallTent.h"
#include "Img_Manager.h"

void CSmallTent::Initialize()
{
    __super::Initialize();
    CImg_Manager::Get_Instance()->Insert_Png(
        L"../Resource/Object/Combined/SmallTant.png", L"PROP_SMALLTENT");
    Set_Sprite(L"PROP_SMALLTENT", 361.f, 281.f);
}
