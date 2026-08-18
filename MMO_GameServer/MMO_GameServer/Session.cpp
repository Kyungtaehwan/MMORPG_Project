#include "pch.h"
#include "Session.h"
#include "Packet_Handler.h"
#include "Session_Manager.h"
#include "Player_Manager.h"
#include "Zone_Manager.h"
#include "DB_Manager.h"
#include "Protocol.h"
#include <iostream>
#include <cstring>
#include "StressMetrics.h"

CSession::CSession() {

}
CSession::~CSession() {}

void CSession::Initialize()
{
    m_recvEvent.m_owner = this;
    m_sendEvent.m_owner = this;
    m_acceptEvent.m_owner = this;
    // 이번트의 오너를 나로 설정

    m_prevRemain = 0;
    // 수신 버퍼 잔여물 지우기

    m_connected = false;
    // 아직 연결X

    ZeroMemory(m_recvBuf, sizeof(m_recvBuf));
#if !USE_SEND_BUFFER
    ZeroMemory(m_sendBuf, sizeof(m_sendBuf));
#endif
    // 버퍼 초기화
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
    if (nSize <= 0 || nSize > SEND_BUF_SIZE) return;

#if USE_SEND_BUFFER
    // 수신자가 한 명뿐인 경로. 여기서 버퍼를 만들어 아래 공용 경로로 넘긴다.
    Send(MakeSendBuffer(pPacket, nSize));
    return;
#else
    std::lock_guard<std::mutex> lock(m_sendLock);

    // 큐에 적재. 송신 중이면 이전 전송 완료 후 순차 전송된다.
    m_sendQueue.emplace(
        reinterpret_cast<uint8_t*>(pPacket),
        reinterpret_cast<uint8_t*>(pPacket) + nSize);
    StressMetrics::RecordSendQueue(m_sendQueue.size());

    if (!m_sending)
    {
        m_sending = true;
        StartSend_Locked();
    }
#endif
}

// 브로드캐스트 경로. 이미 만들어진 페이로드를 받는다.
void CSession::Send(const SendPayload& payload)
{
#if USE_SEND_BUFFER
    if (!m_connected) return;
    if (!payload) return;
    if (payload->Size() <= 0 || payload->Size() > SEND_BUF_SIZE) return;

    std::lock_guard<std::mutex> lock(m_sendLock);

    // 바이트열을 복사하지 않는다. 참조 하나만 큐에 얹는다.
    m_sendQueue.push(payload);
    StressMetrics::RecordSendQueue(m_sendQueue.size());

    if (!m_sending)
    {
        m_sending = true;
        StartSend_Locked();
    }
#else
    // 기준선: 예전과 완전히 같은 경로로 흘려보낸다.
    Send(const_cast<void*>(payload.pData), payload.nSize);
#endif
}

// m_sendLock 보유 상태에서 호출. 큐 앞 패킷 1개를 m_sendBuf로 복사해 WSASend.
// (USE_SEND_BUFFER 가 켜져 있으면 복사 없이 공유 버퍼를 직접 가리킨다)
void CSession::StartSend_Locked()
{
    if (m_sendQueue.empty())
    {
        m_sending = false;
#if USE_SEND_BUFFER
        m_sendingBuf.reset();
#endif
        return;
    }

    WSABUF wsaBuf;

#if USE_SEND_BUFFER

    // 큐에서 꺼낸 참조를 m_sendingBuf 로 잡는다
    m_sendingBuf = m_sendQueue.front();
    m_sendQueue.pop();
    m_sendEvent.Reset();

    wsaBuf.buf = reinterpret_cast<char*>(const_cast<uint8_t*>(m_sendingBuf->Data()));
    wsaBuf.len = m_sendingBuf->Size();
#else
    std::vector<uint8_t>& front = m_sendQueue.front();
    int32_t nSize = static_cast<int32_t>(front.size());
    memcpy(m_sendBuf, front.data(), nSize);
    m_sendQueue.pop();

    m_sendEvent.Reset();

    wsaBuf.buf = reinterpret_cast<char*>(m_sendBuf);
    wsaBuf.len = nSize;
#endif

    DWORD dwSentBytes = 0;
    int nResult = WSASend(
        m_socket, &wsaBuf, 1, &dwSentBytes, 0,
        &m_sendEvent.m_overlapped, nullptr);

    if (nResult == SOCKET_ERROR)
    {
        int nErr = WSAGetLastError();
        if (nErr != WSA_IO_PENDING)
        {
            std::cout << "[CSession] WSASend 오류: " << nErr << std::endl;
            m_sending = false;
#if USE_SEND_BUFFER
            m_sendingBuf.reset();
#endif
            Disconnect();
        }
    }
}

void CSession::Disconnect()
{
    if (m_connected.exchange(false) == false) return;

    // exchange를 통과한 스레드는 하나뿐이므로 여기서 딱 한 번만 줄어든다.
    CSession_Manager::Get_Instance()->OnDisconnected();

    std::cout << "[CSession] 연결 해제. ID=" << m_id << std::endl;

    {
        PlayerRef pPlayer = CPlayer_Manager::Get_Instance()->Get_Player(m_id);
        if (pPlayer)
        {
            // 로그아웃 저장: 현재 상태(존/위치/골드/인벤/장비)를 DB에 기록.
            // (이름이 있어야 = 로그인 성공한 플레이어만 저장 대상)
            if (pPlayer->m_szName[0] != '\0')
            {
                // 접속종료 저장: 스냅샷을 저장 전용 스레드에 넘김.
                // (동기 저장이 워커를 블로킹하던 것이 대량 disconnect 시 크래시 원인이었음)
                FSaveSnapshot snap;
                pPlayer->TakeSnapshot(snap);
                CDB_Manager::Get_Instance()->EnqueueSave(snap);
            }

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
    // 이전 전송 완료 - 큐에 남은 다음 패킷 전송
    std::lock_guard<std::mutex> lock(m_sendLock);
#if USE_SEND_BUFFER
    // 커널이 다 읽었으므로 이제 놓아준다. 마지막 참조였다면 여기서 해제된다.
    m_sendingBuf.reset();
#endif
    StartSend_Locked();
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

        // 헤더의 size는 클라가 보낸 값이라 그대로 믿으면 X
        // size가 수신버퍼보다 크면 영원히 조립이 끝나지 않는다
        if (pHeader->size < sizeof(PacketHeader) ||
            pHeader->size > RECV_BUF_SIZE)
        {
            std::cout << "[CSession] 비정상 패킷 크기=" << pHeader->size
                << " ID=" << m_id << " - 연결 종료" << std::endl;
            Disconnect();
            return;
        }

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