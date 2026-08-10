#pragma once
#include "ServerItem.h"
#include "SaveData.h"
#include <mutex>
#include <cstring>

// 아이템 사용 결과 (버프 적용 시 클라에 알리기 위함)
struct FUseResult
{
    bool    used;
    int32_t buffType;    // -1=없음, 0=공격력, 1=무적
    int32_t durationMs;
    int32_t itemCode;    // 실제로 소비된 아이템 코드(거래 로그용). used=false 면 0
};

enum PLAYER_STATE {
    PLAYER_IDLE, PLAYER_WALK, PLAYER_HIT, PLAYER_ATTACK, PLAYER_DEAD, PLAYER_END
};

class CPlayer
{
public:
    CPlayer();
    ~CPlayer() = default;

    // ---- 기본 정보 ----
    int32_t  m_nPlayerID = -1;
    int32_t  m_nSessionID = -1;
    char     m_szName[20] = {};

    // ---- 현재 위치 ----
    // CS_MOVE_로 업데이트되는 마지막 확인 위치
    float    m_fCurX = 0.f;
    float    m_fCurZ = 0.f;

    // ---- 이동 정보 ----
    float    m_fDestX = 0.f;       // 목적지
    float    m_fDestZ = 0.f;
    float    m_fMoveStartX = 0.f;  // 이동 시작 위치
    float    m_fMoveStartZ = 0.f;
    uint32_t m_nMoveStartTime = 0; // 이동 시작 시간 (ms)
    bool     m_bMoving = false;
    PLAYER_STATE m_eState = PLAYER_IDLE;
    // ---- 속도 ----
    float    m_fSpeed = 1.f;
    uint8_t  m_eDir = 0;
    // ---- 전투 -----
    int32_t       m_iHp = 100;
    int32_t       m_iMaxHp = 100;


    // ---- 타일 좌표 (시야 계산용) ----
    int32_t  m_nTileX = 0;
    int32_t  m_nTileZ = 0;

    // 지금 속한 섹터 번호 인덱스
    // 소속 존이 없으면 -1
    int32_t  m_nSectorIdx = -1;

    // ---- 존 정보 ----
    int32_t  m_nZoneID = 0;

    // ---- 시야 리스트 ----
    std::unordered_set<int32_t> m_viewList;
    std::mutex                  m_viewLock;

    std::unordered_set<int32_t> m_monsterViewList;
    std::mutex                  m_monsterViewLock;
    // ---- 마지막 패킷 시간 ----
    uint32_t m_nLastMoveTime = 0;
    uint32_t m_nLastAtkTime  = 0;
    uint32_t m_nAtkCoolMs    = 400;
    bool     m_bDead = false;

    // ---- 인벤토리 (서버 단일 진실, new 없이 값으로만 관리) ----
    static constexpr int32_t INVEN_SIZE = 40;
    int32_t  m_invenCode[INVEN_SIZE]  = {};  // itemCode (0 = 빈 슬롯)
    int32_t  m_invenCount[INVEN_SIZE] = {};  // 슬롯별 개수
    int32_t  m_gold = 0;

    // ---- 세이브 스냅샷 락 ----
    mutable std::recursive_mutex m_saveLock;

    // pOutBalance : 변경 "직후"의 잔액을 같은 락 안에서 받아간다(거래 로그의 gold_balance 용).
    void AddGold(int32_t nAmount, int32_t* pOutBalance = nullptr);

    // 골드 소비. 부족하면 false (변경 없음).
    bool SpendGold(int32_t nAmount, int32_t* pOutBalance = nullptr);

    // 현재 잔액(락 잡고 읽기). 골드가 안 변하는 로그의 gold_balance 용.
    int32_t GetGold() const;

    // 슬롯에서 nCount개 제거. 부족하면 false (변경 없음). 0이 되면 슬롯 비움.
    bool RemoveItemSlot(int32_t nSlot, int32_t nCount);

    // 아이템 1종 추가. 스택 가능(포션/스크롤/기타)은 99까지 누적,
    // 그 외(장비)는 빈 슬롯 차지. 가득 차면 false.
    bool AddItem(int32_t nCode, int32_t nAmount);

