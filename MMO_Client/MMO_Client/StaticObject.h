#pragma once
#include "GameObject.h"

// ================================================================
//  CStaticObject  정적(비상호작용) 오브젝트 베이스
//  - CActiveObject(상호작용/클릭)와 대칭되는 클래스.
//  - 월드 좌표에 스프라이트 1장을 아이소 깊이정렬로 렌더만 함.
//  - 클릭/상호작용/이동 없음. 각 오브젝트는 이 클래스를 상속해
//    Initialize에서 Set_Sprite 스프라이트만 지정.
//  - 충돌은 별도 타일에서 처리.
// ================================================================
class CStaticObject : public CGameObject
{
public:
    CStaticObject() = default;
    virtual ~CStaticObject() = default;

public:
    virtual void Initialize()                   override;
    virtual int  Update(float dt)               override;
    virtual void Late_Update(float dt)          override;
    virtual void Render(ID2D1RenderTarget* pRT) override;
    virtual void Release()                      override;

protected:

    void Set_Sprite(const TCHAR* key, float cx, float cy,
        float fHeight = 0.f, float fSortOff = 0.f);

protected:
    const TCHAR* m_pImgKey = L"";
    float        m_fHeightOffset = 0.f;
};
