#pragma once
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <MSWSock.h>
#include <Windows.h>

#include <cstdint>
#include <atomic>
#include <mutex>
#include <memory>
#include <queue>
#include <vector>

#include "SendBuffer.h"

enum class IOType : uint8_t
{
    Accept,
    Recv,
    Send,
    MonsterAI,
    MonsterRespawn,
    MonsterAttackHit,
    PlayerAutoSave
};


class CSession;
// ----------------------------------------------------------------
//  CIOEvent WSAOVERLAPPED 확장체
//  WSAOVERLAPPED가 반드시 첫 번째 멤버여야 함
// ----------------------------------------------------------------
struct CIOEvent
{
    WSAOVERLAPPED m_overlapped = {};
    IOType        m_type;

    CSession* m_owner = nullptr;

    uint8_t m_acceptBuf[(sizeof(SOCKADDR_IN) + 16) * 2] = {};

    CIOEvent(IOType t) : m_type(t), m_owner(nullptr)
    {
        ZeroMemory(&m_overlapped, sizeof(m_overlapped));
        ZeroMemory(m_acceptBuf, sizeof(m_acceptBuf));

    }

    void Reset()
    {
        ZeroMemory(&m_overlapped, sizeof(m_overlapped));
    }
};


class CSession : public std::enable_shared_from_this<CSession>
{
    static constexpr int32_t RECV_BUF_SIZE = 4096;
    static constexpr int32_t SEND_BUF_SIZE = 4096;

public:
    CSession();
    ~CSession();

    // ---- 외부에서 호출 ----
    void        Initialize();
    void        SetSocket(SOCKET socket) { m_socket = socket; }
    SOCKET      GetSocket() { return m_socket; }
    void        SetID(int32_t id) { m_id = id; }
    int32_t     GetID() { return m_id; }
    bool        IsConnected() { return m_connected; }

    void        RegisterRecv();
    void        Send(void* pPacket, int32_t nSize);
    // 브로드캐스트용. 이미 만들어진 페이로드를 그대로 받는다.
    // USE_SEND_BUFFER 가 켜져 있으면 참조만 큐에 넣고, 꺼져 있으면
    // 위의 Send(void*, int32_t) 로 그대로 넘어간다.
    void        Send(const SendPayload& payload);
    void        Disconnect();

    void        OnRecvComplete(int32_t nNumOfBytes);
    void        OnSendComplete();
    uint8_t*    GetAcceptBuf() { return m_acceptEvent.m_acceptBuf; }
    CIOEvent*   GetAcceptEvent() { return &m_acceptEvent; }
    void        SetConnected(bool b) { m_connected = b; }
private:
    void        ProcessRecvData(int32_t nNewBytes);
    void        HandlePacket(uint8_t* pData, int32_t nSize);
    void        StartSend_Locked();   // m_sendLock 보유 상태에서 큐 앞 패킷 1개 전송

private:
    SOCKET               m_socket = INVALID_SOCKET;
    int32_t              m_id = -1;
    std::atomic<bool>    m_connected = false;

    // recv
    CIOEvent             m_recvEvent{ IOType::Recv };
    uint8_t              m_recvBuf[RECV_BUF_SIZE] = {};
    int32_t              m_prevRemain = 0;

    // send (세션당 순차 전송: 앞 패킷 완료 후 다음 전송 — 버퍼/오버랩 재사용 안전)
    CIOEvent             m_sendEvent{ IOType::Send };
    std::mutex           m_sendLock;
    bool                 m_sending = false;

#if USE_SEND_BUFFER
    // 공유 버퍼 모드: 큐가 바이트열이 아니라 참조를 담는다.
    // WSASend 가 공유 버퍼의 메모리를 직접 가리키므로 m_sendBuf 로의
    // 복사가 통째로 사라진다. 대신 완료 통지가 올 때까지 그 버퍼가
    // 살아 있어야 해서 m_sendingBuf 가 붙잡고 있는다.
    std::queue<SendBufferRef> m_sendQueue;
    SendBufferRef             m_sendingBuf;
#else
    uint8_t              m_sendBuf[SEND_BUF_SIZE] = {};
    std::queue<std::vector<uint8_t>> m_sendQueue;
#endif

    // accept
    CIOEvent             m_acceptEvent{ IOType::Accept };


};

using SessionRef = std::shared_ptr<CSession>;