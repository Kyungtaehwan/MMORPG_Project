#include "pch.h"
#include "BigTent.h"
#include "Img_Manager.h"

void CBigTent::Initialize()
{
    __super::Initialize();
    CImg_Manager::Get_Instance()->Insert_Png(
        L"../Resource/Object/Combined/BigTant.png", L"PROP_BIGTENT");
    Set_Sprite(L"PROP_BIGTENT", 639.f, 363.f);

    m_fHeightOffset = 0.f;
}
