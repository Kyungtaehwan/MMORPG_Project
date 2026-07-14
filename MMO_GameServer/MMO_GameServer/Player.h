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
    // 주기 저장이 다른 스레드에서 인벤/골드/장비를 읽을 때 torn snapshot 방지.
    // 인벤/골드/장비를 바꾸는 메서드와 TakeSnapshot 이 같은 락을 공유한다.
    // recursive: Equip/UnEquip 이 내부에서 AddItem 을 부르므로 재귀 허용 필요.
    mutable std::recursive_mutex m_saveLock;

    void AddGold(int32_t nAmount)
    {
        std::lock_guard<std::recursive_mutex> lk(m_saveLock);
        m_gold += nAmount;
    }

    // 골드 소비. 부족하면 false (변경 없음).
    bool SpendGold(int32_t nAmount)
    {
        std::lock_guard<std::recursive_mutex> lk(m_saveLock);
        if (nAmount < 0 || m_gold < nAmount) return false;
        m_gold -= nAmount;
        return true;
    }

    // 슬롯에서 nCount개 제거. 부족하면 false (변경 없음). 0이 되면 슬롯 비움.
    bool RemoveItemSlot(int32_t nSlot, int32_t nCount)
    {
        std::lock_guard<std::recursive_mutex> lk(m_saveLock);
        if (nSlot < 0 || nSlot >= INVEN_SIZE) return false;
        if (m_invenCode[nSlot] <= 0 || nCount <= 0) return false;
        if (m_invenCount[nSlot] < nCount) return false;
        m_invenCount[nSlot] -= nCount;
        if (m_invenCount[nSlot] <= 0) { m_invenCode[nSlot] = 0; m_invenCount[nSlot] = 0; }
        return true;
    }

    // 아이템 1종 추가. 스택 가능(포션/스크롤/기타)은 99까지 누적,
    // 그 외(장비)는 빈 슬롯 차지. 가득 차면 false.
    bool AddItem(int32_t nCode, int32_t nAmount)
    {
        std::lock_guard<std::recursive_mutex> lk(m_saveLock);
        if (nCode <= 0 || nAmount <= 0) return false;

        int category = nCode / 1000;
        bool bStackable = (category == 1 || category == 2 || category == 4);

        if (bStackable)
        {
            for (int i = 0; i < INVEN_SIZE; ++i)
            {
                if (m_invenCode[i] == nCode && m_invenCount[i] < 99)
                {
                    m_invenCount[i] += nAmount;
                    if (m_invenCount[i] > 99) m_invenCount[i] = 99;
                    return true;
                }
            }
        }

        for (int i = 0; i < INVEN_SIZE; ++i)
        {
            if (m_invenCode[i] == 0)
            {
                m_invenCode[i]  = nCode;
                m_invenCount[i] = nAmount;
                return true;
            }
        }
        return false;  // 인벤 가득 참
    }

    // AddItem 이 성공할지 미리 확인(비변경). 경매 구매 전 인벤 여유 판정용.
    // - 원자적 매물 감소가 성공한 뒤 지급이 실패하지 않도록 사전 검사.
    bool CanAddItem(int32_t nCode, int32_t nAmount) const
    {
        std::lock_guard<std::recursive_mutex> lk(m_saveLock);
        if (nCode <= 0 || nAmount <= 0) return false;

        int category = nCode / 1000;
        bool bStackable = (category == 1 || category == 2 || category == 4);
        if (bStackable)
            for (int i = 0; i < INVEN_SIZE; ++i)
                if (m_invenCode[i] == nCode && m_invenCount[i] < 99)
                    return true;                      // 기존 스택에 더 담을 수 있음
        for (int i = 0; i < INVEN_SIZE; ++i)
            if (m_invenCode[i] == 0)
                return true;                          // 빈 슬롯 있음
        return false;
    }

    // ---- 퀵슬롯 (표시 전용 - 서버는 값만 보관하고 DB에 저장한다) ----
    //  인벤과 달리 "권위"랄 게 없다(사용은 CS_USE_ITEM 이 별도로 검증).
    //  등록 내용이 계정에 남아야 하므로 서버가 들고 있다가 로그인 때 돌려준다.
    static constexpr int32_t QUICK_SLOTS_N = 8;   // Protocol.h QUICK_SLOTS 와 동일해야 함
    int32_t  m_quickCode[QUICK_SLOTS_N] = {};   // 슬롯별 itemCode (0=빈칸)

    // 퀵슬롯 한 칸 설정. 범위/코드 검증 후 반영. 소비 가능 아이템(포션1xxx/스크롤2xxx)과
    // 해제(0)만 허용 — 장비를 퀵슬롯에 넣는 잘못된 클라를 걸러낸다.
    bool SetQuickSlot(int32_t nSlot, int32_t nCode)
    {
        std::lock_guard<std::recursive_mutex> lk(m_saveLock);
        if (nSlot < 0 || nSlot >= QUICK_SLOTS_N) return false;
        if (nCode < 0) return false;
        if (nCode > 0)
        {
            int cat = nCode / 1000;
            if (cat != 1 && cat != 2) return false;   // 포션/스크롤만 등록 가능
        }
        m_quickCode[nSlot] = nCode;
        return true;
    }

    // ---- 장비 / 스탯 (서버 권위) ----
    static constexpr int32_t EQUIP_SLOTS = 6;   // 클라 SLOT_END
    int32_t  m_equipCode[EQUIP_SLOTS] = {};     // 슬롯별 itemCode (0=빈칸)
    int32_t  m_iMp = 100;
    int32_t  m_iMaxMp = 100;
    int32_t  m_baseAtk = 10;   // 클라 Player Initialize 와 일치
    int32_t  m_baseDef = 5;

    // ---- 레벨 / 경험치 (서버 권위) ----
    //  필요 경험치 = 100 * 현재레벨 (1→2 100, 2→3 200 …). 만렙 도달 시 경험치 누적 중단.
    //  레벨업 보상은 baseAtk/baseDef/MaxHp/MaxMp 상승 + 풀회복 (LevelUp 참고).
    static constexpr int32_t MAX_LEVEL = 50;
    int32_t  m_nLevel = 1;
    int32_t  m_nExp   = 0;

    static int32_t ExpToNext(int32_t nLevel)
    {
        if (nLevel >= MAX_LEVEL) return 0;   // 만렙 - 더 올릴 곳 없음
        return 100 * nLevel;
    }

    bool IsMaxLevel() const { return m_nLevel >= MAX_LEVEL; }

    // 레벨에서 파생되는 스탯을 다시 계산한다(증분이 아니라 순수 함수).
    // 레벨업 때도, DB에서 레벨을 불러올 때도 같은 식을 쓰므로 값이 어긋날 일이 없다.
    // 최종 공/방은 여기 base 에 장비/버프가 더해진 Get_Atk()/Get_Def().
    void ApplyLevelStats()
    {
        int32_t n = m_nLevel - 1;
        m_iMaxHp  = 100 + n * 20;
        m_iMaxMp  = 100 + n * 10;
        m_baseAtk = 10  + n * 2;
        m_baseDef = 5   + n * 1;
        if (m_iHp > m_iMaxHp) m_iHp = m_iMaxHp;
        if (m_iMp > m_iMaxMp) m_iMp = m_iMaxMp;
    }

    // DB에서 불러온 레벨/경험치 적용(로그인 시 1회). 스탯 재계산 + 풀회복.
    void SetLevelExp(int32_t nLevel, int32_t nExp)
    {
        std::lock_guard<std::recursive_mutex> lk(m_saveLock);
        if (nLevel < 1)         nLevel = 1;
        if (nLevel > MAX_LEVEL) nLevel = MAX_LEVEL;
        m_nLevel = nLevel;
        m_nExp   = (nExp > 0) ? nExp : 0;
        ApplyLevelStats();
        m_iHp = m_iMaxHp;
        m_iMp = m_iMaxMp;
    }

    // 경험치 획득. 필요치를 넘으면 레벨업(연속 레벨업 가능).
    // 반환값 = 이번 호출로 오른 레벨 수(0이면 레벨업 없음).
    // 인벤/장비와 같은 락을 쓴다 - 주기 저장이 반쪽 상태(레벨만 오르고 경험치는 옛값)를
    // 읽지 않도록. 레벨업 시 HP/MP도 바뀌므로 호출자는 SC_PLAYER_HP도 같이 보낼 것.
    int32_t AddExp(int32_t nAmount)
    {
        std::lock_guard<std::recursive_mutex> lk(m_saveLock);
        if (nAmount <= 0 || IsMaxLevel()) return 0;

        m_nExp += nAmount;

        int32_t nGained = 0;
        while (!IsMaxLevel() && m_nExp >= ExpToNext(m_nLevel))
        {
            m_nExp -= ExpToNext(m_nLevel);
            ++m_nLevel;
            ++nGained;
        }

        if (nGained > 0)
        {
            ApplyLevelStats();
            m_iHp = m_iMaxHp;   // 레벨업 보상 - 풀회복
            m_iMp = m_iMaxMp;
        }

        // 만렙이면 남는 경험치는 버린다(클라는 바를 꽉 찬 상태로 표시).
        if (IsMaxLevel()) m_nExp = 0;

        return nGained;
    }

    // 버프 (지속시간 서버 관리)
    uint32_t m_nAtkBuffEnd = 0;
    int32_t  m_nAtkBuffAmt = 0;
    uint32_t m_nInvincibleEnd = 0;

    int32_t Get_Atk() const
    {
        int32_t atk = m_baseAtk;
        for (int i = 0; i < EQUIP_SLOTS; ++i)
            if (m_equipCode[i]) atk += EquipAtk(m_equipCode[i]);
        if (static_cast<uint32_t>(GetTickCount64()) < m_nAtkBuffEnd)
            atk += m_nAtkBuffAmt;
        return atk;
    }
    int32_t Get_Def() const
    {
        int32_t def = m_baseDef;
        for (int i = 0; i < EQUIP_SLOTS; ++i)
            if (m_equipCode[i]) def += EquipDef(m_equipCode[i]);
        return def;
    }
    bool IsInvincible() const
    {
        return static_cast<uint32_t>(GetTickCount64()) < m_nInvincibleEnd;
    }

    // 인벤 슬롯의 장비를 장착 (기존 장비는 인벤으로 반환)
    bool Equip(int32_t nInvenSlot)
    {
        std::lock_guard<std::recursive_mutex> lk(m_saveLock);
        if (nInvenSlot < 0 || nInvenSlot >= INVEN_SIZE) return false;
        int32_t code = m_invenCode[nInvenSlot];
        if (code / 1000 != 3) return false;          // 장비 아님
        int32_t slot = EquipSlotOf(code);
        if (slot < 0 || slot >= EQUIP_SLOTS) return false;

        int32_t prev = m_equipCode[slot];
        m_invenCode[nInvenSlot]  = 0;                // 인벤에서 제거(슬롯 비움)
        m_invenCount[nInvenSlot] = 0;
        m_equipCode[slot] = code;
        if (prev) AddItem(prev, 1);                  // 기존 장비 인벤 반환
        return true;
    }

    // 장비 슬롯 해제 (인벤 빈칸 있어야 함)
    bool UnEquip(int32_t nEquipSlot)
    {
        std::lock_guard<std::recursive_mutex> lk(m_saveLock);
        if (nEquipSlot < 0 || nEquipSlot >= EQUIP_SLOTS) return false;
        int32_t code = m_equipCode[nEquipSlot];
        if (!code) return false;
        if (!AddItem(code, 1)) return false;         // 인벤 가득 참
        m_equipCode[nEquipSlot] = 0;
        return true;
    }

    // 인벤 슬롯 아이템 사용 (포션). 회복/버프 적용 후 수량 차감.
    FUseResult UseItem(int32_t nInvenSlot)
    {
        std::lock_guard<std::recursive_mutex> lk(m_saveLock);
        FUseResult r{ false, -1, 0 };
        if (nInvenSlot < 0 || nInvenSlot >= INVEN_SIZE) return r;
        int32_t code = m_invenCode[nInvenSlot];
        if (code <= 0) return r;

        int cat = code / 1000;
        int sub = code % 1000;

        if (cat == 1)  // 포션
        {
            if (sub < 0 || sub >= 6) return r;
            const FPotionEffect& e = g_PotionTable[sub];
            uint32_t now = static_cast<uint32_t>(GetTickCount64());
            switch (e.kind)
            {
            case PK_HP:
                m_iHp += e.amount; if (m_iHp > m_iMaxHp) m_iHp = m_iMaxHp; break;
            case PK_MP:
                m_iMp += e.amount; if (m_iMp > m_iMaxMp) m_iMp = m_iMaxMp; break;
            case PK_ATK:
                m_nAtkBuffAmt = e.amount; m_nAtkBuffEnd = now + ATK_BUFF_DURATION_MS;
                r.buffType = 0; r.durationMs = static_cast<int32_t>(ATK_BUFF_DURATION_MS); break;
            case PK_INVINCIBLE:
                m_nInvincibleEnd = now + INVINCIBLE_DURATION_MS;
                r.buffType = 1; r.durationMs = static_cast<int32_t>(INVINCIBLE_DURATION_MS); break;
            }
        }
        else if (cat == 2)
        {
            // 스크롤: 효과는 Phase 3, 일단 소비만
        }
        else
        {
            return r;  // 사용 불가 아이템
        }

        // 수량 차감
        if (m_invenCount[nInvenSlot] > 1) --m_invenCount[nInvenSlot];
        else { m_invenCode[nInvenSlot] = 0; m_invenCount[nInvenSlot] = 0; }

        r.used = true;
        return r;
    }

    // ================================================================
    //  TakeSnapshot — 저장용 상태를 락 잡고 통째로 복사
    //   주기 저장이 이 복사본을 받아 락 밖에서 DB에 기록한다(락 시간 최소화).
    //   INVEN_SIZE==FSaveSnapshot::INVEN, EQUIP_SLOTS==FSaveSnapshot::EQUIP.
    // ================================================================
    void TakeSnapshot(FSaveSnapshot& s) const
    {
        std::lock_guard<std::recursive_mutex> lk(m_saveLock);
        std::memset(&s, 0, sizeof(s));
        strncpy_s(s.id, m_szName, sizeof(s.id) - 1);
        s.zoneID = m_nZoneID;
        s.x      = m_fCurX;
        s.z      = m_fCurZ;
        s.gold   = m_gold;
        s.level  = m_nLevel;
        s.exp    = m_nExp;
        for (int i = 0; i < INVEN_SIZE; ++i)
        {
            s.invenCode[i]  = m_invenCode[i];
            s.invenCount[i] = m_invenCount[i];
        }
        for (int i = 0; i < EQUIP_SLOTS; ++i)
            s.equipCode[i] = m_equipCode[i];
        for (int i = 0; i < QUICK_SLOTS_N; ++i)
            s.quickCode[i] = m_quickCode[i];
    }

    // ================================================================
    //  GetCurrentPos
    // 현재 시간 기준 실제 위치 계산
    //
    //  CS_MOVE_DEST를 받은 순간부터
    //  "시작위치 + 방향 * 속도 * 경과시간" 으로 역산
    //
    //  몬스터 AI, 전투 사정거리 계산에서 이걸 쓰면
    //  서버가 항상 정확한 float 위치를 알 수 있음
    // ================================================================
    void GetCurrentPos(uint32_t nCurrentTime, float& fOutX, float& fOutZ) const
    {
        if (!m_bMoving)
        {
            fOutX = m_fCurX;
            fOutZ = m_fCurZ;
            return;
        }

        float fDX = m_fDestX - m_fMoveStartX;
        float fDZ = m_fDestZ - m_fMoveStartZ;
        float fDist = sqrtf(fDX * fDX + fDZ * fDZ);

        if (fDist < 0.001f)
        {
            fOutX = m_fDestX;
            fOutZ = m_fDestZ;
            return;
        }

        // 경과 시간 (ms - 초)
        float fElapsed = (nCurrentTime - m_nMoveStartTime) / 1000.f;
        float fMoved = m_fSpeed * fElapsed;

        if (fMoved >= fDist)
        {
            // 이미 목적지 도착
            fOutX = m_fDestX;
            fOutZ = m_fDestZ;
            return;
        }

        // 시작위치 + 방향 * 이동거리
        fOutX = m_fMoveStartX + (fDX / fDist) * fMoved;
        fOutZ = m_fMoveStartZ + (fDZ / fDist) * fMoved;
    }

    // ================================================================
    //  GetDistanceTo  다른 위치까지 float 거리
    //  몬스터 사정거리 계산에 사용
    // ================================================================
    float GetDistanceTo(float fTargetX, float fTargetZ,
        uint32_t nCurrentTime) const
    {
        float fMyX, fMyZ;
        GetCurrentPos(nCurrentTime, fMyX, fMyZ);

        float fDX = fTargetX - fMyX;
        float fDZ = fTargetZ - fMyZ;
        return sqrtf(fDX * fDX + fDZ * fDZ);
    }

    // ---- 타일 좌표 갱신 ----
    bool UpdateTilePos()
    {
        int32_t nNewTileX = static_cast<int32_t>(floorf(m_fCurX));
        int32_t nNewTileZ = static_cast<int32_t>(floorf(m_fCurZ));

        if (m_nTileX == nNewTileX && m_nTileZ == nNewTileZ)
            return false;

        m_nTileX = nNewTileX;
        m_nTileZ = nNewTileZ;
        return true;
    }

    // ---- 도착 여부 확인 ----
    // 타이머에서 이동 완료 체크에 사용
    bool IsArrived(uint32_t nCurrentTime) const
    {
        if (!m_bMoving) return true;

        float fDX = m_fDestX - m_fMoveStartX;
        float fDZ = m_fDestZ - m_fMoveStartZ;
        float fDist = sqrtf(fDX * fDX + fDZ * fDZ);

        float fElapsed = (nCurrentTime - m_nMoveStartTime) / 1000.f;
        return (m_fSpeed * fElapsed) >= fDist;
    }
};

using PlayerRef = std::shared_ptr<CPlayer>;