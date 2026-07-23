#include "pch.h"
#include "Packet_Handler.h"
#include "Session.h"
#include "Session_Manager.h"
#include "Player_Manager.h"
#include "Zone_Manager.h"
#include "Protocol.h"
#include "AccountDB.h"
#include "DB_Manager.h"
#include "ServerItem.h"
#include "StressMetrics.h"

void CPacket_Handler::Handle(std::shared_ptr<CSession> pSession,
    uint8_t* pBuffer, int32_t nSize)
{
    PacketHeader* pHeader = reinterpret_cast<PacketHeader*>(pBuffer);

    // 부하 계측: 핸들러 처리 시간(락 대기·시야계산 포함)을 us 단위로 기록
    const int64_t nMetricStart = StressMetrics::Now();

    switch (pHeader->id)
    {
    case CS_LOGIN:     Handle_CS_LOGIN(pSession, pBuffer, nSize); break;
    case CS_MOVE_DEST: Handle_CS_MOVE_DEST(pSession, pBuffer, nSize); break;
    case CS_MOVE_POS:  Handle_CS_MOVE_POS(pSession, pBuffer, nSize); break;
    case CS_ATTACK_MONSTER: Handle_CS_ATTACK_MONSTER(pSession, pBuffer, nSize); break;
    case CS_RESPAWN:         Handle_CS_RESPAWN(pSession, pBuffer, nSize); break;
    case CS_PORTAL:          Handle_CS_PORTAL(pSession, pBuffer, nSize); break;
    case CS_PICKUP:          Handle_CS_PICKUP(pSession, pBuffer, nSize); break;
    case CS_EQUIP:           Handle_CS_EQUIP(pSession, pBuffer, nSize); break;
    case CS_UNEQUIP:         Handle_CS_UNEQUIP(pSession, pBuffer, nSize); break;
    case CS_USE_ITEM:        Handle_CS_USE_ITEM(pSession, pBuffer, nSize); break;
    case CS_MOVE_STOP:       Handle_CS_MOVE_STOP(pSession, pBuffer, nSize); break;
    case CS_BUY:             Handle_CS_BUY(pSession, pBuffer, nSize); break;
    case CS_SELL:            Handle_CS_SELL(pSession, pBuffer, nSize); break;
    case CS_AUCTION_LIST:     Handle_CS_AUCTION_LIST(pSession, pBuffer, nSize); break;
    case CS_AUCTION_REGISTER: Handle_CS_AUCTION_REGISTER(pSession, pBuffer, nSize); break;
    case CS_AUCTION_BUY:      Handle_CS_AUCTION_BUY(pSession, pBuffer, nSize); break;
    case CS_AUCTION_COLLECT:  Handle_CS_AUCTION_COLLECT(pSession, pBuffer, nSize); break;
    case CS_AUCTION_CANCEL:   Handle_CS_AUCTION_CANCEL(pSession, pBuffer, nSize); break;
    case CS_QUICKSLOT_SET:    Handle_CS_QUICKSLOT_SET(pSession, pBuffer, nSize); break;
    default:
        std::cout << "[CPacket_Handler] 알 수 없는 패킷: "
            << pHeader->id << std::endl;
        break;
    }

    StressMetrics::Record(StressMetrics::ElapsedUs(nMetricStart));
}

void CPacket_Handler::Handle_CS_LOGIN(std::shared_ptr<CSession> pSession,
    uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_LOGIN_PACKET))) return;
    CS_LOGIN_PACKET* pPkt = reinterpret_cast<CS_LOGIN_PACKET*>(pBuffer);

    // 널 종단 보장 (클라가 꽉 채워 보냈을 경우 대비)
    pPkt->id[sizeof(pPkt->id) - 1] = '\0';
    pPkt->pw[sizeof(pPkt->pw) - 1] = '\0';

