#include "pch.h"
#include "Network_Manager.h"
#include <iostream>
#include "Object_Manager.h"   // 다른 플레이어 오브젝트 관리
#include "Other_Player.h"     // 다른 플레이어 렌더 객체
#include "Player.h"           // 내 플레이어
#include "Level_Manager.h"
#include "Monster.h"
#include "Monster_Orc.h"
CNetwork_Manager* CNetwork_Manager::m_pInstance = nullptr;

// ================================================================
//  Connect
//  TCP 연결 + 수신 스레드 시작
//  클라이언트는 서버 1개와만 통신  워커 스레드 1개면 충분
// ================================================================
bool CNetwork_Manager::Connect(const wchar_t* pszIP, uint16_t nPort)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;

    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) return false;

    // TCP_NODELAY  작은 패킷을 즉시 전송 (게임에서 필수)
    BOOL bNoDelay = TRUE;
    setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY,
        reinterpret_cast<const char*>(&bNoDelay), sizeof(bNoDelay));

    // IP 변환
    char szIP[64] = {};
    WideCharToMultiByte(CP_ACP, 0, pszIP, -1, szIP, sizeof(szIP), NULL, NULL);

    SOCKADDR_IN addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(nPort);
    inet_pton(AF_INET, szIP, &addr.sin_addr);

    if (connect(m_socket,
        reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        std::cout << "[Network] 연결 실패: " << WSAGetLastError() << std::endl;
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    m_bConnected = true;
    std::cout << "[Network] 서버 연결 완료" << std::endl;

    // 수신 스레드 시작  detach로 백그라운드 실행
    m_recvThread = std::thread(&CNetwork_Manager::RecvThread, this);
    m_recvThread.detach();

    return true;
}

void CNetwork_Manager::Disconnect()
{
    if (m_bConnected.exchange(false) == false) return;

    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
    WSACleanup();
}

// ================================================================
//  RecvThread 백그라운드 수신 스레드
//
//  서버 패킷 수신  ProcessPacket()으로 파싱
//  파싱된 패킷은 람다로 감싸서 큐에 push
//  실제 처리는 메인 스레드 Dispatch()에서
// ================================================================
void CNetwork_Manager::RecvThread()
{
    while (m_bConnected)
    {
        int32_t nReceived = recv(
            m_socket,
            reinterpret_cast<char*>(m_recvBuf) + m_nPrevRemain,
            RECV_BUF_SIZE - m_nPrevRemain,
            0);

        if (nReceived <= 0)
        {
            std::cout << "[Network] 서버 연결 끊김" << std::endl;
            m_bConnected = false;
            break;
        }

        // 서버 코드와 동일한 패킷 경계 파싱
        int32_t  nRemain = m_nPrevRemain + nReceived;
        uint8_t* pCursor = m_recvBuf;

        while (nRemain > 0)
        {
            if (nRemain < static_cast<int32_t>(sizeof(PacketHeader))) break;

            PacketHeader* pHeader = reinterpret_cast<PacketHeader*>(pCursor);
            if (nRemain < pHeader->size) break;

            // 패킷 데이터를 복사해서 람다에 캡처
            // (pCursor는 다음 루프에서 바뀌므로 복사 필수)
            std::vector<uint8_t> vData(pCursor, pCursor + pHeader->size);

            ProcessPacket(pCursor, pHeader->size);

            pCursor += pHeader->size;
            nRemain -= pHeader->size;
        }

        m_nPrevRemain = nRemain;
        if (nRemain > 0)
            memmove(m_recvBuf, pCursor, nRemain);
    }
}

// ================================================================
//  ProcessPacket
//  패킷 ID 확인  해당 핸들러 호출
//  핸들러는 람다로 감싸서 큐에 push
// ================================================================
void CNetwork_Manager::ProcessPacket(uint8_t* pBuffer, int32_t nSize)
{
    PacketHeader* pHeader = reinterpret_cast<PacketHeader*>(pBuffer);

    // 패킷 데이터를 vector로 복사  람다가 캡처할 수 있게
    std::vector<uint8_t> vData(pBuffer, pBuffer + nSize);

    switch (pHeader->id)
    {
    case SC_LOGIN_OK:
        PushTask([this, vData]() mutable {
            Handle_SC_LOGIN_OK(vData.data(), static_cast<int32_t>(vData.size()));
            });
        break;

    case SC_LOGIN_FAIL:
        PushTask([this, vData]() mutable {
            Handle_SC_LOGIN_FAIL(vData.data(), static_cast<int32_t>(vData.size()));
            });
        break;

    case SC_ENTER_GAME:
        PushTask([this, vData]() mutable {
            Handle_SC_ENTER_GAME(vData.data(), static_cast<int32_t>(vData.size()));
            });
        break;

    case SC_ADD_PLAYER:
        PushTask([this, vData]() mutable {
            Handle_SC_ADD_PLAYER(vData.data(), static_cast<int32_t>(vData.size()));
            });
        break;

    case SC_REMOVE_PLAYER:
        PushTask([this, vData]() mutable {
            Handle_SC_REMOVE_PLAYER(vData.data(), static_cast<int32_t>(vData.size()));
            });
        break;

    case SC_MOVE_PLAYER:
        PushTask([this, vData]() mutable {
            Handle_SC_MOVE_PLAYER(vData.data(), static_cast<int32_t>(vData.size()));
            });
        break;
    case SC_PLAYER_HIT:
        PushTask([this, vData]() mutable {
            Handle_SC_PLAYER_HIT(
                vData.data(), static_cast<int32_t>(vData.size()));
            }); break;
    case SC_ADD_MONSTER:
        PushTask([this, vData]() mutable {
            Handle_SC_ADD_MONSTER(vData.data(), static_cast<int32_t>(vData.size()));
            }); break;

    case SC_REMOVE_MONSTER:
        PushTask([this, vData]() mutable {
            Handle_SC_REMOVE_MONSTER(vData.data(), static_cast<int32_t>(vData.size()));
            }); break;

    case SC_MOVE_MONSTER:
        PushTask([this, vData]() mutable {
            Handle_SC_MOVE_MONSTER(vData.data(), static_cast<int32_t>(vData.size()));
            }); break;

    case SC_MONSTER_STATE:
        PushTask([this, vData]() mutable {
            Handle_SC_MONSTER_STATE(vData.data(), static_cast<int32_t>(vData.size()));
            }); break;
    case SC_PLAYER_STATE:
        PushTask([this, vData]() mutable {
            Handle_SC_PLAYER_STATE(
                vData.data(), static_cast<int32_t>(vData.size()));
            }); break;

    case SC_MONSTER_HIT:
        PushTask([this, vData]() mutable {
            Handle_SC_MONSTER_HIT(
                vData.data(), static_cast<int32_t>(vData.size()));
            }); break;
    default:
        std::cout << "[Network] 알 수 없는 패킷: " << pHeader->id << std::endl;
        break;
    }
}

// ================================================================
//  Dispatch  메인 스레드에서 매 프레임 호출
//
//  큐에 쌓인 태스크를 전부 처리
//  락을 짧게 잡는 방법
//  전체 큐를 로컬 큐로 swap  락 해제  처리
//   처리 중 새 패킷이 와도 큐에 쌓을 수 있음
// ================================================================
void CNetwork_Manager::Dispatch()
{
    // 큐 전체를 로컬로 가져옴 (락 최소화)
    std::queue<FPacketTask> localQueue;
    {
        std::lock_guard<std::mutex> lock(m_queueLock);
        std::swap(localQueue, m_taskQueue);
    }

    // 락 없이 처리 이미 메인 스레드에 있으므로 D2D 안전
    while (!localQueue.empty())
    {
        localQueue.front().m_handler();
        localQueue.pop();
    }
}

// ================================================================
//  패킷별 핸들러  메인 스레드에서 실행됨
// ================================================================
void CNetwork_Manager::Handle_SC_LOGIN_OK(uint8_t* pBuffer, int32_t nSize)
{
    SC_LOGIN_OK_PACKET* pPkt = reinterpret_cast<SC_LOGIN_OK_PACKET*>(pBuffer);
    m_nMyPlayerID = pPkt->playerID;
    std::cout << "[Network] 로그인 성공. PlayerID=" << m_nMyPlayerID << std::endl;

}

void CNetwork_Manager::Handle_SC_LOGIN_FAIL(uint8_t* pBuffer, int32_t nSize)
{
    SC_LOGIN_FAIL_PACKET* pPkt = reinterpret_cast<SC_LOGIN_FAIL_PACKET*>(pBuffer);
    std::cout << "[Network] 로그인 실패. reason=" << (int)pPkt->reason << std::endl;

    // TODO: 실패 메시지 UI 표시
}

void CNetwork_Manager::Handle_SC_ENTER_GAME(uint8_t* pBuffer, int32_t nSize)
{
    SC_ENTER_GAME_PACKET* pPkt =
        reinterpret_cast<SC_ENTER_GAME_PACKET*>(pBuffer);

    m_fSpawnX = pPkt->fCurX;
    m_fSpawnZ = pPkt->fCurZ;
    m_bSpawnReady = true;

    CLevel_Manager::Get_Instance()->Level_Change(LEVEL_TEST);
}

void CNetwork_Manager::Handle_SC_ADD_PLAYER(uint8_t* pBuffer, int32_t nSize)
{
    SC_ADD_PLAYER_PACKET* pPkt =
        reinterpret_cast<SC_ADD_PLAYER_PACKET*>(pBuffer);

    if (pPkt->playerID == m_nMyPlayerID) return;

    CGameObject* pExist =
        CObject_Manager::Get_Instance()->Find_OtherPlayer(pPkt->playerID);
    if (pExist) return;

    COther_Player* pOther = new COther_Player;
    pOther->Initialize(pPkt->playerID, pPkt->name,
        pPkt->fCurX, pPkt->fCurZ);

    if (pPkt->state == PLAYER_DEAD)
    {
        pOther->SetDeadState();
    }
    else
    {
        if (pPkt->fDestX != pPkt->fCurX || pPkt->fDestZ != pPkt->fCurZ)
        {
            pOther->OnMoveDestPacket(
                pPkt->fCurX, pPkt->fCurZ,
                pPkt->fDestX, pPkt->fDestZ,
                pPkt->fSpeed, 0);
        }
    }

    CObject_Manager::Get_Instance()->Add_Object(OBJ_OTHER_PLAYER, pOther);

    std::cout << "[Network] 플레이어 추가. ID=" << pPkt->playerID
        << " name=" << pPkt->name
        << " state=" << (int)pPkt->state << std::endl;
}

void CNetwork_Manager::Handle_SC_REMOVE_PLAYER(uint8_t* pBuffer, int32_t nSize)
{
    SC_REMOVE_PLAYER_PACKET* pPkt =
        reinterpret_cast<SC_REMOVE_PLAYER_PACKET*>(pBuffer);

    CGameObject* pObj =
        CObject_Manager::Get_Instance()->Find_OtherPlayer(pPkt->playerID);
    if (!pObj) return;

    // Set_Dead() → Object_Manager Update에서 자동 제거
    pObj->Set_Dead();

    std::cout << "[Network] 플레이어 제거. ID=" << pPkt->playerID << std::endl;
}

void CNetwork_Manager::Handle_SC_MOVE_PLAYER(uint8_t* pBuffer, int32_t nSize)
{
    SC_MOVE_PLAYER_PACKET* pPkt =
        reinterpret_cast<SC_MOVE_PLAYER_PACKET*>(pBuffer);

    if (pPkt->playerID == m_nMyPlayerID) return;

    CGameObject* pObj =
        CObject_Manager::Get_Instance()->Find_OtherPlayer(pPkt->playerID);
    if (!pObj) return;

    COther_Player* pOther = static_cast<COther_Player*>(pObj);
    pOther->OnMoveDestPacket(
        pPkt->fCurX, pPkt->fCurZ,
        pPkt->fDestX, pPkt->fDestZ,
        pPkt->fSpeed, pPkt->moveTime);
}
//-몬스터
void CNetwork_Manager::Handle_SC_ADD_MONSTER(uint8_t* pBuffer, int32_t nSize)
{
    SC_ADD_MONSTER_PACKET* pPkt =
        reinterpret_cast<SC_ADD_MONSTER_PACKET*>(pBuffer);

    // 중복 방지
    if (CObject_Manager::Get_Instance()->Find_Monster(pPkt->monsterID))
        return;

    // 몬스터 타입에 따라 생성
    CMonster* pMonster = nullptr;
    switch (static_cast<MONSTER_TYPE>(pPkt->monsterType))
    {
    case MONSTER_ORC:
        pMonster = new CMonster_Orc;
        break;
    default:
        std::cout << "[Network] 알 수 없는 몬스터 타입: "
            << (int)pPkt->monsterType << std::endl;
        return;
    }

    pMonster->Initialize();
    pMonster->Set_WorldPos(pPkt->fCurX, pPkt->fCurZ);
    pMonster->Set_MonsterID(pPkt->monsterID);
    pMonster->Set_Speed(pPkt->fSpeed);

    // 이동 중이면 목적지 세팅
    MONSTER_STATE eState = static_cast<MONSTER_STATE>(pPkt->state);
    if (eState == MON_WALK)
    {
        pMonster->Set_Dest(pPkt->fDestX, pPkt->fDestZ);
        pMonster->Set_Moving(true);
        pMonster->Motion_Change(MON_WALK);
    }

    CObject_Manager::Get_Instance()->Add_Object(OBJ_MONSTER, pMonster);

    std::cout << "[Network] 몬스터 추가. ID=" << pPkt->monsterID
        << " Type=" << (int)pPkt->monsterType
        << " pos=(" << pPkt->fCurX << ", " << pPkt->fCurZ << ")"
        << std::endl;
}

void CNetwork_Manager::Handle_SC_REMOVE_MONSTER(uint8_t* pBuffer, int32_t nSize)
{
    SC_REMOVE_MONSTER_PACKET* pPkt =
        reinterpret_cast<SC_REMOVE_MONSTER_PACKET*>(pBuffer);

    CGameObject* pObj =
        CObject_Manager::Get_Instance()->Find_Monster(pPkt->monsterID);
    if (!pObj) return;

    pObj->Set_Dead();

    std::cout << "[Network] 몬스터 제거. ID=" << pPkt->monsterID << std::endl;
}

void CNetwork_Manager::Handle_SC_MOVE_MONSTER(uint8_t* pBuffer, int32_t nSize)
{
    SC_MOVE_MONSTER_PACKET* pPkt =
        reinterpret_cast<SC_MOVE_MONSTER_PACKET*>(pBuffer);

    CGameObject* pObj =
        CObject_Manager::Get_Instance()->Find_Monster(pPkt->monsterID);
    if (!pObj) return;

    // CMonster 베이스로 캐스팅 - 어떤 몬스터든 동작
    CMonster* pMonster = static_cast<CMonster*>(pObj);
    pMonster->OnMovePacket(
        pPkt->fCurX, pPkt->fCurZ,
        pPkt->fDestX, pPkt->fDestZ,
        pPkt->fSpeed, pPkt->dir);
}

void CNetwork_Manager::Handle_SC_MONSTER_STATE(uint8_t* pBuffer, int32_t nSize)
{
    SC_MONSTER_STATE_PACKET* pPkt =
        reinterpret_cast<SC_MONSTER_STATE_PACKET*>(pBuffer);

    CGameObject* pObj =
        CObject_Manager::Get_Instance()->Find_Monster(pPkt->monsterID);
    if (!pObj) return;

    CMonster* pMonster = static_cast<CMonster*>(pObj);

    // 방향 먼저 적용
    pMonster->Set_Dir(static_cast<DIRECTION>(pPkt->dir));

    pMonster->OnStatePacket(
        static_cast<MONSTER_STATE>(pPkt->state),
        pPkt->targetID);
}


// ================================================================
//  송신 함수들
// ================================================================
bool CNetwork_Manager::SendRaw(const void* pData, int32_t nSize)
{
    if (!m_bConnected) return false;
    return send(m_socket,
        reinterpret_cast<const char*>(pData), nSize, 0) != SOCKET_ERROR;
}

void CNetwork_Manager::SendLogin(const char* pszID, const char* pszPW)
{
    CS_LOGIN_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = CS_LOGIN;
    strncpy_s(pkt.id, pszID, sizeof(pkt.id) - 1);
    strncpy_s(pkt.pw, pszPW, sizeof(pkt.pw) - 1);
    SendRaw(&pkt, sizeof(pkt));
}

void CNetwork_Manager::SendMoveDest(float fDestX, float fDestZ, uint32_t nMoveTime)
{
    CS_MOVE_DEST_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = CS_MOVE_DEST;
    pkt.fDestX = fDestX;
    pkt.fDestZ = fDestZ;
    pkt.moveTime = nMoveTime;
    SendRaw(&pkt, sizeof(pkt));
}

void CNetwork_Manager::SendMovePos(float fCurX, float fCurZ, uint32_t nMoveTime)
{
    CS_MOVE_POS_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = CS_MOVE_POS;
    pkt.fCurX = fCurX;
    pkt.fCurZ = fCurZ;
    pkt.moveTime = nMoveTime;
    SendRaw(&pkt, sizeof(pkt));
}

void CNetwork_Manager::SendAttackMonster(
    int32_t nMonsterID, float fCurX, float fCurZ)
{
    CS_ATTACK_MONSTER_PACKET pkt = {};
    pkt.header.size = sizeof(pkt);
    pkt.header.id = CS_ATTACK_MONSTER;
    pkt.monsterID = nMonsterID;
    pkt.fCurX = fCurX;
    pkt.fCurZ = fCurZ;
    SendRaw(&pkt, sizeof(pkt));
}

// 다른 플레이어 공격 모션 수신
void CNetwork_Manager::Handle_SC_PLAYER_STATE(uint8_t* pBuffer, int32_t nSize)
{
    SC_PLAYER_STATE_PACKET* pPkt =
        reinterpret_cast<SC_PLAYER_STATE_PACKET*>(pBuffer);

    // 내 플레이어는 로컬에서 이미 처리
    if (pPkt->playerID == m_nMyPlayerID) return;

    CGameObject* pObj =
        CObject_Manager::Get_Instance()->Find_OtherPlayer(pPkt->playerID);
    if (!pObj) return;

    COther_Player* pOther = static_cast<COther_Player*>(pObj);
    PLAYER_STATE eTempState = (PLAYER_STATE)pPkt->state;
    switch (eTempState) {
    case PLAYER_ATTACK:
        pOther->OnAttackPacket();
        break;
    case PLAYER_HIT:
        break;
    case PLAYER_DEAD:
        break;

    default:
        break;

    }
}

void CNetwork_Manager::Handle_SC_PLAYER_HIT(uint8_t* pBuffer, int32_t nSize)
{
    SC_PLAYER_HIT_PACKET* pPkt =
        reinterpret_cast<SC_PLAYER_HIT_PACKET*>(pBuffer);

    if (pPkt->playerID == m_nMyPlayerID)
    {
        // 내 플레이어 피격
        CPlayer* pPlayer = dynamic_cast<CPlayer*>(CObject_Manager::Get_Instance()->Get_Player());
        if (!pPlayer) return;

        pPlayer->Set_Hp(pPkt->nHp);
        pPlayer->Hit();
    }
    else
    {
        // 다른 플레이어 피격
        CGameObject* pObj =
            CObject_Manager::Get_Instance()->Find_OtherPlayer(pPkt->playerID);
        if (!pObj) return;

        COther_Player* pOther = static_cast<COther_Player*>(pObj);
        pOther->OnHitPacket(pPkt->nHp);
    }
}

// 몬스터 피격 수신
void CNetwork_Manager::Handle_SC_MONSTER_HIT(uint8_t* pBuffer, int32_t nSize)
{
    SC_MONSTER_HIT_PACKET* pPkt =
        reinterpret_cast<SC_MONSTER_HIT_PACKET*>(pBuffer);

    CGameObject* pObj =
        CObject_Manager::Get_Instance()->Find_Monster(pPkt->monsterID);
    if (!pObj) return;

    CMonster* pMonster = static_cast<CMonster*>(pObj);

    // HP 갱신
    pMonster->Set_Hp(pPkt->nHp);
   // pMonster->Set_MaxHp(pPkt->nMaxHp);

    // 방향 적용
    pMonster->Set_Dir(static_cast<DIRECTION>(pPkt->dir));

    // 피격 상태 적용
    pMonster->OnStatePacket(MON_HIT, -1);
}