    // AddItem 이 성공할지 미리 확인(비변경). 경매 구매 전 인벤 여유 판정용.
    // - 원자적 매물 감소가 성공한 뒤 지급이 실패하지 않도록 사전 검사.
    bool CanAddItem(int32_t nCode, int32_t nAmount) const;

    // ---- 퀵슬롯 (표시 전용 - 서버는 값만 보관하고 DB에 저장한다) ----
    //  등록 내용이 계정에 남아야 하므로 서버가 들고 있다가 로그인 때 돌려준다.(권위X)
    static constexpr int32_t QUICK_SLOTS_N = 8;   // Protocol.h QUICK_SLOTS 와 동일해야 함
    int32_t  m_quickCode[QUICK_SLOTS_N] = {};   // 슬롯별 itemCode (0=빈칸)

    // 퀵슬롯 한 칸 설정. 범위/코드 검증 후 반영. 소비 가능 아이템과
    // 해제(0)만 허용 — 장비를 퀵슬롯에 넣는 잘못된 클라를 걸러낸다.
    bool SetQuickSlot(int32_t nSlot, int32_t nCode);

    // ---- 장비 / 스탯 (서버 권위) ----
    static constexpr int32_t EQUIP_SLOTS = 6;   // 클라 SLOT_END
    int32_t  m_equipCode[EQUIP_SLOTS] = {};     // 슬롯별 itemCode (0=빈칸)
    int32_t  m_iMp = 100;
    int32_t  m_iMaxMp = 100;
    int32_t  m_baseAtk = 10;   // 클라 Player Initialize 와 일치
    int32_t  m_baseDef = 5;

    // ---- 레벨 / 경험치 (서버 권위) ----
    static constexpr int32_t MAX_LEVEL = 50;
    int32_t  m_nLevel = 1;
    int32_t  m_nExp   = 0;

    // 다음 레벨까지 필요한 경험치. 만렙이면 0.
    static int32_t ExpToNext(int32_t nLevel);

    bool IsMaxLevel() const;

    // 레벨에서 파생되는 스탯을 다시 계산
    void ApplyLevelStats();

    // DB에서 불러온 레벨/경험치 적용(로그인 시 1회). 스탯 재계산 + 풀회복.
    void SetLevelExp(int32_t nLevel, int32_t nExp);

    // 경험치 획득. 필요치를 넘으면 레벨업
    // 반환값 = 이번 호출로 오른 레벨 수
    int32_t AddExp(int32_t nAmount);

    // 버프 (지속시간 서버 관리)
    uint32_t m_nAtkBuffEnd = 0;
    int32_t  m_nAtkBuffAmt = 0;
    uint32_t m_nInvincibleEnd = 0;

    // 기본 + 장비 + 버프
    int32_t Get_Atk() const;
    // 기본 + 장비
    int32_t Get_Def() const;
    bool IsInvincible() const;

    // 인벤 슬롯의 장비를 장착 (기존 장비는 인벤으로 반환)
    bool Equip(int32_t nInvenSlot);

    // 장비 슬롯 해제 (인벤 빈칸 있어야 함)
    bool UnEquip(int32_t nEquipSlot);

    // 인벤 슬롯 아이템 사용 (포션). 회복/버프 적용 후 수량 차감.
    FUseResult UseItem(int32_t nInvenSlot);

    // 저장용 상태를 락 잡고 통째로 복사.
    //  주기 저장이 이 복사본을 받아 락 밖에서 DB에 기록한다(락 시간 최소화).
    void TakeSnapshot(FSaveSnapshot& s) const;

    // 현재 시간 기준 실제 위치 계산(역산).
    //  "시작위치 + 방향 * 속도 * 경과시간" — 몬스터 AI, 전투 사거리가 쓴다.
    void GetCurrentPos(uint32_t nCurrentTime, float& fOutX, float& fOutZ) const;

    // 다른 위치까지 float 거리. 몬스터 사정거리 계산에 사용.
    float GetDistanceTo(float fTargetX, float fTargetZ, uint32_t nCurrentTime) const;

    // 타일 좌표 갱신. 타일이 바뀌었으면 true.
    bool UpdateTilePos();

    // 도착 여부 확인. 타이머에서 이동 완료 체크에 사용.
    bool IsArrived(uint32_t nCurrentTime) const;
};

using PlayerRef = std::shared_ptr<CPlayer>;