#ifdef STRESS_TEST
    // ---- 부하 테스트 우회 ----
    // id 가 "bot_" 로 시작하면 DB(sp_login)를 건너뛰고 기본 스탯으로 즉시 입장.
    // DB 를 변수에서 제외해 순수 월드 로직(AOI/브로드캐스트) 부하만 측정한다.
    // (STRESS_TEST 매크로가 정의된 빌드에서만 컴파일 — 일반 빌드엔 없음)
    if (strncmp(pPkt->id, "bot_", 4) == 0)
    {
        PlayerRef pBot = CPlayer_Manager::Get_Instance()->Create(pSession->GetID());
        if (!pBot) { Send_SC_LOGIN_FAIL(pSession, 0); return; }

        strncpy_s(pBot->m_szName, pPkt->id, sizeof(pBot->m_szName) - 1);
        pBot->SetLevelExp(1, 0);   // 레벨1 기본 스탯(HP/MP) 적용
        pBot->m_gold = 0;

        // 봇이 들어갈 존은 pw 로 지정(봇이 존 번호 문자열을 pw 에 담아 보냄).
        //   재빌드 없이 맵을 바꿔가며 측정하기 위함.
        //   1=마을(몬스터 없음, 순수 AOI) / 5=레이드(장애물·A*) / 6=레이드평지(대조군).
        int32_t nBotZone = atoi(pPkt->pw);
        CZone* pZone = CZone_Manager::Get_Instance()->GetZone(nBotZone);
        if (!pZone) pZone = CZone_Manager::Get_Instance()->GetZone(ZONE_TOWN);
        if (!pZone) { Send_SC_LOGIN_FAIL(pSession, 0); return; }

        // 봇을 존 전역에 분산 배치(한 점에 뭉치면 시야리스트가 N 이라 비현실적)
        float fSpawnX = 12.5f, fSpawnZ = 15.5f;
        pZone->FindWalkableSpawn(fSpawnX, fSpawnZ);
        pZone->EnterZone(pBot, fSpawnX, fSpawnZ);

        Send_SC_LOGIN_OK(pSession, pBot->m_nPlayerID);
        Send_SC_ENTER_GAME(pSession);
        return;   // 인벤/장비/퀵슬롯 등 무거운 초기 전송은 생략(봇은 불필요)
    }
#endif

    // ---- 인증 + 계정 데이터 로드 (DB: sp_login 호출) ----
    // sp_login 이 결과셋 3개(character/inventory/equipment)를 돌려주고
    // CDB_Manager::Login 이 이를 FAccountData 로 채워준다. 인증 실패면 false.
    FAccountData acc;
    if (!CDB_Manager::Get_Instance()->Login(pPkt->id, pPkt->pw, acc))
    {
        Send_SC_LOGIN_FAIL(pSession, 1);   // 1 = 아이디/비번 불일치(또는 DB 오류)
        return;
    }
    const FAccountData* pAcc = &acc;

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Create(pSession->GetID());
    if (!pPlayer) { Send_SC_LOGIN_FAIL(pSession, 0); return; }

    // DB 경로에선 pAcc->id 가 비어 있으므로, 클라가 보낸 id 로 이름 설정.
    strncpy_s(pPlayer->m_szName, pPkt->id, sizeof(pPlayer->m_szName) - 1);

    // ---- 계정 데이터 반영 (DB에서 로드된 값: 골드/레벨/인벤/장비) ----
    // 레벨을 먼저 적용해야 MaxHp/MaxMp/기본공방이 그 레벨 기준으로 재계산되고
    // 풀피 상태로 입장한다(SetLevelExp 내부에서 ApplyLevelStats + 회복).
    pPlayer->SetLevelExp(pAcc->level, pAcc->exp);
    pPlayer->m_gold = pAcc->gold;
    for (const FSaveItem& it : pAcc->inven)
    {
        if (it.code == 0) break;
        pPlayer->AddItem(it.code, it.count);
    }
    for (int i = 0; i < CPlayer::EQUIP_SLOTS; ++i)
        pPlayer->m_equipCode[i] = pAcc->equip[i];
    for (int i = 0; i < CPlayer::QUICK_SLOTS_N; ++i)
        pPlayer->m_quickCode[i] = pAcc->quick[i];

    // ---- 계정에 저장된 존/위치로 입장 ----
    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pAcc->zoneID);
    if (!pZone) { Send_SC_LOGIN_FAIL(pSession, 0); return; }

    pZone->EnterZone(pPlayer, pAcc->spawnX, pAcc->spawnZ);

    Send_SC_LOGIN_OK(pSession, pPlayer->m_nPlayerID);
    Send_SC_ENTER_GAME(pSession);

    // 골드/인벤/장비를 클라에 1회 동기화 (ENTER_GAME 뒤 - 클라 플레이어 생성 후 도착)
    pZone->Send_InvenUpdate(pPlayer);

    // 레벨/경험치 + 그 레벨의 HP/MP 도 1회 동기화 (클라 HUD 초기 표시)
    pZone->Send_PlayerExp(pPlayer, false);
    pZone->Send_PlayerHp(pPlayer);

    // 퀵슬롯 복원. 반드시 Send_InvenUpdate 뒤에 보낼 것 —
    // 클라는 등록된 코드를 인벤에서 찾아 아이콘을 그리므로 인벤이 먼저 채워져 있어야 한다.
    Send_SC_QUICKSLOT_UPDATE(pSession);
}

