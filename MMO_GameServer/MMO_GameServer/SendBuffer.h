#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "ServerConfig.h"

// ================================================================
//  송신 공유 버퍼
//
//  ★ 무엇을 없애려는 것인가
//    브로드캐스트는 같은 바이트열을 시야 안 N 명에게 보낸다.
//    그런데 지금까지는 수신자마다
//        패킷 구조체를 다시 채우고 -> vector 를 새로 할당하고 -> 복사
//    를 반복했다. 내용이 똑같은데 N 번 만들고 있었던 것이다.
//
//    공유 버퍼는 그 바이트열을 한 번만 만들어 shared_ptr 로 들고,
//    N 명에게는 참조만 넘긴다. 할당과 복사가 N -> 1 이 된다.
//
//  ★ 전송 횟수는 줄지 않는다
//    이것은 "모아서 한 번에 보내는" 최적화가 아니다.
//    WSASend 호출 수는 그대로다. 그건 다음 단계인 송신 배칭의 몫이고,
//    큐가 참조를 담게 되는 이 변경이 그 배칭의 전제 조건이다.
//
//  ★ 수명 규칙 (이 최적화에서 크래시가 나는 지점)
//    WSASend 는 비동기다. 완료 통지가 올 때까지 버퍼가 살아 있어야 한다.
//    그래서 CSession 은 전송 중인 버퍼를 m_sendingBuf 로 붙잡고 있다가
//    OnSendComplete 에서 놓는다. 큐에서 pop 하자마자 버리면
//    아직 커널이 읽고 있는 메모리가 해제된다.
// ================================================================

#if USE_SEND_BUFFER

class FSendBuffer
{
public:
    FSendBuffer(const void* pSrc, int32_t nSize)
        : m_data(static_cast<const uint8_t*>(pSrc),
                 static_cast<const uint8_t*>(pSrc) + nSize)
    {
    }

    const uint8_t* Data() const { return m_data.data(); }
    int32_t        Size() const { return static_cast<int32_t>(m_data.size()); }

private:
    std::vector<uint8_t> m_data;
};

using SendBufferRef = std::shared_ptr<FSendBuffer>;

inline SendBufferRef MakeSendBuffer(const void* pSrc, int32_t nSize)
{
    return std::make_shared<FSendBuffer>(pSrc, nSize);
}

// 브로드캐스트 호출부가 #if 로 갈라지지 않도록 하는 별칭.
// 락을 FRWLock 하나로 감싼 것과 같은 방식이다.
using SendPayload = SendBufferRef;

#define MAKE_SEND_PAYLOAD(pkt) \
    MakeSendBuffer(&(pkt), static_cast<int32_t>(sizeof(pkt)))

#else   // USE_SEND_BUFFER == 0  (기준선)

// 꺼져 있을 때는 패킷을 가리키기만 한다. 복사도 할당도 하지 않으므로
// 호출부 모양만 같아지고 동작은 예전 그대로 CSession::Send(void*, int32_t) 다.
struct FRawPayload
{
    const void* pData;
    int32_t     nSize;
};

using SendPayload = FRawPayload;

#define MAKE_SEND_PAYLOAD(pkt) \
    FRawPayload{ &(pkt), static_cast<int32_t>(sizeof(pkt)) }

#endif
