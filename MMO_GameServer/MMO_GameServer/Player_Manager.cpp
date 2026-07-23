#include "pch.h"
#include "Player_Manager.h"
#include "DB_Manager.h"

CPlayer_Manager* CPlayer_Manager::m_pInstance = nullptr;

PlayerRef CPlayer_Manager::Create(int32_t nSessionID)
{
    std::lock_guard<std::mutex> lock(m_lock);

    // 세션 ID와 동일한 슬롯 사용
    // - CSession_Manager와 인덱스가 맞아서 조회가 단순해짐
    if (nSessionID < 0 || nSessionID >= MAX_PLAYER) return nullptr;
    if (m_players[nSessionID] != nullptr) return nullptr;

    PlayerRef pPlayer = std::make_shared<CPlayer>();
    pPlayer->m_nPlayerID = nSessionID;
    pPlayer->m_nSessionID = nSessionID;
    m_players[nSessionID] = pPlayer;

    return pPlayer;
}

PlayerRef CPlayer_Manager::Get_Player(int32_t nPlayerID)
{
    if (nPlayerID < 0 || nPlayerID >= MAX_PLAYER) return nullptr;

    std::lock_guard<std::mutex> lock(m_lock);
    return m_players[nPlayerID];
}

void CPlayer_Manager::Remove(int32_t nPlayerID)
{
    if (nPlayerID < 0 || nPlayerID >= MAX_PLAYER) return;

    std::lock_guard<std::mutex> lock(m_lock);
    if (m_players[nPlayerID] != nullptr)
    {
        m_players[nPlayerID] = nullptr;
        std::cout << "[CPlayer_Manager] 플레이어 제거. ID=" << nPlayerID << std::endl;
    }
}

// ----------------------------------------------------------------
//  AutoSaveNext : 주기 저장(라운드로빈) — 한 틱에 온라인 1명 저장
//   - m_lock 은 "다음 대상 1명 찾고 shared_ptr 복사"에만 짧게 사용.
//   - 느린 DB I/O(스냅샷 저장)는 락을 모두 푼 뒤 수행 - 다른 워커/접속 안 막음.
// ----------------------------------------------------------------
void CPlayer_Manager::AutoSaveNext()
{
    PlayerRef target = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_lock);
        // 커서부터 한 바퀴 돌며 다음 "로그인된" 플레이어를 찾는다.
        for (int32_t n = 0; n < MAX_PLAYER; ++n)
        {
            int32_t idx = (m_saveCursor + n) % MAX_PLAYER;
            PlayerRef p = m_players[idx];
#ifdef STRESS_TEST
            // 부하 봇(bot_*)은 DB 계정이 없으므로 자동저장에서 제외
            // (측정 오염·워커 블로킹·DB 오염 방지)
            if (p && strncmp(p->m_szName, "bot_", 4) == 0) continue;
#endif
            if (p && p->m_szName[0] != '\0')
            {
                target = p;
                m_saveCursor = (idx + 1) % MAX_PLAYER;   // 다음 틱은 그다음부터
                break;
            }
        }
    }
    if (!target) return;   // 접속자 없음 - 이번 틱은 그냥 넘어감

    // 락 밖: 스냅샷 뜨고(플레이어 자체 락) DB에 저장(락 없음)
    FSaveSnapshot snap;
    target->TakeSnapshot(snap);
    CDB_Manager::Get_Instance()->Save(snap);
}