// ================================================================
//  Handle_CS_QUICKSLOT_SET — 퀵슬롯 한 칸 등록/해제
//   퀵슬롯은 "표시 전용"이라 응답을 돌려주지 않는다(클라가 이미 그리고 있음).
//   서버는 값만 보관했다가 주기 저장/접속 종료 때 DB에 쓰고, 다음 로그인에 돌려준다.
// ================================================================
void CPacket_Handler::Handle_CS_QUICKSLOT_SET(std::shared_ptr<CSession> pSession,
    uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_QUICKSLOT_SET_PACKET))) return;
    CS_QUICKSLOT_SET_PACKET* pPkt = reinterpret_cast<CS_QUICKSLOT_SET_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer) return;

    pPlayer->SetQuickSlot(pPkt->slot, pPkt->itemCode);   // 범위/코드 검증은 안에서
}

void CPacket_Handler::Send_SC_QUICKSLOT_UPDATE(std::shared_ptr<CSession> pSession)
{
    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer) return;

    SC_QUICKSLOT_UPDATE_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_QUICKSLOT_UPDATE;
    for (int i = 0; i < QUICK_SLOTS; ++i)
        pkt.codes[i] = pPlayer->m_quickCode[i];
    pSession->Send(&pkt, sizeof(pkt));
}

// ================================================================
//  Handle_CS_MOVE_DEST  마우스 클릭 시
//  목적지 검증 + 현재 view_list에 브로드캐스트
// ================================================================
void CPacket_Handler::Handle_CS_MOVE_DEST(std::shared_ptr<CSession> pSession,
    uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_MOVE_DEST_PACKET))) return;
    CS_MOVE_DEST_PACKET* pPkt = reinterpret_cast<CS_MOVE_DEST_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer) return;

    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
    if (!pZone) return;

    pZone->OnMoveDest(pPlayer, pPkt->fDestX, pPkt->fDestZ, pPkt->moveTime);
}

// ================================================================
//  Handle_CS_MOVE_POS  이동 중 타일 변경 시
//  위치 업데이트 + 시야 재계산 + 브로드캐스트
// ================================================================
void CPacket_Handler::Handle_CS_MOVE_POS(std::shared_ptr<CSession> pSession,
    uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_MOVE_POS_PACKET))) return;
    CS_MOVE_POS_PACKET* pPkt = reinterpret_cast<CS_MOVE_POS_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer) return;

    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
    if (!pZone) return;

    pZone->OnMovePos(pPlayer, pPkt->fCurX, pPkt->fCurZ, pPkt->moveTime);
}

void CPacket_Handler::Send_SC_LOGIN_OK(std::shared_ptr<CSession> pSession,
    uint32_t nPlayerID)
{
    SC_LOGIN_OK_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_LOGIN_OK;
    pkt.playerID = nPlayerID;
    pSession->Send(&pkt, sizeof(pkt));
}

