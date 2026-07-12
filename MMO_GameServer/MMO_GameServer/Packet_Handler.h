#pragma once
#include "Protocol.h"
#include <memory>

class CSession;

class CPacket_Handler
{
public:
    static void Handle(std::shared_ptr<CSession> pSession,
        uint8_t* pBuffer, int32_t nSize);

private:
    static void Handle_CS_LOGIN(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_MOVE_DEST(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_MOVE_POS(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_ATTACK_MONSTER(std::shared_ptr<CSession> pSession,uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_RESPAWN(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_PORTAL(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_PICKUP(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_EQUIP(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_UNEQUIP(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_USE_ITEM(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_MOVE_STOP(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_BUY(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_SELL(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);

    // 경매장
    static void Handle_CS_AUCTION_LIST(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_AUCTION_REGISTER(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_AUCTION_BUY(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_AUCTION_COLLECT(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Handle_CS_AUCTION_CANCEL(std::shared_ptr<CSession> pSession, uint8_t* pBuffer, int32_t nSize);
    static void Send_SC_AUCTION_LIST(std::shared_ptr<CSession> pSession, int32_t page,
        int32_t tab, const int32_t* searchCodes, int32_t searchCount);

    static void Send_SC_LOGIN_OK(std::shared_ptr<CSession> pSession, uint32_t nPlayerID);
    static void Send_SC_LOGIN_FAIL(std::shared_ptr<CSession> pSession, uint8_t nReason);
    static void Send_SC_ENTER_GAME(std::shared_ptr<CSession> pSession);
    static void Send_SC_CHANGE_ZONE(std::shared_ptr<CSession> pSession, int32_t nZoneID, float fSpawnX, float fSpawnZ);
};