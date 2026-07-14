#pragma once
#include "UI.h"
#include "Item_Define.h"

class CItemData_UseItem;
class CPlayer;

class CUI_QuickSlot : public CUI
{
public:
    CUI_QuickSlot() = default;
    virtual ~CUI_QuickSlot() = default;

public:
    virtual void    Initialize()                    override;
    virtual int     Update(float dt)                override;
    virtual void    Late_Update(float dt)           override;
    virtual void    Render(ID2D1RenderTarget* pRT)  override;
    virtual void    Release()                       override;
    virtual void    Process_Event()                 override {}

public:
    void    Set_Player(CPlayer* pPlayer) { m_pPlayer = pPlayer; }

private:
    void    Render_Slots(ID2D1RenderTarget* pRT);
    void    Render_DragIcon(ID2D1RenderTarget* pRT);

    void    On_LButtonDown(POINT tMouse);
    void    On_LButtonUp(POINT tMouse);
    void    On_RClick(POINT tMouse);

    int     Get_SlotAt(POINT tMouse);
    void    Update_SlotValidity();
    bool    Is_UseSlot(int iSlot) { return iSlot >= 0 && iSlot < 4; }  // 0~3만 소비아이템

    // 슬롯 내용 변경의 단일 창구. 값이 실제로 바뀔 때만 서버에 CS_QUICKSLOT_SET 을 보낸다
    // (Update_SlotValidity 가 매 프레임 돌기 때문에 중복 전송을 막아야 함).
    void    Set_Slot(int iSlot, int iCode);
    // 서버 스냅샷(로그인 복원) 반영. 서버가 준 값이므로 되돌려 보내지 않는다.
    void    Sync_FromServer();

#ifdef GAME_DEBUG
    void Debug_Render(ID2D1RenderTarget* pRT);
#endif

private:
    CPlayer* m_pPlayer = nullptr;
    // 포인터 대신 아이템 고유 코드 저장 (0 = 빈칸).
    // 인벤이 서버 스냅샷으로 재구성돼도 코드는 안 바뀌어 등록 유지됨.
    int      m_aSlotCode[8] = {};   // 0~3: 소비아이템, 4~7: 스킬(추후)
    uint32_t m_nQuickVersion = 0;   // 서버 스냅샷을 몇 번째까지 반영했는지

    static constexpr float SLOT_SIZE = 78.f;
    static constexpr float PANEL_W = 624.f;
    static constexpr float PANEL_H = 78.f;
};