void CPacket_Handler::Send_SC_LOGIN_FAIL(std::shared_ptr<CSession> pSession,
    uint8_t nReason)
{
    SC_LOGIN_FAIL_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_LOGIN_FAIL;
    pkt.reason = nReason;
    pSession->Send(&pkt, sizeof(pkt));
}

void CPacket_Handler::Send_SC_ENTER_GAME(std::shared_ptr<CSession> pSession)
{
    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer) return;

    SC_ENTER_GAME_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_ENTER_GAME;
    pkt.playerID = pPlayer->m_nPlayerID;
    pkt.fCurX = pPlayer->m_fCurX;
    pkt.fCurZ = pPlayer->m_fCurZ;
    pkt.zoneID = pPlayer->m_nZoneID;
    strncpy_s(pkt.name, pPlayer->m_szName, sizeof(pkt.name) - 1);
    pSession->Send(&pkt, sizeof(pkt));
}

void CPacket_Handler::Handle_CS_ATTACK_MONSTER(
    std::shared_ptr<CSession> pSession,
    uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_ATTACK_MONSTER_PACKET))) return;

    CS_ATTACK_MONSTER_PACKET* pPkt =
        reinterpret_cast<CS_ATTACK_MONSTER_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()
        ->Get_Player(pSession->GetID());
    if (!pPlayer) return;

    CZone* pZone = CZone_Manager::Get_Instance()
        ->GetZone(pPlayer->m_nZoneID);
    if (!pZone) return;

    pZone->OnPlayerAttackMonster(pPlayer,
        pPkt->monsterID, pPkt->fCurX, pPkt->fCurZ);
}


void CPacket_Handler::Handle_CS_RESPAWN(
    std::shared_ptr<CSession> pSession,
    uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_RESPAWN_PACKET))) return;

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer) return;
    if (!pPlayer->m_bDead) return;

    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
    if (!pZone) return;

    pZone->OnPlayerRespawn(pPlayer);
}

// ================================================================
//  Handle_CS_PORTAL  포탈 존 전환
//  1) 현재 존에서 나가기 (제거 브로드캐스트 + 시야 정리)
//  2) 클라에 새 맵 로드를 지시 (새 add 패킷보다 먼저 보내야 함)
//  3) 새 존에 진입 (새 플레이어/몬스터 add, 옛 몬스터 remove 전송)
// ================================================================
void CPacket_Handler::Handle_CS_PORTAL(
    std::shared_ptr<CSession> pSession,
    uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_PORTAL_PACKET))) return;
    CS_PORTAL_PACKET* pPkt = reinterpret_cast<CS_PORTAL_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer) return;
    if (pPlayer->m_bDead) return;

    // 이미 목표 존에 있으면 무시
    if (pPlayer->m_nZoneID == pPkt->targetZone) return;

    CZone* pNewZone = CZone_Manager::Get_Instance()->GetZone(pPkt->targetZone);
    if (!pNewZone) return;

    CZone* pOldZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
    if (pOldZone) pOldZone->LeaveZone(pPlayer);

    // add 패킷이 오기 전에 클라가 새 맵 로드 + 로컬 플레이어 이동
    Send_SC_CHANGE_ZONE(pSession, pPkt->targetZone, pPkt->spawnX, pPkt->spawnZ);

    pNewZone->EnterZone(pPlayer, pPkt->spawnX, pPkt->spawnZ);
}

void CPacket_Handler::Send_SC_CHANGE_ZONE(
    std::shared_ptr<CSession> pSession,
    int32_t nZoneID, float fSpawnX, float fSpawnZ)
{
    SC_CHANGE_ZONE_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_CHANGE_ZONE;
    pkt.zoneID = nZoneID;
    pkt.spawnX = fSpawnX;
    pkt.spawnZ = fSpawnZ;
    pSession->Send(&pkt, sizeof(pkt));
}

void CPacket_Handler::Handle_CS_PICKUP(
    std::shared_ptr<CSession> pSession,
    uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_PICKUP_PACKET))) return;
    CS_PICKUP_PACKET* pPkt = reinterpret_cast<CS_PICKUP_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer) return;

    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
    if (!pZone) return;

    pZone->OnPlayerPickup(pPlayer, pPkt->dropId);
}

