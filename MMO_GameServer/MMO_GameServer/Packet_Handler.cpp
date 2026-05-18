#include "pch.h"
#include "Packet_Handler.h"
#include "Session.h"
#include "Session_Manager.h"
#include "Player_Manager.h"
#include "Zone_Manager.h"
#include "Protocol.h"

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

    //std::cout << "[LOGIN] ID=" << pPkt->id
    //    << " SessionID=" << pSession->GetID() << std::endl;

    PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Create(pSession->GetID());
    if (!pPlayer) { Send_SC_LOGIN_FAIL(pSession, 0); return; }

    strncpy_s(pPlayer->m_szName, pPkt->id, sizeof(pPlayer->m_szName) - 1);

    CZone* pZone = CZone_Manager::Get_Instance()->GetZone(ZONE_TEST);
    if (!pZone) { Send_SC_LOGIN_FAIL(pSession, 0); return; }

    pZone->EnterZone(pPlayer, 10.f, 10.f);

    Send_SC_LOGIN_OK(pSession, pPlayer->m_nPlayerID);
    Send_SC_ENTER_GAME(pSession);
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

