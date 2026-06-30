#pragma once
#include "ServerItem.h"

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

    void AddGold(int32_t nAmount) { m_gold += nAmount; }

    // 아이템 1종 추가. 스택 가능(포션/스크롤/기타)은 99까지 누적,
    // 그 외(장비)는 빈 슬롯 차지. 가득 차면 false.
    bool AddItem(int32_t nCode, int32_t nAmount)
    {
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

    // ---- 장비 / 스탯 (서버 권위) ----
    static constexpr int32_t EQUIP_SLOTS = 6;   // 클라 SLOT_END
    int32_t  m_equipCode[EQUIP_SLOTS] = {};     // 슬롯별 itemCode (0=빈칸)
    int32_t  m_iMp = 100;
    int32_t  m_iMaxMp = 100;
    int32_t  m_baseAtk = 10;   // 클라 Player Initialize 와 일치
    int32_t  m_baseDef = 5;

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

        // 경과 시간 (ms → 초)
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