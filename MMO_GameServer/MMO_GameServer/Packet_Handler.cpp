#include "pch.h"
#include "Packet_Handler.h"
#include "Session.h"
#include "Session_Manager.h"
#include "Player_Manager.h"
#include "Zone_Manager.h"
#include "Protocol.h"
#include "AccountDB.h"
#include "AuctionManager.h"
#include "ServerItem.h"

void CPacket_Handler::Handle(std::shared_ptr<CSession> pSession,
    uint8_t* pBuffer, int32_t nSize)
{
    PacketHeader* pHeader = reinterpret_cast<PacketHeader*>(pBuffer);

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
    default:
        std::cout << "[CPacket_Handler] 알 수 없는 패킷: "
            << pHeader->id << std::endl;
        break;
    }
}

void CPacket_Handler::Handle_CS_LOGIN(std::shared_ptr<CSession> pSession,
    uint8_t* pBuffer, int32_t nSize)
{
    if (nSize < static_cast<int32_t>(sizeof(CS_LOGIN_PACKET))) return;
    CS_LOGIN_PACKET* pPkt = reinterpret_cast<CS_LOGIN_PACKET*>(pBuffer);

    // 널 종단 보장 (클라가 꽉 채워 보냈을 경우 대비)
    pPkt->id[sizeof(pPkt->id) - 1] = '\0';
    pPkt->pw[sizeof(pPkt->pw) - 1] = '\0';

    // ---- 인증 (DB 대체: AccountDB. 추후 DB 조회로 교체) ----
    const FAccountData* pAcc = FindAccount(pPkt->id, pPkt->pw);
    if (!pAcc) { Send_SC_LOGIN_FAIL(pSession, 1); return; }   // 1 = 아이디/비번 불일치

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Create(pSession->GetID());
    if (!pPlayer) { Send_SC_LOGIN_FAIL(pSession, 0); return; }

    strncpy_s(pPlayer->m_szName, pAcc->id, sizeof(pPlayer->m_szName) - 1);

    // ---- 계정 데이터 로드 (DB 없으므로 매번 동일: 골드/인벤/장비) ----
    // 추후 DB 붙이면 이 블록을 "DB에서 로드"로 교체.
    pPlayer->m_gold = pAcc->gold;
    for (const FSaveItem& it : pAcc->inven)
    {
        if (it.code == 0) break;
        pPlayer->AddItem(it.code, it.count);
    }
    for (int i = 0; i < CPlayer::EQUIP_SLOTS; ++i)
        pPlayer->m_equipCode[i] = pAcc->equip[i];

    // ---- 계정에 저장된 존/위치로 입장 ----
    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pAcc->zoneID);
    if (!pZone) { Send_SC_LOGIN_FAIL(pSession, 0); return; }

    pZone->EnterZone(pPlayer, pAcc->spawnX, pAcc->spawnZ);

    Send_SC_LOGIN_OK(pSession, pPlayer->m_nPlayerID);
    Send_SC_ENTER_GAME(pSession);

    // 골드/인벤/장비를 클라에 1회 동기화 (ENTER_GAME 뒤 → 클라 플레이어 생성 후 도착)
    pZone->Send_InvenUpdate(pPlayer);
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
//  서버가 현재 위치를 커밋하고 m_bMoving=false → 몬스터가 제 위치를 때림
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
//  골드 검증 → 인벤 추가 → 골드 차감 → SC_INVEN_UPDATE
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
//  인벤 차감 → 골드 지급 → SC_INVEN_UPDATE
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
void CPacket_Handler::Send_SC_AUCTION_LIST(std::shared_ptr<CSession> pSession)
{
    if (!pSession) return;
    SC_AUCTION_LIST_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = SC_AUCTION_LIST;
    CAuction_Manager::Get_Instance()->Fill_Snapshot(pkt);
    pSession->Send(&pkt, sizeof(pkt));
}

void CPacket_Handler::Handle_CS_AUCTION_LIST(
    std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize)
{
    Send_SC_AUCTION_LIST(pSession);
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

    // 인벤에서 빼서(에스크로) 매물 등록
    if (!CAuction_Manager::Get_Instance()->Register(pPlayer->m_szName, nCode, nCount, nPrice))
        return;
    pPlayer->RemoveItemSlot(nSlot, nCount);

    pZone->Send_InvenUpdate(pPlayer);
    Send_SC_AUCTION_LIST(pSession);
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

    CAuction_Manager* pAuc = CAuction_Manager::Get_Instance();

    // 1) 비변경 검증 → 코드/총액
    int32_t nCode = 0, nTotal = 0;
    if (!pAuc->Peek_Buy(pPkt->listingID, nQty, pPlayer->m_szName, nCode, nTotal))
        return;

    // 2) 골드 확인
    if (pPlayer->m_gold < nTotal) return;

    // 3) 인벤 수용(가득이면 실패 → 변화 없음)
    if (!pPlayer->AddItem(nCode, nQty)) return;

    // 4) 매물 확정(경쟁 상태 극히 드묾: 실패 시에도 아이템은 지급된 상태이나
    //    이 게임 규모에선 사실상 발생하지 않음)
    if (!pAuc->Commit_Buy(pPkt->listingID, nQty, pPlayer->m_szName)) return;

    // 5) 골드 차감
    pPlayer->SpendGold(nTotal);

    pZone->Send_InvenUpdate(pPlayer);
    Send_SC_AUCTION_LIST(pSession);
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
    if (!CAuction_Manager::Get_Instance()->Collect(pPkt->listingID, pPlayer->m_szName, nGold))
        return;

    pPlayer->AddGold(nGold);
    pZone->Send_InvenUpdate(pPlayer);
    Send_SC_AUCTION_LIST(pSession);
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

    CAuction_Manager* pAuc = CAuction_Manager::Get_Instance();

    // 1) 취소 정보(남은수량/미수령골드) 조회
    int32_t nCode = 0, nCount = 0, nGold = 0;
    if (!pAuc->Peek_Cancel(pPkt->listingID, pPlayer->m_szName, nCode, nCount, nGold))
        return;

    // 2) 남은 수량 인벤 반환 (가득이면 취소 불가 → 변화 없음)
    if (nCount > 0)
        if (!pPlayer->AddItem(nCode, nCount)) return;

    // 3) 미수령 골드 지급
    if (nGold > 0) pPlayer->AddGold(nGold);

    // 4) 매물 제거
    pAuc->Remove_Listing(pPkt->listingID, pPlayer->m_szName);

    pZone->Send_InvenUpdate(pPlayer);
    Send_SC_AUCTION_LIST(pSession);
}