void CPacket_Handler::Handle_CS_EQUIP(
    std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_EQUIP_PACKET))) return;
    CS_EQUIP_PACKET* pPkt = reinterpret_cast<CS_EQUIP_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer) return;
    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
    if (!pZone) return;

    if (pPlayer->Equip(pPkt->invenSlot))
        pZone->Send_InvenUpdate(pPlayer);
}

void CPacket_Handler::Handle_CS_UNEQUIP(
    std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_UNEQUIP_PACKET))) return;
    CS_UNEQUIP_PACKET* pPkt = reinterpret_cast<CS_UNEQUIP_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer) return;
    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
    if (!pZone) return;

    if (pPlayer->UnEquip(pPkt->equipSlot))
        pZone->Send_InvenUpdate(pPlayer);
}

void CPacket_Handler::Handle_CS_USE_ITEM(
    std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_USE_ITEM_PACKET))) return;
    CS_USE_ITEM_PACKET* pPkt = reinterpret_cast<CS_USE_ITEM_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer) return;
    if (pPlayer->m_bDead) return;
    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
    if (!pZone) return;

    FUseResult result = pPlayer->UseItem(pPkt->invenSlot);
    if (result.used)
    {
        pZone->Send_InvenUpdate(pPlayer);  // 수량 차감 반영
        pZone->Send_PlayerHp(pPlayer);     // HP/MP 회복 반영

        if (result.buffType >= 0)          // 버프면 클라에 알림
        {
            SC_BUFF_PACKET pkt = {};
            pkt.header.size = sizeof(pkt);
            pkt.header.id = SC_BUFF;
            pkt.playerID = pPlayer->m_nPlayerID;
            pkt.buffType = result.buffType;
            pkt.durationMs = result.durationMs;
            pSession->Send(&pkt, sizeof(pkt));
        }
    }
}

// ================================================================
//  Handle_CS_MOVE_STOP  UI 진입 등으로 이동 강제 정지
//  서버가 현재 위치를 커밋하고 m_bMoving=false - 몬스터가 제 위치를 때림
// ================================================================
void CPacket_Handler::Handle_CS_MOVE_STOP(
    std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_MOVE_STOP_PACKET))) return;
    CS_MOVE_STOP_PACKET* pPkt = reinterpret_cast<CS_MOVE_STOP_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer) return;
    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
    if (!pZone) return;

    pZone->OnMoveStop(pPlayer, pPkt->fCurX, pPkt->fCurZ);
}

// ================================================================
//  Handle_CS_BUY  상점 구매 (현재 포션만)
//  골드 검증 - 인벤 추가 - 골드 차감 - SC_INVEN_UPDATE
// ================================================================
void CPacket_Handler::Handle_CS_BUY(
    std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_BUY_PACKET))) return;
    CS_BUY_PACKET* pPkt = reinterpret_cast<CS_BUY_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer) return;
    if (pPlayer->m_bDead) return;
    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
    if (!pZone) return;

    int32_t nCount = pPkt->count;
    if (nCount <= 0 || nCount > 99) return;

    int32_t nUnit = PotionBuyPrice(pPkt->itemCode);   // 0이면 판매목록 아님
    if (nUnit <= 0) return;

    int32_t nTotal = nUnit * nCount;
    if (pPlayer->m_gold < nTotal) return;             // 골드 부족

    if (!pPlayer->AddItem(pPkt->itemCode, nCount)) return;  // 인벤 가득
    pPlayer->SpendGold(nTotal);

    pZone->Send_InvenUpdate(pPlayer);
}

// ================================================================
//  Handle_CS_SELL  상점 판매 (현재 포션만)
//  인벤 차감 - 골드 지급 - SC_INVEN_UPDATE
// ================================================================
void CPacket_Handler::Handle_CS_SELL(
    std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_SELL_PACKET))) return;
    CS_SELL_PACKET* pPkt = reinterpret_cast<CS_SELL_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer) return;
    if (pPlayer->m_bDead) return;
    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
    if (!pZone) return;

    int32_t nSlot = pPkt->invenSlot;
    if (nSlot < 0 || nSlot >= CPlayer::INVEN_SIZE) return;

    int32_t nCount = pPkt->count;
    if (nCount <= 0) return;

    int32_t nCode = pPlayer->m_invenCode[nSlot];
    int32_t nUnit = ItemSellPrice(nCode);             // 0이면 판매 불가
    if (nUnit <= 0) return;

    if (!pPlayer->RemoveItemSlot(nSlot, nCount)) return;  // 수량 부족
    pPlayer->AddGold(nUnit * nCount);

    pZone->Send_InvenUpdate(pPlayer);
}

