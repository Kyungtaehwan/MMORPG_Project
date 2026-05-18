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

    static void Send_SC_LOGIN_OK(std::shared_ptr<CSession> pSession, uint32_t nPlayerID);
    static void Send_SC_LOGIN_FAIL(std::shared_ptr<CSession> pSession, uint8_t nReason);
    static void Send_SC_ENTER_GAME(std::shared_ptr<CSession> pSession);
};