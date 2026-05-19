#include "pch.h"
#include "Session.h"
#include "Packet_Handler.h"
#include "Session_Manager.h"
#include "Player_Manager.h"
#include "Zone_Manager.h"
#include "Protocol.h"
#include <iostream>
#include <cstring>

CSession::CSession() {

}
CSession::~CSession() {}

void CSession::Initialize()
{
    m_recvEvent.m_owner = this;
    m_sendEvent.m_owner = this;
    m_acceptEvent.m_owner = this;
    m_prevRemain = 0;
    m_connected = false;

    // 버퍼 초기화
    ZeroMemory(m_recvBuf, sizeof(m_recvBuf));
    ZeroMemory(m_sendBuf, sizeof(m_sendBuf));
}

void CSession::RegisterRecv()
{
    if (!m_connected) return;

    m_recvEvent.Reset();

    WSABUF wsaBuf;
    wsaBuf.buf = reinterpret_cast<char*>(m_recvBuf) + m_prevRemain;
    wsaBuf.len = RECV_BUF_SIZE - m_prevRemain;

    DWORD dwFlags = 0;
    DWORD dwRecvBytes = 0;

    int nResult = WSARecv(
        m_socket,
        &wsaBuf, 1,
        &dwRecvBytes,
        &dwFlags,
        &m_recvEvent.m_overlapped,
        nullptr
    );

    if (nResult == SOCKET_ERROR)
    {
        int nErr = WSAGetLastError();
        if (nErr != WSA_IO_PENDING)
        {
            std::cout << "[CSession] WSARecv 오류: " << nErr << std::endl;
            Disconnect();
        }
    }
}

void CSession::Send(void* pPacket, int32_t nSize)
{
    if (!m_connected) return;

    std::lock_guard<std::mutex> lock(m_sendLock);

    m_sendEvent.Reset();

    memcpy(m_sendBuf, pPacket, nSize);

    WSABUF wsaBuf;
    wsaBuf.buf = reinterpret_cast<char*>(m_sendBuf);
    wsaBuf.len = nSize;

    DWORD dwSentBytes = 0;

    int nResult = WSASend(
        m_socket,
        &wsaBuf, 1,
        &dwSentBytes,
        0,
        &m_sendEvent.m_overlapped,
        nullptr
    );

    if (nResult == SOCKET_ERROR)
    {
        int nErr = WSAGetLastError();
        if (nErr != WSA_IO_PENDING)
        {
            std::cout << "[CSession] WSASend 오류: " << nErr << std::endl;
            Disconnect();
        }
    }
}

void CSession::Disconnect()
{
    if (m_connected.exchange(false) == false) return;

    std::cout << "[CSession] 연결 해제. ID=" << m_id << std::endl;

    {
        PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(m_id);
        if (pPlayer)
        {
            CZone* pZone = CZone_Manager::Get_Instance()->GetZone(pPlayer->m_nZoneID);
            if (pZone)
                pZone->LeaveZone(pPlayer);
        }
    }
    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
    CPlayer_Manager::Get_Instance()->Remove(m_id);
    CSession_Manager::Get_Instance()->Release(m_id);  

    
}

void CSession::OnRecvComplete(int32_t nNumOfBytes)
{
    ProcessRecvData(nNumOfBytes);
    RegisterRecv();
}

void CSession::OnSendComplete()
{
    // 나중에 SendQueue 확장 시 여기서 다음 패킷 전송
}

void CSession::ProcessRecvData(int32_t nNewBytes)
{
    int32_t  nRemainData = m_prevRemain + nNewBytes;
    uint8_t* pCursor = m_recvBuf;

    while (nRemainData > 0)
    {
        if (nRemainData < static_cast<int32_t>(sizeof(PacketHeader)))
            break;

        PacketHeader* pHeader = reinterpret_cast<PacketHeader*>(pCursor);

        if (nRemainData < pHeader->size)
            break;

        HandlePacket(pCursor, pHeader->size);

        pCursor += pHeader->size;
        nRemainData -= pHeader->size;
    }

    m_prevRemain = nRemainData;
    if (nRemainData > 0)
        memmove(m_recvBuf, pCursor, nRemainData);
}

void CSession::HandlePacket(uint8_t* pData, int32_t nSize)
{
    CPacket_Handler::Handle(shared_from_this(), pData, nSize);
}