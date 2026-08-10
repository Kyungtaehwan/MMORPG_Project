#include "pch.h"
#include "Player.h"
#include <cmath>

CPlayer::CPlayer() {}

// ================================================================
//  골드 / 인벤토리
//   전부 m_saveLock 을 잡는다. 주기 저장(TakeSnapshot)이 같은 락을 쓰므로
//   저장 도중에 값이 반쯤 바뀐 스냅샷이 나가지 않는다.
// ================================================================

void CPlayer::AddGold(int32_t nAmount, int32_t* pOutBalance)
{
    std::lock_guard<std::recursive_mutex> lk(m_saveLock);
    m_gold += nAmount;
    if (pOutBalance) *pOutBalance = m_gold;
}

bool CPlayer::SpendGold(int32_t nAmount, int32_t* pOutBalance)
{
    std::lock_guard<std::recursive_mutex> lk(m_saveLock);
    if (nAmount < 0 || m_gold < nAmount) return false;
    m_gold -= nAmount;
    if (pOutBalance) *pOutBalance = m_gold;
    return true;
}

int32_t CPlayer::GetGold() const
{
    std::lock_guard<std::recursive_mutex> lk(m_saveLock);
    return m_gold;
}

bool CPlayer::RemoveItemSlot(int32_t nSlot, int32_t nCount)
{
    std::lock_guard<std::recursive_mutex> lk(m_saveLock);
    if (nSlot < 0 || nSlot >= INVEN_SIZE) return false;
    if (m_invenCode[nSlot] <= 0 || nCount <= 0) return false;
    if (m_invenCount[nSlot] < nCount) return false;
    m_invenCount[nSlot] -= nCount;
    if (m_invenCount[nSlot] <= 0) { m_invenCode[nSlot] = 0; m_invenCount[nSlot] = 0; }
    return true;
}

bool CPlayer::AddItem(int32_t nCode, int32_t nAmount)
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

bool CPlayer::CanAddItem(int32_t nCode, int32_t nAmount) const
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

bool CPlayer::SetQuickSlot(int32_t nSlot, int32_t nCode)
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

// ================================================================
//  레벨 / 경험치
//   파생 스탯은 ApplyLevelStats 한 곳에서만 계산한다. 레벨업 때도 DB 로드
//   때도 같은 식을 쓰므로 두 경로의 값이 어긋나지 않는다.
// ================================================================

int32_t CPlayer::ExpToNext(int32_t nLevel)
{
    if (nLevel >= MAX_LEVEL) return 0;   // 만렙 - 더 올릴 곳 없음
    return 100 * nLevel;
}

bool CPlayer::IsMaxLevel() const
{
    return m_nLevel >= MAX_LEVEL;
}

void CPlayer::ApplyLevelStats()
{
    int32_t n = m_nLevel - 1;
    m_iMaxHp  = 100 + n * 20;
    m_iMaxMp  = 100 + n * 10;
    m_baseAtk = 10  + n * 2;
    m_baseDef = 5   + n * 1;
    if (m_iHp > m_iMaxHp) m_iHp = m_iMaxHp;
    if (m_iMp > m_iMaxMp) m_iMp = m_iMaxMp;
}

void CPlayer::SetLevelExp(int32_t nLevel, int32_t nExp)
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

int32_t CPlayer::AddExp(int32_t nAmount)
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

// ================================================================
//  장비 / 스탯 / 아이템 사용
// ================================================================

int32_t CPlayer::Get_Atk() const
{
    int32_t atk = m_baseAtk;
    for (int i = 0; i < EQUIP_SLOTS; ++i)
        if (m_equipCode[i]) atk += EquipAtk(m_equipCode[i]);
    if (static_cast<uint32_t>(GetTickCount64()) < m_nAtkBuffEnd)
        atk += m_nAtkBuffAmt;
    return atk;
}

int32_t CPlayer::Get_Def() const
{
    int32_t def = m_baseDef;
    for (int i = 0; i < EQUIP_SLOTS; ++i)
        if (m_equipCode[i]) def += EquipDef(m_equipCode[i]);
    return def;
}

bool CPlayer::IsInvincible() const
{
    return static_cast<uint32_t>(GetTickCount64()) < m_nInvincibleEnd;
}

bool CPlayer::Equip(int32_t nInvenSlot)
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

bool CPlayer::UnEquip(int32_t nEquipSlot)
{
    std::lock_guard<std::recursive_mutex> lk(m_saveLock);
    if (nEquipSlot < 0 || nEquipSlot >= EQUIP_SLOTS) return false;
    int32_t code = m_equipCode[nEquipSlot];
    if (!code) return false;
    if (!AddItem(code, 1)) return false;         // 인벤 가득 참
    m_equipCode[nEquipSlot] = 0;
    return true;
}

FUseResult CPlayer::UseItem(int32_t nInvenSlot)
{
    std::lock_guard<std::recursive_mutex> lk(m_saveLock);
    FUseResult r{ false, -1, 0, 0 };
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

    r.used     = true;
    r.itemCode = code;
    return r;
}

// ================================================================
//  TakeSnapshot — 저장용 상태를 락 잡고 통째로 복사
//   주기 저장이 이 복사본을 받아 락 밖에서 DB에 기록한다(락 시간 최소화).
//   INVEN_SIZE==FSaveSnapshot::INVEN, EQUIP_SLOTS==FSaveSnapshot::EQUIP.
// ================================================================
void CPlayer::TakeSnapshot(FSaveSnapshot& s) const
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
void CPlayer::GetCurrentPos(uint32_t nCurrentTime, float& fOutX, float& fOutZ) const
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

    // 경과 시간
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
float CPlayer::GetDistanceTo(float fTargetX, float fTargetZ,
    uint32_t nCurrentTime) const
{
    float fMyX, fMyZ;
    GetCurrentPos(nCurrentTime, fMyX, fMyZ);

    float fDX = fTargetX - fMyX;
    float fDZ = fTargetZ - fMyZ;
    return sqrtf(fDX * fDX + fDZ * fDZ);
}

// ---- 타일 좌표 갱신 ----
bool CPlayer::UpdateTilePos()
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
bool CPlayer::IsArrived(uint32_t nCurrentTime) const
{
    if (!m_bMoving) return true;

    float fDX = m_fDestX - m_fMoveStartX;
    float fDZ = m_fDestZ - m_fMoveStartZ;
    float fDist = sqrtf(fDX * fDX + fDZ * fDZ);

    float fElapsed = (nCurrentTime - m_nMoveStartTime) / 1000.f;
    return (m_fSpeed * fElapsed) >= fDist;
}
