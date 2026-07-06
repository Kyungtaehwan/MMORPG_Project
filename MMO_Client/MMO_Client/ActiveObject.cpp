#include "pch.h"
#include "ActiveObject.h"
#include "Input_Manager.h"

void CActiveObject::Initialize()
{
    m_bDead = false;
    m_bInteractable = false;
    m_bClick = false;
    m_fSpeed = 0.f;
}

int CActiveObject::Update(float dt)
{
    if (m_bInteractable)
    {
        if (CInput_Manager::Get_Instance()->Is_GameMode())  // 추가
        {
            POINT tMouse = CInput_Manager::Get_Instance()->Get_MousePos();
            if (Is_MouseCollide(tMouse))
            {
                CInput_Manager::Get_Instance()->Set_CursorMode(CURSOR_DOOR);
                if (CInput_Manager::Get_Instance()->Mouse_Down_Snap(MBUTTON_L))  // 좌클릭 상호작용
                    On_Click();
            }
        }
    }
    __super::Update_Rect();
    __super::Move_Frame();
    Update_MouseRect();

    return OBJ_NOEVENT;
}

void CActiveObject::Late_Update(float dt) {}

void CActiveObject::Render(ID2D1RenderTarget* pRT)
{
    Render_Indicator(pRT);
}

void CActiveObject::Release() {}

void CActiveObject::On_Collision(CGameObject* pOther)
{
    m_bInteractable = true;
    On_PlayerNear();
}

void CActiveObject::On_CollisionEnd()
{
    m_bInteractable = false;
    CInput_Manager::Get_Instance()->Set_CursorMode(CURSOR_NORMAL);
    On_PlayerFar();
}