// ================================================================
//  경매장 (즉시구매 / 개당가격 / 부분구매)
//  응답: SC_AUCTION_LIST(스냅샷) + 인벤 변화 시 SC_INVEN_UPDATE 재사용
// ================================================================
void CPacket_Handler::Send_SC_AUCTION_LIST(std::shared_ptr<CSession> pSession, int32_t page,
    int32_t tab, const int32_t* searchCodes, int32_t searchCount)
{
    if (!pSession) return;
    if (page < 0) page = 0;

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    const char* myName = pPlayer ? pPlayer->m_szName : "";

    SC_AUCTION_LIST_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_AUCTION_LIST;
    pkt.page = page;

    bool bHasNext = false;
    int32_t n = CDB_Manager::Get_Instance()->Auction_GetPage(
        page, tab, myName, searchCodes, searchCount, pkt.entries, bHasNext);
    pkt.count = n;
    pkt.hasNext = bHasNext ? 1 : 0;
    pSession->Send(&pkt, sizeof(pkt));
}

void CPacket_Handler::Handle_CS_AUCTION_LIST(
    std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_AUCTION_LIST_PACKET))) return;
    CS_AUCTION_LIST_PACKET* pPkt = reinterpret_cast<CS_AUCTION_LIST_PACKET*>(pBuffer);
    int32_t sc = pPkt->searchCount;
    if (sc < 0) sc = 0;
    if (sc > AUCTION_SEARCH_MAX) sc = AUCTION_SEARCH_MAX;
    Send_SC_AUCTION_LIST(pSession, pPkt->page, pPkt->tab, pPkt->searchCodes, sc);
}

void CPacket_Handler::Handle_CS_AUCTION_REGISTER(
    std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_AUCTION_REGISTER_PACKET))) return;
    CS_AUCTION_REGISTER_PACKET* pPkt = reinterpret_cast<CS_AUCTION_REGISTER_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer || pPlayer->m_bDead) return;
    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
    if (!pZone) return;

    int32_t nSlot = pPkt->invenSlot;
    if (nSlot < 0 || nSlot >= CPlayer::INVEN_SIZE) return;
    int32_t nCount = pPkt->count;
    int32_t nPrice = pPkt->unitPrice;
    if (nCount <= 0 || nPrice <= 0) return;

    int32_t nCode = pPlayer->m_invenCode[nSlot];
    if (nCode <= 0) return;
    if (pPlayer->m_invenCount[nSlot] < nCount) return;

    // 인벤에서 빼서(에스크로) 매물 등록 (DB INSERT)
    if (!CDB_Manager::Get_Instance()->Auction_Register(pPlayer->m_szName, nCode, nCount, nPrice))
        return;
    pPlayer->RemoveItemSlot(nSlot, nCount);

    pZone->Send_InvenUpdate(pPlayer);
    // 등록 후 목록 갱신은 클라가 재요청(보통 0페이지로 이동해 새 매물 확인).
}

