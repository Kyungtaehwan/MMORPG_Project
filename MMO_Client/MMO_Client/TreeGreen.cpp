#include "pch.h"
#include "TreeGreen.h"
#include "Img_Manager.h"

void CTreeGreen::Initialize()
{
    __super::Initialize();
    CImg_Manager::Get_Instance()->Insert_Png(
        L"../Resource/Object/Combined/Tree_0.png", L"PROP_TREE_GREEN");
    // 정렬 오프셋 -0.5: 밑동(타일) 동점을 깨서, 플레이어가 나무 타일/앞이면 앞에,
    // 한 칸이라도 뒤로 가면 나무에 가려짐 (밑동 근처에서 스왑). 값 낮출수록 덜 가려짐.
    Set_Sprite(L"PROP_TREE_GREEN", 199.f, 221.f, 0.f, 1.f);
}
