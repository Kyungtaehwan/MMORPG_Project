#pragma once
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <MSWSock.h>
#include <Windows.h>

#include <cstdint>
#include <atomic>
#include <mutex>
#include <memory>

enum class IOType : uint8_t
{
    Accept,
    Recv,
    Send,
    MonsterAI,
    MonsterRespawn,
    MonsterAttackHit
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
    void        Disconnect();

    void        OnRecvComplete(int32_t nNumOfBytes);
    void        OnSendComplete();
    uint8_t*    GetAcceptBuf() { return m_acceptEvent.m_acceptBuf; }
    CIOEvent*   GetAcceptEvent() { return &m_acceptEvent; }
    void        SetConnected(bool b) { m_connected = b; }
private:
    void        ProcessRecvData(int32_t nNewBytes);
    void        HandlePacket(uint8_t* pData, int32_t nSize);

private:
    SOCKET               m_socket = INVALID_SOCKET;
    int32_t              m_id = -1;
    std::atomic<bool>    m_connected = false;

    // recv
    CIOEvent             m_recvEvent{ IOType::Recv };
    uint8_t              m_recvBuf[RECV_BUF_SIZE] = {};
    int32_t              m_prevRemain = 0;

    // send
    CIOEvent             m_sendEvent{ IOType::Send };
    uint8_t              m_sendBuf[SEND_BUF_SIZE] = {};
    std::mutex           m_sendLock;

    // accept
    CIOEvent             m_acceptEvent{ IOType::Accept };


};

using SessionRef = std::shared_ptr<CSession>;