void CPacket_Handler::Handle_CS_AUCTION_BUY(
    std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_AUCTION_BUY_PACKET))) return;
    CS_AUCTION_BUY_PACKET* pPkt = reinterpret_cast<CS_AUCTION_BUY_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer || pPlayer->m_bDead) return;
    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
    if (!pZone) return;

    int32_t nQty = pPkt->count;
    if (nQty <= 0 || nQty > 99) return;

    CDB_Manager* pDB = CDB_Manager::Get_Instance();

    // 1) 사전조회(비변경): 코드/개당가. 매물 없음/본인매물이면 중단.
    int32_t nCode = 0, nUnit = 0;
    if (!pDB->Auction_PeekBuy(pPkt->listingID, pPlayer->m_szName, nCode, nUnit))
        return;
    int32_t nTotal = nUnit * nQty;

    // 2) 지급 가능성 사전 확인(골드 + 인벤 여유). 여기서 막아야 원자 UPDATE 후 지급 실패가 없음.
    if (pPlayer->m_gold < nTotal) return;
    if (!pPlayer->CanAddItem(nCode, nQty)) return;

    // 3) 매물 확정: 원자적 조건부 UPDATE. 동시 구매 시 여기서 한 명만 성공(1행), 나머지 거부(0행).
    //    - 이 단계가 성공한 뒤에만 아이템 지급 - 복사/오버셀 불가.
    if (!pDB->Auction_CommitBuy(pPkt->listingID, nQty, nTotal, pPlayer->m_szName))
        return;   // 경쟁에서 밀렸거나 수량 부족 - 아무것도 안 준 상태로 종료

    // 4) 확정됐으니 지급(사전 확인했으므로 성공 보장) + 골드 차감
    pPlayer->AddItem(nCode, nQty);
    pPlayer->SpendGold(nTotal);

    pZone->Send_InvenUpdate(pPlayer);
    // 경매 목록 갱신은 클라가 현재 탭/페이지/검색으로 재요청(CS_AUCTION_LIST)한다.
}

void CPacket_Handler::Handle_CS_AUCTION_COLLECT(
    std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_AUCTION_COLLECT_PACKET))) return;
    CS_AUCTION_COLLECT_PACKET* pPkt = reinterpret_cast<CS_AUCTION_COLLECT_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer || pPlayer->m_bDead) return;
    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
    if (!pZone) return;

    int32_t nGold = 0;
    if (!CDB_Manager::Get_Instance()->Auction_Collect(pPkt->listingID, pPlayer->m_szName, nGold))
        return;

    pPlayer->AddGold(nGold);
    pZone->Send_InvenUpdate(pPlayer);
    // 경매 목록 갱신은 클라가 현재 탭/페이지/검색으로 재요청(CS_AUCTION_LIST)한다.
}

void CPacket_Handler::Handle_CS_AUCTION_CANCEL(
    std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_AUCTION_CANCEL_PACKET))) return;
    CS_AUCTION_CANCEL_PACKET* pPkt = reinterpret_cast<CS_AUCTION_CANCEL_PACKET*>(pBuffer);

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(pSession->GetID());
    if (!pPlayer || pPlayer->m_bDead) return;
    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
    if (!pZone) return;

    CDB_Manager* pDB = CDB_Manager::Get_Instance();

    // 1) 사전조회(비변경): 코드/남은수량/미수령골드. 본인 매물 아니면 중단.
    int32_t nCode = 0, nCount = 0, nGold = 0;
    if (!pDB->Auction_PeekCancel(pPkt->listingID, pPlayer->m_szName, nCode, nCount, nGold))
        return;

    // 2) 남은 수량을 인벤에 담을 수 있는지 먼저 확인(DELETE 후 지급 실패로 아이템 소실 방지)
    if (nCount > 0 && !pPlayer->CanAddItem(nCode, nCount)) return;

    // 3) 매물 삭제(원자적). 삭제 시점의 실제 수량/골드를 받아 그대로 지급.
    //    동시 구매가 있었다면 실제 수량은 조회 때보다 적을 수 있음(<= 이므로 인벤에 반드시 들어감).
    int32_t aCode = 0, aCount = 0, aGold = 0;
    if (!pDB->Auction_Cancel(pPkt->listingID, pPlayer->m_szName, aCode, aCount, aGold))
        return;

    if (aCount > 0) pPlayer->AddItem(aCode, aCount);   // 남은 수량 반환
    if (aGold  > 0) pPlayer->AddGold(aGold);           // 미수령 골드 지급

    pZone->Send_InvenUpdate(pPlayer);
    // 경매 목록 갱신은 클라가 현재 탭/페이지/검색으로 재요청(CS_AUCTION_LIST)한다.
}
