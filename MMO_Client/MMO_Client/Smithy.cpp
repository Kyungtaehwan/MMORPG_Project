#include "pch.h"
#include "Smithy.h"
#include "Img_Manager.h"

void CSmithy::Initialize()
{
    __super::Initialize();
    CImg_Manager::Get_Instance()->Insert_Png(
        L"../Resource/Object/Combined/Smithy.png", L"PROP_SMITHY");
    Set_Sprite(L"PROP_SMITHY", 316.f, 246.f);
}
