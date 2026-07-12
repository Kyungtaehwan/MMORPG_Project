#pragma once
#include "UI.h"
#include "Protocol.h"        // FAuctionEntry, AUCTION_MAX
#include "UI_CloseButton.h"
#include "UI_InvenGrid.h"
#include "UI_QtyDialog.h"
#include "UI_ConfirmDialog.h"

class CPlayer;
class CInventory;

// ================================================================
//  CUI_Auction  경매장 UI (즉시구매 / 개당가격 / 부분구매)
//   탭: 구매 / 등록 / 내판매
//   - 구매: 전체 매물 + 검색 + 페이지네이션, 행 [구매]-수량 다이얼로그
//   - 등록: 인벤 그리드에서 아이템 선택 + 개수/가격 입력 - [등록]
//   - 내판매: 내 매물(남은수량/미수령골드), [수령] 버튼
//   - 데이터는 서버 SC_AUCTION_LIST 스냅샷(Network_Manager 보관)을 참조.
// ================================================================
class CUI_Auction : public CUI
{
public:
    enum TAB { TAB_BUY, TAB_REGISTER, TAB_MINE };
    enum FOCUS { FOCUS_NONE, FOCUS_SEARCH, FOCUS_COUNT, FOCUS_PRICE };

public:
    CUI_Auction() = default;
    virtual ~CUI_Auction() = default;

    virtual void Initialize()                    override;
    virtual int  Update(float dt)                override;
    virtual void Late_Update(float dt)           override;
    virtual void Render(ID2D1RenderTarget* pRT)  override;
    virtual void Release()                       override;
    virtual void Process_Event()                 override {}

    void Set_References(CPlayer* pPlayer, CInventory* pInven)
    {
        m_pPlayer = pPlayer;
        m_pInven = pInven;
        m_grid.Set_Reference(pInven);
    }

    void Open();
    void Close();
    bool Is_Open() const { return m_bVisible; }
    void On_Char(wchar_t ch);   // 검색/숫자 입력 (WM_CHAR 경로)

private:
    // 입력
    int  Get_TabAt(POINT m);
    void Update_Buy(POINT m, bool bClick);
    void Update_Register(POINT m, bool bClick);
    void Update_Mine(POINT m, bool bClick);

    // 서버 페이지네이션: 현재 탭/페이지/검색으로 목록 재요청
    void Request_List();
    // 검색어(m_szSearch) 일치 아이템 코드 수집 - outCodes. 반환=개수. (구매 탭 검색용)
    int  Resolve_Search(int32_t* outCodes, int cap);
    int  Server_Tab() const { return (m_iTab == TAB_MINE) ? 1 : 0; }  // 클라탭-서버탭(0구매/1내판매)

    // 렌더
    void Render_Bg(ID2D1RenderTarget* pRT);
    void Render_Tabs(ID2D1RenderTarget* pRT);
    void Render_Buy(ID2D1RenderTarget* pRT);
    void Render_Register(ID2D1RenderTarget* pRT);
    void Render_Mine(ID2D1RenderTarget* pRT);
    void Render_ListRows(ID2D1RenderTarget* pRT, bool bMine);
    void Draw_Btn(ID2D1RenderTarget* pRT, const RECT& r, const TCHAR* label,
        POINT m, bool bAccent, bool bEnabled = true);

    // 헬퍼
    static void Item_Display(int code, TCHAR* outName, int nameCap,
        TCHAR* outIcon, int iconCap);
    RECT R(float ox, float oy, float w, float h) const;  // 패널 기준 rect

private:
    CPlayer* m_pPlayer = nullptr;
    CInventory* m_pInven = nullptr;
    bool        m_bVisible = false;
    int         m_iTab = TAB_BUY;
    int         m_iFocus = FOCUS_NONE;
    bool        m_bFieldFresh = true;   // 숫자필드 포커스 후 첫 입력이면 값 교체

    CUI_CloseButton  m_closeBtn;
    CUI_InvenGrid    m_grid;      // 등록 탭
    CUI_QtyDialog    m_dlg;       // 구매 수량(팝업 겸 확인)
    CUI_ConfirmDialog m_confirm;  // 등록/취소 확인 팝업
    int              m_iPendingListing = 0;  // 구매 확정 대상

    // 확인 팝업 대기 액션
    enum PENDING { PEND_NONE, PEND_REGISTER, PEND_CANCEL };
    int  m_iPending = PEND_NONE;
    int  m_iPendingCancelID = 0;

    // 검색 / 페이지 (페이지네이션은 서버가 수행 — m_iPage=서버에 요청할 페이지)
    wchar_t m_szSearch[32] = {};
    int     m_iPage = 0;

    // 등록 입력
    int m_iRegSlot = -1;
    int m_iRegCount = 1;
    int m_iRegPrice = 10;

    static constexpr float PANEL_W = 560.f;
    static constexpr float PANEL_H = 470.f;
    static constexpr int   ROWS_PER_PAGE = 5;
    static constexpr float ROW_H = 62.f;
    static constexpr float CONTENT_TOP = 92.f;   // 패널 상단 - 컨텐츠
    static constexpr float LIST_TOP = 128.f;     // 패널 상단 - 리스트 첫행(검색/페이지바 아래)
};
