# 2D 아이소메트릭 MMORPG — 전체 포트폴리오 (서버 + 클라이언트)

> Windows **IOCP 게임 서버(C++)** + **Direct2D 클라이언트(C++)** 를 직접 구현한 MMORPG.
> 이동/전투/인벤토리/경매/레벨을 **서버 권위(server-authoritative)** 로 처리하고,
> **MySQL(ODBC/nanodbc)** 로 계정·캐릭터·인벤·장비·퀵슬롯·경매를 영속화한다.
>
> 이 문서는 **면접에서 말로 설명하기 위한 전체 정리본**이다. "무엇을 썼나"가 아니라
> **"왜 그렇게 했나"** 를 중심으로 쓴다. 서버만 보려면 `PORTFOLIO_Server.md`(요약본)를 참고.

---

## 목차

1. [프로젝트 개요](#1-프로젝트-개요)
2. [기술 스택 총정리](#2-기술-스택-총정리)
3. [전체 아키텍처 한 장](#3-전체-아키텍처-한-장)
4. [프로토콜 — 서버·클라 계약](#4-프로토콜--서버클라-계약)
5. [서버 ① 네트워크 계층 (IOCP)](#5-서버--네트워크-계층-iocp)
6. [서버 ② 동시성 모델](#6-서버--동시성-모델)
7. [서버 ③ 월드 — 존/타일/시야(AOI)](#7-서버--월드--존타일시야aoi)
8. [서버 ④ 이동 — 역산 + 해킹 방지](#8-서버--이동--역산--해킹-방지)
9. [서버 ⑤ 길찾기 (A* / Theta* / Corner)](#9-서버--길찾기-a--theta--corner)
10. [서버 ⑥ 몬스터 AI](#10-서버--몬스터-ai)
11. [서버 ⑦ 전투 / 사망 / 리스폰](#11-서버--전투--사망--리스폰)
12. [서버 ⑧ 아이템 · 인벤토리 · 장비 · 버프](#12-서버--아이템--인벤토리--장비--버프)
13. [서버 ⑨ 상점](#13-서버--상점)
14. [서버 ⑩ 경매장 (핵심)](#14-서버--경매장-핵심)
15. [서버 ⑪ 레벨 / 경험치](#15-서버--레벨--경험치)
16. [서버 ⑫ 퀵슬롯 — 유일하게 "클라 권위"인 것](#16-서버--퀵슬롯--유일하게-클라-권위인-것)
17. [DB 연동 전체](#17-db-연동-전체)
18. [클라이언트 ① 구조와 게임 루프](#18-클라이언트--구조와-게임-루프)
19. [클라이언트 ② 아이소메트릭 좌표계와 렌더 정렬](#19-클라이언트--아이소메트릭-좌표계와-렌더-정렬)
20. [클라이언트 ③ 네트워크 — 스레드 분리와 태스크 큐](#20-클라이언트--네트워크--스레드-분리와-태스크-큐)
21. [클라이언트 ④ 보간 / 스냅 / 애니메이션](#21-클라이언트--보간--스냅--애니메이션)
22. [클라이언트 ⑤ UI 시스템](#22-클라이언트--ui-시스템)
23. [서버-클라 "반드시 일치해야 하는 것" 목록](#23-서버-클라-반드시-일치해야-하는-것-목록)
24. [예상 면접 질문 & 답변 (25문)](#24-예상-면접-질문--답변-25문)
25. [트러블슈팅 사례 (실제 겪은 것)](#25-트러블슈팅-사례-실제-겪은-것)
26. [현재 한계와 개선 방향](#26-현재-한계와-개선-방향)
27. [부록 — 파일별 역할 인덱스](#27-부록--파일별-역할-인덱스)

---

## 1. 프로젝트 개요

| 항목 | 내용 |
|---|---|
| 장르 | 2D 아이소메트릭 MMORPG (2:1 쿼터뷰) |
| 구성 | **게임 서버**(`MMO_GameServer.sln`) + **클라이언트**(`MMO_Client.sln`) — 별도 솔루션 |
| 언어/환경 | C++17, MSVC v143, x64, Windows, Unicode |
| 서버 | Windows IOCP, TCP **7777** 포트 |
| 클라 | Win32 + **Direct2D** + DirectWrite, 144FPS 고정 루프 |
| DB | **MySQL 8.0** + ODBC(Connector/ODBC 9.7) + **nanodbc** + 저장 프로시저 |
| 규모 | 서버 약 6,800줄 / 클라 약 15,000줄 (nanodbc 벤더 코드 제외) |

**직접 구현한 것** — 네트워크 계층(IOCP 래핑부터), 패킷 프로토콜, 세션/시야/존 관리,
몬스터 AI와 길찾기, 전투, 아이템/인벤/장비/버프, 상점, 경매장, 레벨/경험치, 퀵슬롯,
DB 영속화, 그리고 클라의 렌더링/UI/보간까지 전부. 게임 엔진·네트워크 라이브러리를 쓰지 않았다.

**월드 구성** — 허브인 **마을(ZONE_TOWN, 30×30)** 을 중심으로 동서남북 4개 **필드(20×30)**.
마을 네 모서리의 포탈로 각 필드에 진입하고, 각 필드의 복귀 포탈은 마을 반대편에 있다.
몬스터는 서버가 각 필드에 랜덤 배치(오크 3 + 윙 2).

---

## 2. 기술 스택 총정리

### 서버

| 분류 | 기술 | 선택 이유 |
|---|---|---|
| I/O 모델 | **Windows IOCP** + `AcceptEx` | 수천 커넥션을 소수 스레드로. 커넥션 수와 스레드 수를 분리 |
| 오버랩 확장 | `CIOEvent : WSAOVERLAPPED` | 완료 통지에 "무슨 작업이었나"(IOType)를 실어 보냄 |
| 세션 수명 | `std::shared_ptr` (`enable_shared_from_this`) | 비동기 I/O가 진행 중인 세션이 파괴되는 것 방지 |
| 동시성 | `std::mutex`, `std::recursive_mutex`, `std::atomic`, CAS | 자원별로 락을 쪼갬. 몬스터 AI 활성화는 lock-free CAS |
| 타이머 | 우선순위 큐 + 타이머 스레드 → `PostQueuedCompletionStatus` | AI 틱/리스폰/타격판정/자동저장을 **워커 풀이 처리** |
| 길찾기 | **A\***, **Theta\*** (Any-Angle), Corner-based | 격자 최단경로 + 자연스러운 대각 이동 |
| DB 접근 | **MySQL + ODBC + nanodbc** | ODBC 표준으로 엔진 독립. nanodbc는 소스 2파일 얇은 래퍼 |
| DB 로직 | **저장 프로시저**(로그인) + **바인딩 SQL + 트랜잭션**(저장/경매) | 작업 성격에 맞게 도구 분리 (Q3 참고) |
| 동시성 제어 | **원자적 조건부 UPDATE** (InnoDB 행 잠금) | 경매 동시 구매 시 오버셀/아이템 복사 원천 차단 |

### 클라이언트

| 분류 | 기술 | 비고 |
|---|---|---|
| 렌더 | **Direct2D** (`ID2D1HwndRenderTarget`) + **DirectWrite** | 싱글스레드 렌더 모델 |
| 루프 | `QueryPerformanceCounter` 기반 144FPS 고정 + `timeBeginPeriod(1)` | `Sleep(1)` 스핀 혼합 |
| 구조 | 매니저 싱글턴 + `CLevel` 상태 + `CGameObject` 추상 기반 | Level/Object/UI/Img/Input/Map/Collision/Timer/Network/Camera |
| 네트워크 | 전용 recv 스레드 + **태스크 큐** → 메인 스레드 `Dispatch()` | D2D 싱글스레드 안전성 확보 |
| 정렬 | 발밑(Y) 기준 깊이 소팅 + `Set_SortOffset` 보정 | 아이소 겹침 처리 |
| 비동기 | `Change_Zone_Async` — 별도 스레드 로딩 + 로딩 화면 | `std::atomic<bool> m_bLoading` |

---

## 3. 전체 아키텍처 한 장

```
        [클라이언트 N개]  Direct2D / 144FPS / recv스레드+태스크큐
                 │  TCP 7777 (TCP_NODELAY)
    ┌────────────▼──────────────────────────────────────┐
    │                  CIOCP_Server                      │
    │   Listen + AcceptEx 풀(10슬롯)                     │
    │   Worker 스레드 풀(hardware_concurrency)           │
    │   Timer 스레드 1  ·  Debug 콘솔 스레드 1           │
    └────────────┬──────────────────────────────────────┘
                 │ GetQueuedCompletionStatus
                 │ (CIOEvent::IOType 으로 분기)
   ┌─────────────┼────────────────┬──────────────────┐
   ▼             ▼                ▼                  ▼
Accept/Recv/  MonsterAI      MonsterRespawn      PlayerAutoSave
 Send         MonsterAttackHit                   (5초, 라운드로빈 1명)
   │
   ▼
CPacket_Handler::Handle  (패킷 ID → static 디스패치)
   │
   ├── CZone ────── 타일맵 / 플레이어·몬스터 집합 / 시야(AOI) / AI / 전투 / 드롭풀
   ├── CPlayer ──── 위치·HP/MP·인벤40·장비6·퀵슬롯8·레벨/경험치·버프 (m_saveLock)
   ├── CMonster ─── 상태머신 / 어그로 / 공격쿨 / 경험치보상
   └── CDB_Manager ─ sp_login / Save(트랜잭션) / 경매 6종 (nanodbc)
                              │
                         [ MySQL: account, character, inventory,
                                  equipment, quickslot, auction ]
```

**매니저 싱글턴 (서버)**

| 매니저 | 책임 |
|---|---|
| `CSession_Manager` | 세션 슬롯 발급/조회/반납 (`MAX_SESSION`) |
| `CPlayer_Manager` | 세션ID와 **같은 인덱스**로 플레이어 관리 + 주기저장 라운드로빈 커서 |
| `CZone_Manager` | zoneID → `CZone*`. 생성자에서 5개 존 + 블록맵 + 몬스터 랜덤 배치 |
| `CMonster_Manager` | 몬스터 ID 발급/조회, 타입별 스탯 세팅 |
| `CDB_Manager` | 로그인/저장/경매 DB 접근 |

> `CPlayer_Manager`가 **세션ID를 그대로 플레이어 슬롯 인덱스로 쓰는 것**이 포인트다.
> 별도 매핑 테이블이 필요 없어 조회가 O(1)이고, 세션 해제 = 플레이어 해제로 수명이 일치한다.

---

## 4. 프로토콜 — 서버·클라 계약

### 4.1 기본 형태

```cpp
#pragma pack(push, 1)              // 패딩 제거 → 컴파일러/플랫폼 무관하게 바이트 고정
struct PacketHeader { uint16_t size; uint16_t id; };
// [헤더 4바이트][페이로드...]
#pragma pack(pop)
```

- `CS_*` = **1000번대** (Client→Server)
- `SC_*` = **2000번대**(플레이어/시스템), **2100번대**(몬스터)
- `Protocol.h`는 **클라/서버 양쪽에 동일 복제본**. 한쪽만 고치면 직렬화가 즉시 깨지므로 "양쪽 동시 수정"이 규칙.

### 4.2 전체 패킷 목록

**Client → Server (19종)**

| ID | 패킷 | 내용 |
|---|---|---|
| 1000 | `CS_LOGIN` | id[20], pw[20] |
| 1001 | `CS_MOVE_DEST` | 목적지 클릭 (dest, moveTime) |
| 1002 | `CS_MOVE_POS` | 타일 바뀔 때 현재 위치 보고 |
| 1003 | `CS_ATTACK_MONSTER` | monsterID + 내 위치 |
| 1004 | `CS_RESPAWN` | 사망 후 부활 요청 |
| 1005 | `CS_PORTAL` | targetZone + spawn 좌표 |
| 1006 | `CS_PICKUP` | dropId |
| 1007 | `CS_EQUIP` | invenSlot |
| 1008 | `CS_UNEQUIP` | equipSlot |
| 1009 | `CS_USE_ITEM` | invenSlot (포션) |
| 1010 | `CS_MOVE_STOP` | UI 진입 등 이동 강제 정지 |
| 1011 | `CS_BUY` | 상점 구매 (itemCode, count) |
| 1012 | `CS_SELL` | 상점 판매 (invenSlot, count) |
| 1013 | `CS_AUCTION_LIST` | page, tab, searchCodes[] |
| 1014 | `CS_AUCTION_REGISTER` | invenSlot, count, unitPrice |
| 1015 | `CS_AUCTION_BUY` | listingID, count |
| 1016 | `CS_AUCTION_COLLECT` | listingID (판매대금 수령) |
| 1017 | `CS_AUCTION_CANCEL` | listingID (등록 취소) |
| 1018 | `CS_QUICKSLOT_SET` | slot, itemCode (0=해제) |

**Server → Client (23종)**

| ID | 패킷 | 내용 |
|---|---|---|
| 2000/2001 | `SC_LOGIN_OK` / `SC_LOGIN_FAIL` | playerID / reason |
| 2002 | `SC_ENTER_GAME` | playerID, 위치, zoneID, 내 이름 |
| 2003~2005 | `SC_ADD/REMOVE/MOVE_PLAYER` | 시야 기반 |
| 2006 | `SC_PLAYER_STATE` | 상태 + **방향 + 확정 위치** |
| 2007 | `SC_PLAYER_HIT` | HP + **정지 위치** |
| 2008 | `SC_RESPAWN` | 부활 위치/HP |
| 2009 | `SC_CHANGE_ZONE` | 존 전환 확정 |
| 2010/2011 | `SC_ADD_DROP` / `SC_REMOVE_DROP` | 월드 드롭 |
| 2012 | `SC_INVEN_UPDATE` | **인벤+장비+골드 전체 스냅샷** |
| 2013 | `SC_PLAYER_HP` | HP/MaxHP/MP/MaxMP |
| 2014 | `SC_BUFF` | buffType, durationMs |
| 2015 | `SC_AUCTION_LIST` | 한 페이지 매물 + hasNext |
| 2016 | `SC_PLAYER_EXP` | level, exp, maxExp, levelUp |
| 2017 | `SC_QUICKSLOT_UPDATE` | codes[8] |
| 2100~2104 | `SC_ADD/REMOVE/MOVE_MONSTER`, `SC_MONSTER_STATE`, `SC_MONSTER_HIT` | 몬스터 시야 기반 |

### 4.3 스냅샷 방식이라는 설계 결정

인벤·장비·경매·퀵슬롯은 **델타(증분)를 보내지 않고 전체 스냅샷**을 보낸다.

- 장점: 클라와 서버 상태가 **어긋날 수가 없다**. 패킷 하나를 놓쳐도 다음 스냅샷에서 복구된다.
  "아이템 1개 추가" 같은 델타는 순서가 꼬이거나 유실되면 영구히 어긋난다.
- 비용: 인벤 40칸 × (코드+개수) = 320바이트. 인벤이 바뀌는 사건은 초당 수십 번이 아니라
  사용자 행동 단위라 **대역폭이 병목이 아니다**. 정확성을 택했다.

---

## 5. 서버 ① 네트워크 계층 (IOCP)

### 5.1 왜 IOCP인가

- `select`/`poll`은 fd 수에 비례해 O(N) 스캔이 든다.
- "커넥션당 스레드"는 컨텍스트 스위칭·스택 메모리가 폭발한다.
- **IOCP는 커널이 완료를 큐로 넘겨주고 워커 풀이 꺼내 처리** → 커넥션 수와 스레드 수가 분리된다.
  워커 스레드는 `hardware_concurrency()` 개만 띄운다.

### 5.2 AcceptEx 풀

`accept()`를 블로킹으로 부르지 않고, **미리 소켓 10개를 만들어 `AcceptEx`를 걸어둔다.**

```cpp
void CIOCP_Server::StartAccept() {
    for (int i = 0; i < ACCEPT_POOL_SIZE; ++i) {
        int32_t nID = CSession_Manager::Get_Instance()->Assign();   // 세션 슬롯 선점
        SessionRef s = ...;
        s->SetSocket(WSASocket(..., WSA_FLAG_OVERLAPPED));
        CreateIoCompletionPort((HANDLE)s->GetSocket(), m_hIOCP, (ULONG_PTR)nID, 0);
        ReRegisterAccept(s);          // AcceptEx 걸어둠
    }
}
```

- **완료 키(`ULONG_PTR`)에 세션 ID를 넣어** 완료 통지에서 바로 주인을 찾는다.
- 접속이 성사되면(`ProcessAccept`) `SO_UPDATE_ACCEPT_CONTEXT`로 후처리하고
  **곧바로 새 슬롯을 보충**해 풀을 유지한다. (AcceptEx의 필수 후처리 — 빼먹으면 소켓이 반쯤 죽는다.)

### 5.3 CIOEvent — 완료 통지 분기의 핵심

```cpp
struct CIOEvent {
    WSAOVERLAPPED m_overlapped = {};   // ★ 반드시 첫 멤버 (OVERLAPPED* → CIOEvent* 캐스팅)
    IOType        m_type;              // Accept/Recv/Send/MonsterAI/MonsterRespawn/
                                       // MonsterAttackHit/PlayerAutoSave
    CSession*     m_owner = nullptr;
    uint8_t       m_acceptBuf[(sizeof(SOCKADDR_IN)+16)*2] = {};
};
```

`GetQueuedCompletionStatus`가 돌려주는 `OVERLAPPED*`를 `CIOEvent*`로 캐스팅해
**"이 완료가 무슨 작업이었나"** 를 알아낸다. 이 한 구조체 덕분에

- 소켓 I/O(Accept/Recv/Send)
- **게임 이벤트**(몬스터 AI 틱, 리스폰, 타격 판정, 주기 저장)

가 **완전히 같은 워커 루프**를 탄다. 게임 로직을 위한 별도 스레드가 필요 없다.

### 5.4 세션 — TCP 스트림에서 패킷 경계 복원

TCP는 스트림이라 한 번의 `recv`에 패킷이 **여러 개 붙거나(뭉침)**, **하나가 잘려(분할)** 온다.

```cpp
void CSession::ProcessRecvData(int32_t nNewBytes) {
    int32_t nRemain = m_prevRemain + nNewBytes;   // 지난번 남은 조각 + 이번 수신
    uint8_t* pCursor = m_recvBuf;

    while (nRemain > 0) {
        if (nRemain < sizeof(PacketHeader)) break;          // 헤더도 안 왔다
        PacketHeader* h = (PacketHeader*)pCursor;
        if (nRemain < h->size) break;                       // 몸통이 덜 왔다

        HandlePacket(pCursor, h->size);                     // 완전한 패킷만 처리
        pCursor += h->size;  nRemain -= h->size;
    }
    m_prevRemain = nRemain;
    if (nRemain > 0) memmove(m_recvBuf, pCursor, nRemain);  // 잔여를 앞으로 당김
}
```

다음 `WSARecv`는 `m_recvBuf + m_prevRemain` 위치에 이어받는다. **클라도 똑같은 루프**를 쓴다.

### 5.5 송신 — 세션당 순차 전송

```cpp
void CSession::Send(void* p, int32_t n) {
    std::lock_guard lock(m_sendLock);
    m_sendQueue.emplace(...);           // 큐에 적재
    if (!m_sending) { m_sending = true; StartSend_Locked(); }
}
void CSession::OnSendComplete() {       // 이전 전송 완료 통지
    std::lock_guard lock(m_sendLock);
    StartSend_Locked();                 // 큐에 남은 다음 패킷
}
```

**send 버퍼와 오버랩 구조체가 세션당 하나뿐**이다. 앞의 `WSASend`가 완료되기 전에
같은 버퍼로 또 보내면 데이터가 덮인다. 그래서 "보내는 중이면 큐에 쌓고,
완료 통지가 오면 다음 것을 보낸다"로 **직렬화**했다.

### 5.6 세션 수명 (dangling I/O 문제)

`CSession`은 `shared_ptr`(`SessionRef`)로 관리한다. IOCP 완료는 **비동기로 늦게 도착**하므로,
진행 중인 I/O가 남았는데 세션 객체를 지우면 워커가 죽은 메모리를 만진다.
`shared_ptr` 참조가 살아있는 동안 세션이 유지되고, `Disconnect`는 `atomic exchange`로
**중복 진입을 막는다**(여러 워커가 동시에 끊김을 감지할 수 있으므로).

```cpp
void CSession::Disconnect() {
    if (m_connected.exchange(false) == false) return;   // 이미 끊는 중 → 즉시 반환
    ...
}
```

---

## 6. 서버 ② 동시성 모델

**원칙: 락은 잘게 쪼개고 짧게 잡는다. 느린 작업(DB I/O)은 절대 락 안에서 하지 않는다.**

| 자원 | 락 | 보호 범위 |
|---|---|---|
| 존의 플레이어/몬스터 집합 | `CZone::m_zoneLock`, `m_monsterLock` | 집합 add/remove/순회 |
| 플레이어 시야 리스트 | `CPlayer::m_viewLock`, `m_monsterViewLock` | 시야 목록 갱신 (플레이어/몬스터 각각) |
| 세션 송신 큐 | `CSession::m_sendLock` | 큐 조작 + 전송 시작 |
| 타이머 큐 | `g_timerLock` | 우선순위 큐 push/pop |
| 드롭 풀 | `CZone::m_dropLock` | 드롭 슬롯 선점/해제 |
| 저장 대상(인벤/골드/장비/퀵슬롯/레벨) | `CPlayer::m_saveLock` (**recursive**) | 스냅샷 복사 및 변경 |
| 몬스터 AI 활성화 | `std::atomic<bool> m_bActive` (**CAS**) | 락 없이 중복 활성화 방지 |

### 6.1 타이머 스레드 → IOCP (게임 이벤트의 단일 경로)

```cpp
void AddTimer(int32_t nID, EEventType eType, uint32_t nDelayMs);   // (ID, 종류, 만료시각) push
```

**단일 타이머 스레드**가 우선순위 큐(만료 시각 오름차순)를 1ms 간격으로 확인하고,
만료된 이벤트를 `PostQueuedCompletionStatus`로 **IOCP에 던진다**. 그러면 남는 워커가 처리한다.

- 몬스터 AI 틱 (500ms, 처리 후 스스로 재등록)
- 몬스터 리스폰 (사망 후 5초)
- 몬스터 공격 **타격 판정** (모션 시작 후 350/400ms)
- **플레이어 주기 저장** (5초, 처리 후 재등록)

> **"몬스터 1000마리 = 스레드 1000개"가 되지 않는다.** 이벤트 수만 늘고 스레드 수는 고정이다.
> 게임 이벤트와 소켓 I/O가 같은 워커 풀을 공유하므로 스레드 자원이 낭비되지 않는다.

### 6.2 CAS로 몬스터 AI 중복 활성화 방지

몬스터는 플레이어가 시야에 들어와야 AI가 돈다. 그런데 **두 플레이어가 동시에 시야에 진입**하면
두 워커가 동시에 `AddTimer(MonsterAI)`를 걸어 AI 틱이 2배로 도는 버그가 난다.

```cpp
bool expected = false;
if (pMonster->m_bActive.compare_exchange_strong(expected, true))   // 딱 한 번만 성공
    AddTimer(nID, EEventType::MonsterAI, 500);
```

락을 잡지 않고 **CAS 한 줄**로 해결했다. 어그로가 풀려 스폰 지점에 복귀하면 `m_bActive = false`로
되돌려 다음 진입 때 다시 활성화된다.

### 6.3 스냅샷-언더-락 (주기 저장의 데이터 레이스)

7장 DB 연동에서 자세히 다룬다. 요지는 **"짧은 락으로 값만 복사하고, 느린 DB I/O는 락 밖에서"**.

---

## 7. 서버 ③ 월드 — 존/타일/시야(AOI)

### 7.1 존과 타일맵

`CZone`은 맵 한 칸이다. 생성자에서 **내부 크기(20×30 등)를 받아 테두리를 두르고** 타일맵을 만든다.

```
[OUTSIDE 2칸][BORDER 1칸][ 내부 grass / block ][BORDER 1칸][OUTSIDE 2칸]
```

- 실제 타일맵 크기 = `2 + 1 + inner + 1 + 2`. 20×30 필드 → **26×36**.
- 이동 가능 = `TILE_GRASS` 뿐 (`Is_MovableTile`).
- 마을은 블록맵이 전부 0(장애물 없음)이고, 대신 **건물/텐트가 가리는 칸을 `SetBlock()`으로 지정**한다.
  이 목록은 클라 `CZone_Town::Build()`의 목록과 **완전히 같아야** 한다.

### 7.2 시야 처리 (AOI, Area Of Interest)

모든 이벤트를 전체 접속자에게 뿌리면 **O(N²)** 로 터진다.
각 플레이어는 `VIEW_RANGE = 5`(타일) 안의 대상만 시야 리스트로 유지한다.

```cpp
bool CZone::CanSee(PlayerRef a, PlayerRef b) {
    return abs(a->m_nTileX - b->m_nTileX) <= VIEW_RANGE
        && abs(a->m_nTileZ - b->m_nTileZ) <= VIEW_RANGE;   // 체비쇼프 거리(정사각 범위)
}
```

**타일이 바뀔 때만** 시야를 재계산하고 old/new를 diff 한다.

| 조건 | 동작 |
|---|---|
| new에 있고 old에 없음 | **새로 보임** → `SC_ADD_PLAYER` **양방향** + 상대 시야 리스트에도 나를 추가 |
| old에 있고 new에 없음 | **시야 이탈** → `SC_REMOVE_PLAYER` **양방향** |
| 둘 다 있음 | 계속 보임 → `SC_MOVE_PLAYER` |

> **"양방향"이 핵심.** 내가 움직여서 상대가 보이기 시작했다면, 상대 입장에서도 내가 보이기 시작한 것이다.
> 한쪽만 처리하면 "나는 쟤가 보이는데 쟤는 내가 안 보이는" 비대칭이 생긴다.

### 7.3 몬스터 시야는 별도 리스트

플레이어 시야(`m_viewList`)와 몬스터 시야(`m_monsterViewList`)를 **따로** 둔다.
갱신 주체가 다르기 때문이다.

- **플레이어가 움직일 때** → `UpdateMonsterView(player)` 로 그 플레이어 기준 재계산
- **몬스터가 움직일 때** → `Broadcast_MoveMonster` 안에서 **몬스터 기준으로 대칭 갱신**

두 번째가 실제로 버그였다(→ 25장 사례 3). 처음엔 "이미 시야에 있는 플레이어에게만" 이동 패킷을 보냈다.
그러면 **몬스터가 다른 사람을 쫓아 가만히 있는 내 시야로 들어와도 영영 안 보였다.**
플레이어가 움직여야만 시야가 갱신됐기 때문이다. 지금은 몬스터 이동 시에도
`bInRange && !bWasInView` → `Send_AddMonster` 먼저, `!bInRange && bWasInView` → `Send_RemoveMonster` 로
**대칭 처리**한다.

### 7.4 포탈 — 존 전환 (패킷 순서가 중요)

```
CS_PORTAL(targetZone, spawnX/Z) 수신
  1) LeaveZone(옛 존)         — 옛 존 사람들에게 나 remove, 나에게 그들 remove
  2) SC_CHANGE_ZONE 전송  ★ 반드시 add 들보다 먼저
  3) EnterZone(새 존)         — 새 시야의 add 들 + 드롭 목록 전송
```

**왜 `SC_CHANGE_ZONE`이 먼저여야 하나?** 클라는 이 패킷을 받으면 맵을 로드하고 자기 위치를 옮긴다.
만약 add 패킷들이 먼저 도착하면, 클라가 **아직 옛 존에 있다고 생각하는 상태**에서 새 객체를 만들고,
곧이어 존 전환 처리에서 그것들을 정리해버린다. 순서 하나로 "새 존에 갔는데 아무도 안 보이는" 버그가 난다.

클라의 `Handle_SC_CHANGE_ZONE`은 **맵 로드 + 내 위치 이동만** 하고, **객체 정리는 서버의 remove 패킷에 의존**한다.
(단, 드롭만은 예외로 클라가 `DeleteID(OBJ_DROP)`로 정리하고 새 존에서 `Send_AllDrops`로 다시 받는다.)

---

## 8. 서버 ④ 이동 — 역산 + 해킹 방지

### 8.1 계약

| 시점 | 패킷 | 서버가 하는 일 |
|---|---|---|
| 마우스 클릭 | `CS_MOVE_DEST` (목적지, 시각) | 목적지 타일 통과 가능 검증 → 이동 시작 정보 저장 → 시야에 브로드캐스트 |
| **타일이 바뀔 때마다** | `CS_MOVE_POS` (현재 위치, 시각) | 위치 검증 → 시야 재계산 |
| UI 열기 등 | `CS_MOVE_STOP` (현재 위치) | 현재 위치 커밋 + `m_bMoving = false` |

### 8.2 위치를 저장하지 않고 "역산"한다

서버는 이동 중인 플레이어의 위치를 매 틱 갱신하지 않는다.
대신 **이동 시작 위치 · 목적지 · 속도 · 시작 시각** 네 개만 들고 있다가, 필요할 때 계산한다.

```cpp
void CPlayer::GetCurrentPos(uint32_t nNow, float& outX, float& outZ) const {
    if (!m_bMoving) { outX = m_fCurX; outZ = m_fCurZ; return; }

    float dx = m_fDestX - m_fMoveStartX, dz = m_fDestZ - m_fMoveStartZ;
    float dist = sqrtf(dx*dx + dz*dz);
    float moved = m_fSpeed * (nNow - m_nMoveStartTime) / 1000.f;

    if (moved >= dist) { outX = m_fDestX; outZ = m_fDestZ; return; }   // 이미 도착
    outX = m_fMoveStartX + (dx/dist) * moved;                          // 시작 + 방향×거리
    outZ = m_fMoveStartZ + (dz/dist) * moved;
}
```

**이 함수 하나가 서버 곳곳에서 재사용된다.**
몬스터 어그로 판정, 공격 사거리 계산, 픽업 거리 검증, 해킹 검증, 도착 판정 — 전부 이 함수를 부른다.
"상태를 최소화하고 계산으로 대체한" 설계다. 매 틱 위치를 갱신하는 루프 자체가 없다.

### 8.3 해킹 방지 (텔레포트 / 속도핵)

```cpp
void CZone::OnMovePos(PlayerRef p, float fCurX, float fCurZ, uint32_t moveTime) {
    if (p->m_bMoving) {
        float sx, sz;  p->GetCurrentPos(moveTime, sx, sz);      // 서버가 믿는 위치
        float diff = sqrtf((fCurX-sx)*(fCurX-sx) + (fCurZ-sz)*(fCurZ-sz));

        constexpr float MAX_TOLERANCE = 2.f;                     // 2타일까지는 지연/오차로 인정
        if (diff > MAX_TOLERANCE) { fCurX = sx; fCurZ = sz; }    // 초과 → 서버 위치로 덮음
    }
    p->m_fCurX = fCurX;  p->m_fCurZ = fCurZ;
    ...
}
```

- 클라가 보고한 위치를 **서버 역산 위치와 비교**해 허용 오차(2타일)를 넘으면 **버리고 서버 값을 채택**한다.
- 목적지 검증(`IsMovable`)도 별도로 한다. 벽 안으로 이동 요청이 오면 현재 위치로 되돌리는 패킷을 보낸다.
- **왜 오차를 2타일이나 주나?** 네트워크 지연과 클라의 로컬 예측(즉시 이동) 때문에 정상 상황에서도 차이가 난다.
  0으로 두면 정상 플레이어가 계속 튕긴다. "즉시 반응성"과 "치팅 차단"의 절충점이다.

### 8.4 `CS_MOVE_STOP` — 실제로 겪은 함정

클라가 인벤/상점 UI를 열면 그 자리에 멈춘다. 그런데 **`CS_MOVE_POS`를 더 이상 보내지 않는다.**
서버는 이걸 모르니 `m_bMoving`이 계속 `true`고, `GetCurrentPos`는 **옛 목적지까지 계속 오버슈트**한다.
결과: 몬스터 AI가 "저기 있을 것"이라 믿는 허공을 때린다.

→ UI 진입 시 클라가 `CS_MOVE_STOP`을 보내고, 서버가 현재 위치를 커밋하며 `m_bMoving=false`로 만든다.
(이때도 서버 위치와 2타일 넘게 차이 나면 서버 값으로 스냅한다.)

---

## 9. 서버 ⑤ 길찾기 (A* / Theta* / Corner)

`CPathFinder`는 **서버와 클라 양쪽에 같은 구현**이 있다.
서버는 몬스터 추격에, 클라는 내 캐릭터의 클릭 이동 경로에 쓴다.

```cpp
using IsMovableFunc = std::function<bool(int32_t, int32_t)>;   // 맵 구현을 주입

enum class EPathMode { AStar, ThetaStar, CornerBased };

static std::vector<std::pair<float,float>> FindPath(
    int startX, int startZ, int endX, int endZ,
    float realStartX, float realStartZ,      // 타일 중심이 아닌 실제 float 위치에서 출발
    IsMovableFunc fnIsMovable, EPathMode mode);
```

**의존성 역전**: 길찾기는 "이 타일 통과 가능?"만 알면 된다. `std::function`으로 주입받아
`CZone`(서버)든 `CMap_Manager`(클라)든 같은 알고리즘을 쓴다.

| 모드 | 특징 | 쓰는 곳 |
|---|---|---|
| `AStar` | 격자 최단경로 + 타일 중심 String Pulling | **서버 몬스터 추격** |
| `ThetaStar` | Any-Angle. 부모에서 직접 **시야가 통하면 중간 노드를 건너뜀** → 계단 현상 없는 대각 경로 | 옵션 |
| `CornerBased` | 장애물 **모서리를 경유점**으로 삼아 벽을 스치듯 돌아감 | **클라 플레이어 이동** |

**Theta\* 핵심**: 노드 확장 시 `HasLineOfSight(부모, 이웃)`을 검사해 통하면 **부모를 그대로 계승**한다.
A*가 만드는 "격자 계단" 경로가 사라지고 자연스러운 직선/대각이 나온다.

> 몬스터는 `AStar`, 플레이어는 `CornerBased`를 쓴다. 플레이어는 사람이 보기에 자연스러운 게 중요하고,
> 몬스터는 500ms마다 경로를 다시 계산하므로 **매 틱의 첫 웨이포인트만 쓰고 버린다**. 정밀도보다 갱신 빈도가 중요하다.

---

## 10. 서버 ⑥ 몬스터 AI

### 10.1 상태머신

```
MON_IDLE(0) → MON_WALK(1) → MON_ATTACK_0(2) / MON_ATTACK_1(3) → MON_HIT(4) → MON_DEAD(5)
```

AI 틱은 **타이머로 500ms마다** 예약된다(`AddTimer(MonsterAI, 500)` → 처리 후 스스로 재등록).

```
[시야 진입] → CAS 활성화 → AI 틱 시작
  ├ MON_HIT 이고 경직 안 끝남 → AI 스킵, 다음 틱 예약  (피격 경직 600ms)
  ├ 어그로 범위(3타일)에 플레이어 없음
  │    ├ 타겟이 있었으면 → Monster_Patrol (스폰 지점 복귀) → 도착하면 m_bActive=false (AI 정지)
  │    └ 타겟이 없었으면 → IDLE 유지하며 대기 루프
  ├ 공격 사거리(1타일) 안 → Monster_Attack
  └ 그 외 → Monster_Chase (길찾기 추격)
```

- 어그로 범위 `m_fAggroRange = 3`, 해제 범위 `m_fDeAggroRange = 4` — **다르게 둔 이유는 히스테리시스**.
  같으면 경계선에서 어그로가 붙었다 풀렸다 떨린다.
- `FindNearestPlayer`는 `GetCurrentPos`로 **이동 중인 플레이어의 실시간 위치**를 본다.

### 10.2 공격 = "모션"과 "타격"의 분리 (중요)

몬스터가 때리는 순간과 클라 애니메이션의 타격 프레임이 어긋나면
"안 맞았는데 피가 깎이는" 느낌이 난다. 그래서 **두 단계로 나눴다.**

```cpp
void CZone::Monster_Attack(MonsterRef m) {
    if (now - m->m_nLastAtkTime < m->m_nAtkCoolMs) return;      // 쿨타임(2초)

    MONSTER_STATE eAtk = (rand()%2) ? MON_ATTACK_0 : MON_ATTACK_1;   // 모션 2종 랜덤
    m->m_eState = eAtk;
    Broadcast_MonsterState(m, targetID);                        // ① 모션 즉시 전송

    m->m_nPendingHitTargetID = m->m_nTargetID;
    uint32_t delay = (eAtk == MON_ATTACK_0) ? 350 : 400;        // 모션별 타격 프레임 시각
    AddTimer(m->m_nMonsterID, EEventType::MonsterAttackHit, delay);   // ② 타격은 나중에
}
```

`OnMonsterAttackHit`(타이머 콜백)에서 **그때 다시 검증**한다.

- 몬스터가 그 사이 죽었으면 취소
- 타겟이 **도망갔으면(사거리×2 초과) 빗나감**
- **무적 버프 중이면 피해 0**
- 피해 = `max(1, 몬스터공격력(10) − 플레이어 방어력)`

> "때리는 시늉을 하고 나중에 실제로 맞는다"는 게 실제 게임의 동작이다. 회피가 가능해진다.

### 10.3 두 가지 몬스터 타입

| 타입 | HP | 경험치 | 이동 |
|---|---|---|---|
| `MONSTER_ORC` | 100 | 20 | **A\* 길찾기**로 장애물을 우회 |
| `MONSTER_WING` | 80 | 25 | **길찾기 없이 직선 추격**(부유 몬스터 — 장애물 무시) |

WING은 `Monster_Chase`에서 아예 다른 분기를 탄다. 길찾기 비용이 0이고,
플레이어 입장에선 "벽 뒤로 숨어도 소용없는" 성가신 적이 된다. HP는 낮지만 경험치는 더 준다.

### 10.4 패킷 절약 — 변화가 있을 때만 보낸다

```cpp
bool bTileChanged = pMonster->UpdateTilePos();
bool bDirChanged  = (ePrevDir != eNewDir);
bool bStateChanged = (ePrevState != MON_WALK);

if (bTileChanged || bDirChanged || bStateChanged)
    Broadcast_MoveMonster(pMonster);      // 셋 중 하나라도 바뀌었을 때만
```

몬스터는 500ms마다 조금씩 움직인다. 매 틱 이동 패킷을 뿌리면 낭비다.
**타일이 바뀌거나 / 방향이 바뀌거나 / 상태가 바뀔 때만** 보내고, 그 사이는 클라가 보간한다.

---

## 11. 서버 ⑦ 전투 / 사망 / 리스폰

### 11.1 플레이어 → 몬스터

```cpp
void CZone::OnPlayerAttackMonster(PlayerRef p, int32_t monsterID, float px, float pz) {
    if (p->m_bDead) return;
    if (now - p->m_nLastAtkTime < p->m_nAtkCoolMs) return;        // 공격 쿨 400ms (서버 검증)

    if (dist(p, monster) > 3.f) { /* [경고] 로그 */ return; }     // 거리 검증(클라 사거리 1.5의 2배)

    // 공격 시 위치 커밋 — 클라는 공격 중 CS_MOVE_POS를 안 보내므로
    p->m_fCurX = px; p->m_fDestX = px; p->m_bMoving = false; ...

    // 몬스터를 바라보는 방향 계산 → 관찰자에게도 같은 방향으로 보이도록
    p->m_eDir = CalcDir8(monster - player);
    Broadcast_PlayerState(p, PLAYER_ATTACK);                      // 방향 + 확정 위치 포함

    monster->m_nHp -= p->Get_Atk();                               // 기본10 + 장비 + 버프

    if (monster->m_nHp <= 0) {
        monster->m_eState = MON_DEAD;  monster->m_bActive = false;
        Broadcast_MonsterState(monster);
        AddTimer(monsterID, MonsterRespawn, 5000);                // 5초 후 리스폰

        FDropRoll roll = RollDrop();                              // 드롭 추첨
        if (roll.bDrop) SpawnDrop(roll.code, roll.amount, monster->x, monster->z);

        int32_t levelUp = p->AddExp(monster->m_nExpReward);       // 경험치 (막타)
        Send_PlayerExp(p, levelUp > 0);
        if (levelUp > 0) Send_PlayerHp(p);                        // 레벨업 → MaxHP 변동 + 풀회복
        return;
    }
    // 미사망 → 경직 600ms + 이동 취소 + 피격 브로드캐스트
    monster->m_eState = MON_HIT;
    monster->m_fDestX = monster->m_fCurX;   // 제자리 (피격 중 계속 걷던 버그 수정)
    monster->m_nHitStunEndTime = now + 600;
    Broadcast_MonsterHit(monster);
}
```

### 11.2 스탯 계산 (서버가 유일한 진실)

```cpp
int32_t CPlayer::Get_Atk() const {
    int32_t atk = m_baseAtk;                                   // 레벨 파생: 10 + (레벨-1)*2
    for (int i = 0; i < EQUIP_SLOTS; ++i)
        if (m_equipCode[i]) atk += EquipAtk(m_equipCode[i]);   // 장비 6칸
    if (GetTickCount64() < m_nAtkBuffEnd) atk += m_nAtkBuffAmt;   // 공격력 버프(15초)
    return atk;
}
int32_t CPlayer::Get_Def() const { /* 기본 5 + (레벨-1) + 장비 */ }
bool CPlayer::IsInvincible() const { return GetTickCount64() < m_nInvincibleEnd; }   // 무적(8초)
```

버프를 **타이머 이벤트로 만들지 않고 "만료 시각"만 저장**한 게 포인트다.
버프 해제 이벤트를 예약할 필요 없이, 스탯을 물어볼 때마다 `now < end`로 판정하면 끝난다.
(클라는 `SC_BUFF`로 지속시간을 받아 **자체 타이머로 아이콘 쿨타임만** 그린다.)

### 11.3 사망 / 리스폰

- 플레이어 HP 0 → `m_bDead = true`, `PLAYER_DEAD` 브로드캐스트. 이후 **모든 행동 패킷이 거부**된다
  (`if (pPlayer->m_bDead) return;` 가 이동/공격/픽업/사용/구매 핸들러 앞단에 다 있다).
- `CS_RESPAWN` → `OnPlayerRespawn`: 시야에서 완전히 지웠다가(remove 전파) → HP 풀 회복 →
  스폰 지점(10,10) → **시야 재구축(add 전파)**.
  "지웠다 다시 넣는" 이유는 부활 위치가 멀어서 옛 시야가 전부 무효이기 때문이다.
- 몬스터 리스폰(5초): 스탯/위치/상태 전부 초기화 후, **시야 내 플레이어가 있으면 그 즉시 AI 재활성화**(CAS).

---

## 12. 서버 ⑧ 아이템 · 인벤토리 · 장비 · 버프

### 12.1 아이템 코드 규칙 (전 시스템 공통)

```
아이템 고유코드 = category × 1000 + subType

1xxx 포션   (0:HP중 1:HP대 2:MP중 3:MP대 4:공격력버프 5:무적)
2xxx 스크롤
3xxx 장비   (subType 0~38 → g_EquipTable 인덱스)
4xxx 기타(잡템)
9000 골드   (드롭에서만 사용, 인벤엔 안 들어감)
```

**이 규칙 하나가 프로젝트 전체를 단순하게 만들었다.**
아이템을 객체로 주고받지 않고 **정수 하나**로 주고받는다.
→ 패킷에 실기 쉽고, DB 컬럼 하나면 되고, 퀵슬롯에 등록해도 인벤이 재구성돼도 안 깨진다.
서버는 아이템의 **이름/아이콘을 모른다**(클라만 앎). 서버는 코드와 스탯만 안다.

### 12.2 인벤토리 — 동적 할당 없는 값 배열

```cpp
class CPlayer {
    static constexpr int32_t INVEN_SIZE = 40;
    int32_t m_invenCode[INVEN_SIZE]  = {};   // 0 = 빈 슬롯
    int32_t m_invenCount[INVEN_SIZE] = {};
    int32_t m_equipCode[EQUIP_SLOTS] = {};   // 6칸 (무기/갑옷/방패/투구/펜던트/반지)
    int32_t m_quickCode[QUICK_SLOTS_N] = {}; // 8칸
    int32_t m_gold = 0;

    bool AddItem(int32_t code, int32_t amount);      // 스택(포션/스크롤/기타)은 99까지 누적
    bool CanAddItem(int32_t code, int32_t amount) const;   // 비변경 사전 검사 (경매용)
    bool RemoveItemSlot(int32_t slot, int32_t cnt);
    bool Equip(int32_t invenSlot);                   // 기존 장비는 인벤으로 반환
    bool UnEquip(int32_t equipSlot);
    FUseResult UseItem(int32_t invenSlot);           // 포션: 회복/버프 + 수량 차감
};
```

`new` 없이 고정 배열이다. **(1)** 할당/해제 비용·단편화 없음 **(2)** 캐시 지역성 **(3)** 스냅샷 복사와
직렬화가 `memcpy` 수준으로 단순함. 슬롯 기반 인벤은 크기가 고정이라 값 배열이 자연스럽다.

`CanAddItem`이 따로 있는 이유는 **경매 구매** 때문이다. "원자적으로 매물을 확정한 뒤 지급이 실패"하면
아이템이 증발한다. 그래서 **확정 전에 인벤 여유를 미리 확인**한다. (14장)

### 12.3 드롭 — 존별 고정 풀

```cpp
struct FDrop { int32_t id, code, amount; float x, z; bool active; };

class CZone {
    static constexpr int32_t MAX_DROPS = 256;
    FDrop m_drops[MAX_DROPS];              // new 없음. active 플래그로 재사용
    std::mutex m_dropLock;
};
static std::atomic<int32_t> g_nextDropId{1};   // 존 공통 전역 ID (원자적 발급)
```

**흐름**: 몬스터 사망 → `RollDrop()` (30% 꽝 / 골드 40% / 포션 20% / 잡템 30% / 장비 10%)
→ `SpawnDrop` (빈 슬롯 선점) → `SC_ADD_DROP` 브로드캐스트
→ 플레이어가 **드롭에 접촉한 채 좌클릭** → `CS_PICKUP`
→ 서버 `OnPlayerPickup`: **드롭 슬롯을 `active=false`로 선점**(먼저 집는 사람이 임자) → 거리 검증(2타일)
→ `AddItem`/`AddGold` → `SC_REMOVE_DROP` + `SC_INVEN_UPDATE`

> **선점을 먼저 하는 게 중요하다.** 두 명이 동시에 같은 드롭을 클릭해도 `m_dropLock` 안에서
> `active`를 먼저 끈 쪽만 아이템을 받는다. 검증을 먼저 하고 선점을 나중에 하면 둘 다 통과할 수 있다.

### 12.4 장비 / 포션 / 버프 (Phase 2)

클라는 **의사만 보낸다**: `CS_EQUIP` / `CS_UNEQUIP` / `CS_USE_ITEM`.
서버가 인벤·장비·HP/MP·버프를 바꾸고 **스냅샷으로 회신**(`SC_INVEN_UPDATE` + `SC_PLAYER_HP` + `SC_BUFF`).

장비 스탯표(`g_EquipTable[39]`)와 포션 효과표(`g_PotionTable[6]`)는
**클라의 테이블과 값이 완전히 같아야 한다.** 클라는 표시용, 서버는 계산용으로 각자 들고 있다.

> **함정**: 클라 `POTION_TYPE` enum의 [4]무적/[5]공격 순서가 실제 데이터 테이블 순서([4]공격/[5]무적)와
> 뒤바뀌어 있었다. 서버는 **"플레이어가 보는 아이콘"이 기준**이므로 배열 순서(아이콘 기준)에 맞췄다.
> enum 이름이 아니라 **인덱스가 계약**이라는 걸 알려주는 사례.

---

## 13. 서버 ⑨ 상점

마을 NPC(`CNPC_Shop`) 클릭 → 상점 UI. 포션 6종 구매 / 전 아이템 판매.

```cpp
// 구매: 골드 검증 → 인벤 추가 → 골드 차감 → 스냅샷
int32_t nUnit = PotionBuyPrice(pPkt->itemCode);     // 0이면 판매 목록이 아님 → 거부
if (nUnit <= 0) return;
if (pPlayer->m_gold < nUnit * nCount) return;       // 골드 부족
if (!pPlayer->AddItem(pPkt->itemCode, nCount)) return;   // 인벤 가득
pPlayer->SpendGold(nUnit * nCount);
pZone->Send_InvenUpdate(pPlayer);
```

- **가격표는 서버가 정본**(`g_PotionBuyPrice`). 클라가 보낸 가격을 절대 믿지 않는다.
  클라가 "이거 1골드예요" 해도 서버가 자기 표를 본다.
- 판매가 = `ItemSellPrice(code)`: 포션은 구매가/2, 장비는 (공격+방어)×5, 스크롤 20, 잡템 5, 골드는 0(판매 불가).
- `AddItem`을 **먼저** 하고 `SpendGold`를 나중에 한다 — 인벤이 가득 차서 실패하면 골드가 안 빠진다.

---

## 14. 서버 ⑩ 경매장 (핵심)

**가장 공들인 시스템.** 인메모리 → DB 정본으로 마이그레이션하며 동시성 문제를 근본 해결했다.

### 14.1 왜 DB 정본(write-through)인가

경매 매물은 **계속 쌓이고, 오프라인 플레이어의 매물도 살아 있어야** 한다.
메모리에 들고 있으면 (1) 서버 재시작 시 전부 증발 (2) 매물이 늘수록 메모리 압박 (3) 검색/페이징을 직접 구현해야 한다.
→ **DB를 유일한 정본으로 두고, 모든 사건을 즉시 DB에 반영**한다. 서버는 캐시조차 두지 않는다.

> 초기 구현 `AuctionManager.h`(인메모리, 하드코딩 매물)는 **현재 어디에서도 include하지 않는 죽은 코드**다.
> DB 이전 후 통째로 대체됐다. 면접에서 "왜 갈아엎었나"를 설명할 수 있는 좋은 소재.

### 14.2 auction 테이블

```sql
CREATE TABLE auction (
    listing_id   INT AUTO_INCREMENT,   -- DB가 ID 발급 (서버가 ID 관리 안 함)
    item_code    INT NOT NULL,
    count        INT NOT NULL,         -- 남은 수량
    unit_price   INT NOT NULL,         -- 개당 가격
    pending_gold INT NOT NULL DEFAULT 0,  -- 팔렸지만 아직 판매자가 안 찾아간 골드
    seller_name  VARCHAR(20) NOT NULL,
    created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (listing_id)
);
```

- `listing_id DESC` = 최신 등록순. **PK 인덱스라 정렬이 공짜.**
- `pending_gold`를 매물 행에 누적한다 → 판매자가 오프라인이어도 골드가 안전하게 보관된다.
- `seller_name`에 FK를 걸지 않았다. 기본 매물의 판매자가 실계정이 아닌 `"경매장"`이기 때문.

### 14.3 등록 (에스크로)

```cpp
// 1) 인벤 검증 → 2) DB INSERT → 3) 성공했을 때만 인벤에서 차감
if (!pDB->Auction_Register(name, code, count, price)) return;   // INSERT 실패 시 아무 일도 안 일어남
pPlayer->RemoveItemSlot(nSlot, nCount);                          // 에스크로: 인벤에서 뺌
pZone->Send_InvenUpdate(pPlayer);
```

**순서가 중요하다.** 인벤에서 먼저 빼고 INSERT가 실패하면 **아이템이 증발**한다.
DB에 먼저 넣고, 확실히 들어간 뒤에 인벤에서 뺀다.

### 14.4 구매 — 동시 구매 방지 (가장 중요)

**문제**: 두 명이 거의 동시에 마지막 재고 1개를 사면?
순진하게 "읽고 → 확인 → 쓰기" 하면 **둘 다 확인을 통과**해서 오버셀/아이템 복사가 난다.

**해결: 원자적 조건부 UPDATE.**

```cpp
bool CDB_Manager::Auction_CommitBuy(int listingID, int qty, int total, const char* buyer) {
    nanodbc::prepare(s,
        "UPDATE auction SET count = count - ?, pending_gold = pending_gold + ? "
        "WHERE listing_id = ? AND count >= ? AND seller_name <> ?");   // ★ WHERE 절이 가드
    nanodbc::result r = nanodbc::execute(s);
    return r.affected_rows() >= 1;   // 1행 = 성공 / 0행 = 경쟁 탈락·재고부족·본인매물
}
```

`WHERE count >= qty`가 **가드**다. InnoDB가 이 행을 잠가 두 UPDATE를 **직렬화**하므로,
첫 번째만 1행이 반영되고 두 번째는 조건이 거짓이 되어 **0행 → 깔끔히 거부**된다.
락을 애플리케이션에서 잡을 필요가 없다. **DB가 이미 잘하는 일을 DB에 맡겼다.**

**그리고 전체 순서를 "지급이 실패할 수 없게" 배치했다.**

```
1) Auction_PeekBuy       (비변경 조회: 코드/개당가. 매물 없음·본인 매물이면 중단)
2) 사전 확인             ★ 골드 충분한가?  CanAddItem(인벤 여유 있는가)?
3) Auction_CommitBuy     ★ 원자적 UPDATE. 여기서 한 명만 성공
4) 지급                  AddItem + SpendGold   ← 3)이 성공한 뒤에만. 2)에서 확인했으니 실패 불가
5) SC_INVEN_UPDATE
```

> **2)를 3) 앞에 두는 게 핵심.** 순서를 바꾸면 "매물은 확정으로 차감됐는데 인벤이 가득 차서 못 준다" →
> **아이템 증발**. 반대로 지급을 먼저 하면 → **아이템 복사**. 이 순서만이 둘 다 막는다.

### 14.5 수령 / 취소

- **수령(Collect)**: `pending_gold`를 회수하고 0으로. **`count <= 0`(완판)이면 매물 행 자체를 삭제**.
  조회와 갱신을 **트랜잭션**으로 묶는다.
- **취소(Cancel)**: `PeekCancel`로 남은 수량 확인 → **`CanAddItem`으로 인벤 여유 사전 확인** →
  트랜잭션으로 DELETE하며 **삭제 시점의 실제 수량/골드를 반환** → 그걸 지급.
  "조회 시점과 삭제 시점 사이에 누가 사갔을 수 있으므로", 실제 지급은 **DELETE가 돌려준 값** 기준이다.
  (실제 수량 ≤ 조회 수량이므로 인벤에 반드시 들어간다.)

### 14.6 페이지네이션 + 검색

```cpp
// ORDER BY listing_id DESC  LIMIT (pageSize + 1)  OFFSET (page * pageSize)
//                                  ~~~~~~~~~~~~~ 하나 더 읽어서 hasNext 판정
// 탭별 WHERE:  구매 = seller_name <> me AND count > 0
//              내판매 = seller_name = me            (완판도 포함 — 골드 수령해야 하니까)
// 검색 시:     AND item_code IN (1000, 1001, ...)
```

**hasNext를 "한 개 더 읽어서" 판정**하는 게 트릭이다. `COUNT(*)` 쿼리를 한 번 더 치지 않아도 된다.

**검색이 재밌는 부분**: 서버는 **아이템 이름을 모른다**(코드만 안다).
그래서 클라가 검색어와 이름이 일치하는 **코드 목록을 만들어서** 보낸다.

```cpp
// 클라 CUI_Auction::Resolve_Search
for (카테고리 1~4)
  for (sub = 0; sub < 각 enum의 _END; ++sub) {
      int code = cat*1000 + sub;
      Item_Display(code, name, ...);              // 코드 → 이름 (클라만 가능)
      if (wcsstr(name, m_szSearch)) outCodes[cnt++] = code;   // 부분 문자열 일치
  }
// 일치하는 게 하나도 없으면 → 불가능 코드(-1)를 보내 결과 0건을 만든다
if (cnt == 0) { outCodes[0] = -1; cnt = 1; }
```

서버는 이 코드 목록으로 `item_code IN (...)` 필터만 건다.
**한계이자 개선점**: 아이템 이름 메타를 서버/DB로 옮기면 서버 단독 검색이 가능하다. (26장)

### 14.7 인코딩 문제

기본 매물의 판매자명이 한글(`"경매장"`)이다. DB는 `utf8mb4`, 클라는 `CP949`로 문자열을 디코드한다.
그래서 서버가 패킷에 담기 전에 **UTF-8 → CP949로 변환**한다.

```cpp
static void Utf8ToCp949(const std::string& utf8, char* out, size_t cap) {
    wchar_t wbuf[64];
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wbuf, 64);
    WideCharToMultiByte(949, 0, wbuf, -1, out, (int)cap, nullptr, nullptr);
}
```

플레이어 계정명은 ASCII라 두 인코딩에서 동일해 그대로 통과한다. 한글 이름만 변환된다.

---

## 15. 서버 ⑪ 레벨 / 경험치

### 15.1 설계

| 항목 | 값 |
|---|---|
| 필요 경험치 | `100 × 현재레벨` (1→2: 100, 2→3: 200 …) |
| 만렙 | 50 (도달 시 경험치 누적 중단, `maxExp=0`으로 보내 클라는 바를 꽉 채움) |
| 몬스터 보상 | 오크 20 / 윙 25 (`CMonster::m_nExpReward`) |
| 레벨업 보상 | MaxHP +20, MaxMP +10, 기본공격 +2, 기본방어 +1, **HP/MP 풀 회복** |

### 15.2 파생 스탯은 "증분"이 아니라 "레벨의 함수"

```cpp
void CPlayer::ApplyLevelStats() {          // 순수 함수. 레벨만 있으면 스탯이 결정된다
    int32_t n = m_nLevel - 1;
    m_iMaxHp  = 100 + n * 20;
    m_iMaxMp  = 100 + n * 10;
    m_baseAtk = 10  + n * 2;
    m_baseDef = 5   + n * 1;
    if (m_iHp > m_iMaxHp) m_iHp = m_iMaxHp;
    if (m_iMp > m_iMaxMp) m_iMp = m_iMaxMp;
}
```

**레벨업할 때도, DB에서 레벨을 불러올 때도 같은 함수를 부른다.**

레벨업 시 `m_iMaxHp += 20`처럼 증분으로 더하면, DB에서 레벨 5를 불러올 때
"MaxHP를 얼마로 세팅해야 하지?"를 **다시 계산해야 하고**, 그 계산식이 레벨업 코드와 어긋나면
**로그인할 때마다 스탯이 달라지는 버그**가 난다. 파생값을 순수 함수로 만들면 이 문제가 원천적으로 없다.

```cpp
int32_t CPlayer::AddExp(int32_t amount) {          // 반환 = 오른 레벨 수
    std::lock_guard lk(m_saveLock);                // 저장 스냅샷과 같은 락
    if (amount <= 0 || IsMaxLevel()) return 0;

    m_nExp += amount;
    int32_t gained = 0;
    while (!IsMaxLevel() && m_nExp >= ExpToNext(m_nLevel)) {   // 연속 레벨업 가능
        m_nExp -= ExpToNext(m_nLevel);
        ++m_nLevel;  ++gained;
    }
    if (gained > 0) { ApplyLevelStats(); m_iHp = m_iMaxHp; m_iMp = m_iMaxMp; }  // 풀회복
    if (IsMaxLevel()) m_nExp = 0;
    return gained;
}
```

### 15.3 흐름

```
몬스터 사망(막타 친 플레이어)
  → AddExp(monster->m_nExpReward)
  → SC_PLAYER_EXP (level, exp, maxExp, levelUp)
  → 레벨업했으면 SC_PLAYER_HP 도 함께   ★ MaxHP/MaxMP가 바뀌고 풀회복되므로
로그인
  → SetLevelExp(DB값) → ApplyLevelStats() → 풀피 입장
  → SC_PLAYER_EXP + SC_PLAYER_HP 각 1회
```

> **저장은 "현재 레벨에서 쌓은 경험치"**(누적 총량 아님)를 저장한다.
> 누적 총량을 저장하면 곡선을 바꿀 때 모든 유저의 레벨이 재계산돼야 한다.

---

## 16. 서버 ⑫ 퀵슬롯 — 유일하게 "클라 권위"인 것

이 프로젝트에서 **유일하게 서버 권위가 아닌 시스템**이다. 그 이유를 설명할 수 있어야 한다.

### 16.1 왜 서버 권위가 아닌가

퀵슬롯은 **"어느 아이템을 어느 칸에 등록해뒀나"** 하는 **표시 정보**일 뿐이다.
- 실제 아이템 사용은 이미 `CS_USE_ITEM`이 **따로 서버에서 검증**한다(인벤에 있는지, 사용 가능한지).
- 퀵슬롯을 조작해봐야 **얻을 수 있는 이득이 없다**. 없는 아이템을 등록해도 눌렀을 때 서버가 거부한다.
- 반면 서버 권위로 만들면 **왕복 지연 동안 UI가 굳거나 깜빡인다**(드래그했는데 반응이 늦음).

→ **클라가 정본, 서버는 저장소.** 다만 등록 내용이 계정에 남아야 하니 서버가 보관하고 DB에 쓴다.

### 16.2 흐름 (인벤과 정반대)

```
[인게임]  클라가 먼저 바꾸고 → 서버에 통보 (응답 없음, fire-and-forget)

  드래그 등록 / 우클릭 해제 / 아이템 소진으로 자동 해제
        ↓  전부 Set_Slot() 한 곳을 거침
  m_aSlotCode[i] = code   (화면 즉시 반영)
  SendQuickSlotSet(i, code) → CS_QUICKSLOT_SET
        ↓
  서버 CPlayer::SetQuickSlot()  — 검증만 하고 조용히 보관
        ↓
  주기 저장 / 로그아웃 시 DB quickslot 테이블에 기록

[로그인]  서버가 보내주고 → 클라가 받아서 세팅 (이 방향은 딱 한 번)

  SC_QUICKSLOT_UPDATE (codes[8])  ★ 반드시 SC_INVEN_UPDATE 뒤에
```

### 16.3 세 가지 구현 포인트

**(1) 변경 창구를 하나로 모았다.** 슬롯이 바뀌는 곳이 세 군데인데, 특히
`Update_SlotValidity()`(등록한 아이템이 인벤에서 사라지면 슬롯을 비움)는 **매 프레임 돈다.**
가드가 없으면 초당 수백 개의 패킷이 나간다.

```cpp
void CUI_QuickSlot::Set_Slot(int iSlot, int iCode) {
    if (m_aSlotCode[iSlot] == iCode) return;        // ★ 값이 실제로 바뀔 때만 전송
    m_aSlotCode[iSlot] = iCode;
    CNetwork_Manager::Get_Instance()->SendQuickSlotSet(iSlot, iCode);
}
```

**(2) 서버가 검증은 한다.** 클라 권위라고 아무거나 받지는 않는다.

```cpp
bool CPlayer::SetQuickSlot(int32_t slot, int32_t code) {
    if (slot < 0 || slot >= QUICK_SLOTS_N) return false;   // 범위
    if (code > 0) {
        int cat = code / 1000;
        if (cat != 1 && cat != 2) return false;            // 포션/스크롤만 (장비 거부)
    }
    m_quickCode[slot] = code;
    return true;
}
```
(실제로 장비 코드 3000과 슬롯 99를 보내는 테스트로 거부되는 걸 확인했다.)

**(3) 패킷 순서.** `SC_QUICKSLOT_UPDATE`를 인벤 스냅샷보다 **먼저** 보내면 안 된다.
클라는 등록된 코드를 **인벤에서 찾아 아이콘을 그리므로**, 인벤이 비어 있는 상태에서 복원하면
`Update_SlotValidity`가 "인벤에 없는 아이템이네" 하고 **슬롯을 전부 지워버린다.**

### 16.4 클라 수신 — 캐시 + 버전 (경매장과 같은 패턴)

`Handle_SC_*`는 이미 메인 스레드(`Dispatch()`)에서 돈다. 문제는 **UI 객체를 잡기가 마땅찮다는 것**이다.
플레이어는 `CObject_Manager::Get_Player()`로 바로 잡히지만, 퀵슬롯 UI는 `CUI_Manager`의
리스트 안에 `CUI*`로 들어 있어 꺼내려면 리스트 조회 + `dynamic_cast`가 필요하다.

→ 네트워크는 **캐시에 담고 버전만 +1**, UI는 매 프레임 **버전이 바뀌었으면 한 번 가져간다(pull)**.

```cpp
// CNetwork_Manager
void Handle_SC_QUICKSLOT_UPDATE(...) { memcpy(m_quickCode, pkt->codes, ...); ++m_quickVersion; }

// CUI_QuickSlot::Update()
void Sync_FromServer() {
    uint32_t v = pNet->GetQuickVersion();
    if (v == m_nQuickVersion) return;              // 새 스냅샷 없음 → 아무것도 안 함
    memcpy(m_aSlotCode, pNet->GetQuickCodes(), ...);
    m_nQuickVersion = v;                           // "여기까지 반영했다"
}
```

버전 없이 매 프레임 무조건 복사하면, 플레이어가 슬롯을 바꿔도 **다음 프레임에 옛 스냅샷으로 되돌아간다.**
경매장(`m_auctionVersion`)이 쓰던 패턴을 그대로 따랐다.

---

## 17. DB 연동 전체

### 17.1 스택 결정

| 선택 | 이유 |
|---|---|
| **MySQL 8.0** | 무료, InnoDB 행 잠금(경매 동시성에 필요), 자료 풍부 |
| **ODBC** | 표준 계층. DB 엔진이 바뀌어도 애플리케이션 코드는 그대로(드라이버·방언만 교체) |
| **nanodbc** | ODBC C API를 C++ 클래스 3개(`connection`/`statement`/`result`)로 감싼 얇은 래퍼. **소스 2파일** — 프로젝트에 그냥 넣고 `odbc32.lib`만 링크 |
| **connection-per-call** | 호출마다 연결/해제. 로그인·저장·경매는 **이벤트성**이라 충분. 부하가 커지면 풀로 승격 |

```cpp
m_connStr = "Driver={MySQL ODBC 9.7 Unicode Driver};Server=127.0.0.1;Port=3306;"
            "Database=mmorpg;User=mmo_server;Password=1234;CHARSET=utf8mb4;";
```
(DSN 없는 "DSN-less" 연결 문자열. 드라이버 이름이 하드코딩이라 **ODBC 9.7이 정확히 깔려야** 한다.)

### 17.2 스키마 (6 테이블)

```sql
account    (account_id PK, password, created_at)
character  (account_id PK/FK, zone_id, spawn_x, spawn_z, gold, level, exp)
inventory  (account_id+slot PK, item_code, count)      -- 40칸 중 채워진 것만
equipment  (account_id+slot PK, item_code)             -- 6칸 중 착용한 것만
quickslot  (account_id+slot PK, item_code)             -- 8칸 중 등록한 것만
auction    (listing_id PK AUTO_INCREMENT, item_code, count, unit_price,
            pending_gold, seller_name, created_at)
```

- `character`는 `account_id`를 **PK 겸 FK**로 써서 "계정=캐릭터 1:1"을 **구조로 강제**했다.
- 전부 `ON DELETE CASCADE` — 계정 삭제 시 캐릭터/인벤/장비/퀵슬롯이 자동 정리된다.
- 인벤/장비/퀵슬롯은 **채워진 칸만 행으로 저장**한다(빈 칸은 행 없음). 40칸 중 3칸만 차면 3행.

### 17.3 로그인 — 저장 프로시저 `sp_login` (결과셋 4개)

```sql
CREATE PROCEDURE sp_login(IN p_id VARCHAR(20), IN p_pw VARCHAR(64))
BEGIN
    DECLARE v_ok INT DEFAULT 0;
    SELECT COUNT(*) INTO v_ok FROM account WHERE account_id=p_id AND password=p_pw;
    IF v_ok = 0 THEN SET p_id = NULL; END IF;        -- ★ 실패 시 이후 조회가 전부 0행

    SELECT zone_id, spawn_x, spawn_z, gold, level, exp FROM `character` WHERE account_id=p_id;
    SELECT slot, item_code, count FROM inventory  WHERE account_id=p_id ORDER BY slot;
    SELECT slot, item_code        FROM equipment  WHERE account_id=p_id ORDER BY slot;
    SELECT slot, item_code        FROM quickslot  WHERE account_id=p_id ORDER BY slot;
END
```

**`p_id = NULL` 트릭이 포인트.** 인증에 실패하면 조회 키를 NULL로 만들어 **네 결과셋 모두 0행**이 되게 한다.
그러면 C++ 쪽은 성공/실패 분기 없이 **항상 "결과셋 4개"라는 단일 구조**로 처리하고,
"첫 결과셋에 행이 있나?"만 보면 된다.

```cpp
nanodbc::prepare(stmt, NANODBC_TEXT("{CALL sp_login(?, ?)}"));
stmt.bind(0, id); stmt.bind(1, pw);
nanodbc::result r = nanodbc::execute(stmt);

if (!r.next()) return false;                    // 0행 = 인증 실패
out.zoneID = r.get<int>(0); ... out.level = r.get<int>(4); out.exp = r.get<int>(5);

if (r.next_result()) { while (r.next()) { /* inventory */ } }
if (r.next_result()) { while (r.next()) { /* equipment */ } }
if (r.next_result()) { while (r.next()) { /* quickslot */ } }
```

### 17.4 저장 — 트랜잭션

```cpp
bool CDB_Manager::Save(const FSaveSnapshot& snap) {
    nanodbc::connection conn(m_connStr);
    nanodbc::transaction tx(conn);        // ★ commit 없이 소멸되면 자동 롤백

    // 1) character UPDATE (zone, x, z, gold, level, exp)
    // 2) inventory : DELETE 후 채워진 칸만 INSERT   (스냅샷 방식)
    // 3) equipment : DELETE 후 착용한 것만 INSERT
    // 4) quickslot : DELETE 후 등록한 칸만 INSERT

    tx.commit();                          // 전부 성공해야 확정
    return true;
}
```

**DELETE→INSERT(스냅샷)로 하는 이유**: 인벤 40칸 중 무엇이 바뀌었는지 델타를 추적하는 것보다,
**통째로 지우고 다시 쓰는 게 단순하고 틀릴 여지가 없다.** 트랜잭션이 있으니 중간에 실패해도
"인벤이 반쯤 지워진" 상태가 남지 않는다. 예외가 나면 `tx` 소멸자가 **자동 롤백**한다.

### 17.5 언제 저장하나 — 두 시점

**(1) 로그아웃 저장** — `CSession::Disconnect()`

```cpp
if (pPlayer->m_szName[0] != '\0') {        // 로그인 성공한 플레이어만
    FSaveSnapshot snap;
    pPlayer->TakeSnapshot(snap);
    CDB_Manager::Get_Instance()->Save(snap);
}
CZone::LeaveZone(pPlayer);                  // ★ 저장을 remove보다 먼저
```

**(2) 주기 자동저장 (5초)** — 크래시 대비. **기존 타이머 큐를 그대로 재사용**했다.

```cpp
// 워커가 PlayerAutoSave 이벤트를 받으면
CPlayer_Manager::Get_Instance()->AutoSaveNext();          // 온라인 1명만
AddTimer(0, EEventType::PlayerAutoSave, 5000);            // 다음 틱 재등록
```

**한 틱에 1명씩 라운드로빈**으로 저장한다. 100명을 한 번에 저장하면 그 워커가 수 초간 블로킹된다.
커서(`m_saveCursor`)를 돌리며 매 틱 한 명씩 → 100명이면 각자 500초마다 저장되는 셈.
(간격을 줄이거나 전용 DB 스레드로 옮기면 되지만, 지금 규모에선 이게 가장 단순하다.)

### 17.6 스냅샷-언더-락 (데이터 레이스 해결)

**문제**: 주기 저장 대상은 **살아서 플레이 중인** 플레이어다.
타이머 워커가 인벤을 읽는 동안 게임 워커가 아이템을 주우면 → **반쪽 상태(torn snapshot)** 가 저장된다.

**해결**: 인벤/골드/장비/퀵슬롯/레벨을 **바꾸는 모든 메서드**와 `TakeSnapshot`이 **같은 락**을 공유하되,
**락 안에서는 값 복사만** 하고(수 마이크로초), **느린 DB I/O는 락을 푼 뒤** 복사본으로 수행한다.

```cpp
mutable std::recursive_mutex m_saveLock;   // ★ recursive인 이유는 아래

void CPlayer::TakeSnapshot(FSaveSnapshot& s) const {
    std::lock_guard<std::recursive_mutex> lk(m_saveLock);
    s.gold = m_gold;  s.level = m_nLevel;  s.exp = m_nExp;  s.zoneID = m_nZoneID; ...
    for (int i=0;i<INVEN_SIZE;++i){ s.invenCode[i]=m_invenCode[i]; s.invenCount[i]=m_invenCount[i]; }
    for (int i=0;i<EQUIP_SLOTS;++i)   s.equipCode[i] = m_equipCode[i];
    for (int i=0;i<QUICK_SLOTS_N;++i) s.quickCode[i] = m_quickCode[i];
}   // ← 여기서 락 해제

void CPlayer_Manager::AutoSaveNext() {
    PlayerRef target;
    { std::lock_guard lk(m_lock); /* 커서 전진하며 온라인 1명 shared_ptr 복사 */ }
    if (!target) return;

    FSaveSnapshot snap;
    target->TakeSnapshot(snap);                        // 짧은 락 (복사만)
    CDB_Manager::Get_Instance()->Save(snap);           // ★ 느린 I/O는 락 밖
}
```

**왜 `recursive_mutex`인가?** `Equip()`이 내부에서 `AddItem()`(기존 장비를 인벤으로 반환)을 호출한다.
둘 다 같은 락을 잡으므로 일반 `mutex`면 **자기 자신에게 데드락**이 걸린다.

### 17.7 마이그레이션 관리

`schema.sql`은 `CREATE TABLE IF NOT EXISTS`라서 **이미 존재하는 테이블에 컬럼을 추가하지 못한다.**
그래서 기능을 추가할 때마다 마이그레이션 파일을 따로 만든다.

- `db/migration_2026-07-14_exp.sql` — `character`에 `level`/`exp` 컬럼 추가
  (`information_schema`로 컬럼 존재를 먼저 확인해 **재실행 안전**하게 작성)
- `db/migration_2026-07-14_quickslot.sql` — `quickslot` 테이블 추가

> 마이그레이션 후 `procedures.sql`을 **다시 실행**해야 한다. `sp_login`이 새 컬럼/결과셋을 반환하도록 바뀌므로.

---

## 18. 클라이언트 ① 구조와 게임 루프

### 18.1 게임 루프 (144FPS 고정)

```cpp
timeBeginPeriod(1);                       // 타이머 해상도 1ms로 (Sleep 정확도)
while (true) {
    while (PeekMessage(...)) { ... }      // 메시지 우선 처리 (블로킹 GetMessage 아님)

    CTimer_Manager::Update();             // QueryPerformanceCounter 기반 dt
    float dt = ...->Get_DeltaTime();

    MainApp.Update(dt);                   // ★ 여기서 Network Dispatch() 먼저
    MainApp.Late_Update(dt);              // 충돌/카메라 등 후처리
    MainApp.Render();                     // D2D BeginDraw ~ EndDraw

    while (elapsed < targetFrameTime) {   // 프레임 고정 (스핀 + Sleep(1) 혼합)
        if ((targetFrameTime - elapsed) > 0.002f) Sleep(1);   // 2ms 이상 남으면 양보
    }                                                          // 아니면 스핀
}
```

**`Sleep(1)`과 스핀을 섞은 이유**: `Sleep`만 쓰면 정확도가 부족해 프레임이 튀고,
스핀만 쓰면 CPU를 100% 태운다. **2ms 이상 남았을 때만 `Sleep(1)`로 양보**하고 마지막은 스핀으로 맞춘다.

### 18.2 Update / Late_Update 분리

```cpp
void CMainApp::Update(float dt) {
    CNetwork_Manager::Get_Instance()->Dispatch();     // ★ 1) 서버 패킷을 먼저 반영
    CInput_Manager::Get_Instance()->Update();         //   2) 입력 상태 갱신
    CLevel_Manager::Get_Instance()->Update(dt);       //   3) 오브젝트 로직
}
void CMainApp::Late_Update(float dt) {
    CLevel_Manager::Get_Instance()->Late_Update(dt);  //   4) 충돌/카메라/정렬 등 후처리
}
```

**패킷 반영이 가장 먼저**다. 서버 상태를 먼저 프레임에 반영한 뒤 로컬 로직을 돌려야
"내 입력이 서버 상태를 덮어쓰는" 순서 문제가 안 생긴다.

### 18.3 매니저 싱글턴 (`Get_Instance` / `Destroy_Instance`)

| 매니저 | 책임 |
|---|---|
| `CLevel_Manager` | `LEVEL_MENU / LOGIN / CHOICE / TEST` 상태 전환 |
| `CObject_Manager` | `m_ObjectList[OBJ_END]` — PLAYER/OTHER_PLAYER/MONSTER/NPC/PORTAL/DROP. **렌더 Y소팅** |
| `CUI_Manager` | `m_UIList[UI_END]` — HUD/인벤/퀵슬롯/상점/경매/로그인박스 |
| `CImg_Manager` | PNG/BMP 캐시(`map<key, ID2D1Bitmap*>`), DirectWrite 폰트 |
| `CInput_Manager` | 키/마우스 Down·Up 엣지 검출, 드래그 상태, 커서 모드, **게임모드 여부** |
| `CMap_Manager` | 현재 존의 타일맵 + `Is_Movable`, **비동기 존 전환** |
| `CCollision_Manager` | 플레이어↔오브젝트 충돌 |
| `CTimer_Manager` | dt 계산 |
| `CNetwork_Manager` | 소켓 + recv 스레드 + 태스크 큐 |
| `CCamera` | 아이소 좌표 변환 + 타겟 추적 |

### 18.4 오브젝트 계층

```
CGameObject (추상: Initialize/Update/Late_Update/Render/Release 순수가상)
 ├ CPlayer          — 내 캐릭터 (입력, 길찾기, 인벤/장비 보유, 버프 타이머)
 ├ COther_Player    — 다른 플레이어 (서버 패킷으로만 움직임, 보간)
 ├ CMonster ─ CMonster_Orc / CMonster_Wing
 ├ CNPC ─ NPC_Shop / NPC_Market / NPC_Angel / NPC_Knight / NPC_OldMan
 ├ CPortal          — 클릭 시 CS_PORTAL
 ├ CDropItem        — 월드 드롭 아이콘
 └ CStaticObject ─ 텐트/마차/대장간/나무 등 (마을 장식 + 블록)
```

공통 상태: `ISO_INFO{fWorldX, fWorldZ, fHeight}`, `COLLIDER`, `MOUSE_COLLIDER`, `FRAME`(애니메이션),
`DIRECTION`(8방위), HP/MP/**레벨/경험치**.

---

## 19. 클라이언트 ② 아이소메트릭 좌표계와 렌더 정렬

### 19.1 좌표 변환 (2:1 다이아몬드)

타일은 `TILE_WIDTH=160`, `TILE_HEIGHT=80` — **가로가 세로의 2배**인 마름모.

```cpp
// 월드(논리 타일 좌표) → 스크린
POINT CCamera::IsoWorldToScreen(float wx, float wz) {
    pt.x = (LONG)((wx - wz) * TILE_HALF_W - m_fX);      // 64
    pt.y = (LONG)((wx + wz) * TILE_HALF_H - m_fY);      // 32
}

// 스크린 → 월드 (역변환: 마우스 피킹에 필수)
void CCamera::ScreenToIsoWorld(int sx, int sy, float& wx, float& wz) {
    float mapX = sx + m_fX;
    float mapY = sy + m_fY + TILE_HALF_H;               // 상단꼭지점 → 중심 보정
    wx = (mapX / TILE_HALF_W + mapY / TILE_HALF_H) / 2.f;
    wz = (mapY / TILE_HALF_H - mapX / TILE_HALF_W) / 2.f;
}
```

`(wx - wz)`가 화면 X, `(wx + wz)`가 화면 Y가 되는 게 아이소메트릭의 전부다.
역변환은 이 연립방정식을 푼 것. **마우스 클릭 → 어느 타일인가**를 알려면 반드시 필요하다.

### 19.2 8방위 계산 — 서버와 클라가 같은 식을 쓴다

```cpp
// 월드 방향 벡터를 "화면상 각도"로 바꿔서 8방위를 정한다
float screenDX = (nx - nz) * TILE_HALF_W;      // 아이소 변환을 방향 벡터에도 적용
float screenDY = (nx + nz) * TILE_HALF_H;
float angle = atan2f(screenDY, screenDX) * 180.f / PI;
// -22.5~22.5 → R(6),  22.5~67.5 → RB(7),  67.5~112.5 → B(0) ...
```

**월드 좌표에서 바로 각도를 재면 안 된다.** 화면이 2:1로 눌려 있어서, 월드에서 45°인 방향이
화면에선 45°가 아니다. 그래서 **아이소 변환을 거친 뒤** `atan2`를 쓴다.
서버 `OnMoveDest`/`OnPlayerAttackMonster`에도 **똑같은 상수(64/32)와 똑같은 식**이 들어 있다.

### 19.3 렌더 정렬 (Y소팅)

```cpp
sort(vecSortList.begin(), vecSortList.end(),
     [](CGameObject* a, CGameObject* b) { return a->Get_SortDepth() < b->Get_SortDepth(); });
```

깊이는 **발밑(콜라이더 중심)** 기준이다. 스프라이트의 중심이나 상단을 쓰면
"머리가 큰 캐릭터가 뒤에 있는데 앞으로 보이는" 문제가 생긴다.
포탈처럼 예외적인 것은 `Set_SortOffset()`으로 보정한다.

---

## 20. 클라이언트 ③ 네트워크 — 스레드 분리와 태스크 큐

### 20.1 구조

```
[recv 스레드]                          [메인 스레드]
recv() 블로킹
  → 패킷 경계 파싱 (서버와 동일 루프)
  → ProcessPacket(id)
  → PushTask([=]{ Handle_SC_XXX(data); })  ──→  m_taskQueue (mutex)
                                                     │
                                          매 프레임 CMainApp::Update()
                                                     ▼
                                          Dispatch(): 큐를 로컬로 통째로 옮기고
                                                     (락 최소화) 하나씩 실행
```

```cpp
void CNetwork_Manager::PushTask(std::function<void()> h) {
    std::lock_guard lock(m_queueLock);
    m_taskQueue.push({ h });
}
void CNetwork_Manager::Dispatch() {
    std::queue<FPacketTask> local;
    { std::lock_guard lock(m_queueLock); local.swap(m_taskQueue); }   // ★ 락 짧게
    while (!local.empty()) { local.front().m_handler(); local.pop(); }
}
```

**왜 이렇게 하나?** **D2D는 싱글스레드 렌더 모델**이다. recv 스레드에서 직접 오브젝트/UI를 만들거나
지우면 렌더 중인 메인 스레드와 충돌해 크래시한다. 큐를 거치면 **모든 핸들러가 메인 스레드에서만** 실행된다.

**패킷 데이터를 `std::vector<uint8_t>`로 복사해 람다에 캡처**하는 것도 중요하다.
`m_recvBuf`는 다음 `recv`에서 덮어써지므로 포인터를 캡처하면 쓰레기를 읽는다.

### 20.2 `TCP_NODELAY`

```cpp
BOOL bNoDelay = TRUE;
setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&bNoDelay, sizeof(bNoDelay));
```

Nagle 알고리즘은 작은 패킷을 모아서 보낸다. **게임에선 치명적**이다.
"이동 클릭" 같은 40바이트 패킷이 최대 200ms 지연될 수 있다. 반드시 꺼야 한다.

### 20.3 UI가 네트워크 상태를 폴링하는 곳

- **로그인 실패**: `CNetwork_Manager::Is_LoginFailed()` → `CUI_LoginBox`가 매 프레임 확인 → 메시지 표시 후 `Clear`
- **경매 목록**: `GetAuctionVersion()` 이 바뀌면 UI가 다시 그림
- **퀵슬롯**: `GetQuickVersion()` 이 바뀌면 한 번 가져감
- **스폰 준비**: `IsSpawnReady()` / `GetStartZone()` — 로그인 성공 후 레벨 진입 시 서버가 준 존/좌표 사용

---

## 21. 클라이언트 ④ 보간 / 스냅 / 애니메이션

### 21.1 다른 플레이어 — 보간 + 텔레포트 임계값

```cpp
void COther_Player::OnMoveDestPacket(float cx, float cz, float dx, float dz, float speed, uint32_t t) {
    float diff = dist(현재 렌더 위치, 서버가 준 cx/cz);

    constexpr float TELEPORT_THRESHOLD = 3.f;
    if (diff > TELEPORT_THRESHOLD) {          // 오차가 3타일 넘으면 → 순간이동으로 강제 동기화
        m_tIsoInfo.fWorldX = cx;  m_tIsoInfo.fWorldZ = cz;
    }
    // 3타일 이하면 → 현재 위치는 그대로 두고 목적지만 갱신 (부드럽게 따라감)
    m_fDestX = dx;  m_fDestZ = dz;  m_fSpeed = speed;  m_bMoving = true;
    Motion_Change(PLAYER_WALK);
}
```

**작은 오차는 무시하고 목적지만 갱신** → 캐릭터가 부드럽게 이동한다.
**큰 오차는 즉시 스냅** → 지연/패킷 유실로 완전히 어긋난 상태를 복구한다.
이 두 가지를 안 나누면 "고무줄처럼 튀거나(항상 스냅)" "영영 어긋난 채 걷는다(항상 보간)".

### 21.2 상태 패킷이 이동을 이긴다

`OnStatePacket`에서 HIT/ATTACK 상태가 오면 **`m_bMoving = false`로 이동을 끊는다.**
그리고 `Move_To_Dest`는 HIT/ATTACK/DEAD 중일 때 **애니메이션을 override 하지 않는다.**

이걸 안 하면 "맞고 있는데 계속 걸어가는" 몬스터가 나온다(실제로 겪은 버그).

### 21.3 애니메이션

- `FRAME{ iStart, iEnd, iMotion, dwTime, dwSpeed }` + `Move_Frame()`이 시간 기반으로 프레임 전진
- 방향 8종 × 상태(IDLE/WALK/ATTACK/HIT/DEAD)마다 별도 PNG 시트
- `m_bLoopAnim = false` + `Check_AnimEnd()` → 공격/사망처럼 **한 번만 재생**하는 모션 처리
- 버프 아이콘: 서버가 준 `durationMs`로 **클라가 자체 타이머**를 돌려 검은 오버레이 비율을 그림

---

## 22. 클라이언트 ⑤ UI 시스템

`CUI` 추상 기반 + `CUI_Manager`가 `m_UIList[UI_END]`로 관리.

| UI | 기능 |
|---|---|
| `CUI_LoginBox` | ID/PW 입력(`WM_CHAR` → `On_Char` 체인), 로그인 실패 표시 |
| `CUI_HUD` | HP/MP/**EXP 바** + **Lv.N** + 버프 아이콘(쿨타임 오버레이) |
| `CUI_Inventory` | 40칸 그리드 + 장비 6칸. **드래그**, 우클릭 장착/사용, 골드 표시 |
| `CUI_QuickSlot` | 8칸(0~3 소비, 4~7 스킬 예정). 드래그 등록, 우클릭 해제, 숫자키 1~4 사용 |
| `CUI_Shop` | 포션 구매/판매 + 수량 다이얼로그 |
| `CUI_Auction` | 3탭(구매/내판매/등록), 페이지네이션, **검색**, 확인 팝업 |
| `CUI_QtyDialog` | 수량 입력 |
| `CUI_ConfirmDialog` | 등록/취소 확인 |

**퀵슬롯이 "코드 기반"인 게 중요하다.** 포인터(`CItemData*`)를 저장하면
`SC_INVEN_UPDATE`로 인벤이 재구성될 때마다 **댕글링 포인터**가 된다.
아이템 **코드(int)** 를 저장하니 인벤이 통째로 다시 만들어져도 등록이 유지된다.
(아이템이 실제로 다 떨어졌을 때만 `Update_SlotValidity`가 슬롯을 비운다.)

**HP/MP/EXP 바 렌더**: 원본 비트맵을 비율만큼 **잘라서** 그린다(늘이지 않는다).

```cpp
pRT->DrawBitmap(pBitmap,
    D2D1::RectF(fX, fY, fX + fW * fRatio, fY + fH),                  // 목적 사각형
    1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
    D2D1::RectF(0, 0, fBitmapW * fRatio, fBitmapH));                 // ★ 소스도 같은 비율로 자름
```

---

## 23. 서버-클라 "반드시 일치해야 하는 것" 목록

면접에서 "분산 시스템에서 뭐가 어려웠나" 물으면 이 얘기를 하면 된다.
**클라와 서버가 같은 값을 각자 들고 있는 곳**이 여럿 있고, 어긋나면 조용히 깨진다.

| 대상 | 클라 | 서버 | 어긋나면 |
|---|---|---|---|
| 패킷 구조 | `Protocol.h` | `Protocol.h` (복제본) | **직렬화 붕괴** — 즉시 터짐 |
| enum 숫자값 | `PLAYER_STATE`, `DIRECTION`, `MONSTER_STATE` | 동일 | 엉뚱한 애니메이션/방향 |
| 필드 블록맵 | `CZone_*::Build()` | `BLOCK_MAP_FIELD_*` | **이동 검증 불일치** — 벽에 끼거나 튕김 |
| 마을 오브젝트 블록 | `CZone_Town::Build()` s_block | `Zone_Manager` townBlock | 위와 같음 |
| 장비 스탯표 | `s_EquipTable[39]` | `g_EquipTable[39]` | 표시 공격력 ≠ 실제 데미지 |
| 포션 효과표 | `s_PotionTable[6]` | `g_PotionTable[6]` | 아이콘과 효과가 다름 |
| 상점 가격 | `s_ShopPotions.price` | `g_PotionBuyPrice` | 표시가와 결제액 불일치 |
| 아이템 코드 규칙 | `cat*1000+sub` | 동일 | 전부 깨짐 |
| 몬스터 MaxHP | `CMonster_Orc/Wing::Initialize` | `Monster_Manager::Create` | **HP바가 안 사라지거나 잘못 참** |
| 퀵슬롯 칸 수 | `m_aSlotCode[8]` | `QUICK_SLOTS_N = 8` | 스냅샷 크기 불일치 |
| 경험치 곡선 초기값 | `Player.cpp` 생성자 | `ExpToNext(1)` | 서버 패킷 오기 전 잠깐 어긋남 |

> **`TILE_TYPE`은 의도적으로 다르다.** 클라 `TILE_BLOCK=10`, 서버 `TILE_BLOCK=1`.
> 타일 값 자체를 주고받지 않기 때문에 문제가 없지만, 나중에 타일을 동기화하려면 매핑이 필요하다.
> — **이런 걸 문서(CLAUDE.md)에 명시적으로 적어두는 것**도 협업에선 중요하다.

---

## 24. 예상 면접 질문 & 답변 (25문)

### 네트워크 / 동시성

**Q1. 왜 IOCP인가?**
Windows에서 수천 커넥션을 소수 스레드로 처리하는 확장형 비동기 I/O이기 때문이다.
`select`는 fd 수에 비례한 O(N) 스캔이 들고, 커넥션당 스레드는 컨텍스트 스위칭과 스택 메모리가 폭발한다.
IOCP는 커널이 완료를 큐로 넘겨주고 워커 풀이 꺼내 쓰므로 **커넥션 수와 스레드 수가 분리**된다.
워커는 `hardware_concurrency()`개만 띄운다.

**Q2. `CIOEvent`를 왜 만들었나?**
`GetQueuedCompletionStatus`는 `OVERLAPPED*`만 돌려준다. "이게 recv 완료인지 send 완료인지"를 알 수 없다.
`WSAOVERLAPPED`를 **첫 멤버**로 둔 구조체를 만들면 포인터를 그대로 캐스팅해 `IOType`을 읽을 수 있다.
덕분에 **소켓 I/O와 게임 이벤트(AI/리스폰/타격/저장)가 같은 워커 루프**를 탄다. 게임용 스레드가 따로 없다.

**Q3. 왜 객체마다 타이머 스레드를 두지 않았나?**
"몬스터 1000마리 = 스레드 1000개"가 되면 컨텍스트 스위칭으로 죽는다.
**단일 타이머 스레드**가 우선순위 큐에서 만료된 이벤트만 꺼내 `PostQueuedCompletionStatus`로 IOCP에 넣고,
남는 워커가 처리한다. **이벤트 수는 늘어도 스레드 수는 고정**이다.

**Q4. 세션 수명은 어떻게 관리했나? (dangling I/O)**
`shared_ptr`로 관리한다. IOCP 완료는 비동기로 늦게 도착하므로, 진행 중인 I/O가 남았는데 세션을 파괴하면
워커가 죽은 메모리를 만진다. 참조가 살아있는 동안 세션이 유지된다.
`Disconnect`는 `m_connected.exchange(false)`로 **중복 진입을 막았다** — 여러 워커가 동시에 끊김을 감지할 수 있다.

**Q5. TCP 패킷 경계는 어떻게 처리했나?**
TCP는 스트림이라 뭉치거나 잘려 온다. 세션이 **잔여 바이트(`m_prevRemain`)** 를 들고 있다가
다음 수신분과 이어 붙이고, **헤더의 `size`만큼 완전히 도착한 패킷만** 처리한다.
남은 조각은 `memmove`로 버퍼 앞에 당겨두고 다음 `WSARecv`를 그 뒤에 건다. 클라도 같은 루프다.

**Q6. send 버퍼를 왜 큐로 직렬화했나?**
세션당 send 버퍼와 오버랩 구조체가 **하나뿐**이다. 앞의 `WSASend`가 완료되기 전에 같은 버퍼로 또 보내면
데이터가 덮인다. 그래서 "전송 중이면 큐에 쌓고, 완료 통지가 오면 다음 것을 보낸다"로 순차화했다.

**Q7. 락 전략은?**
자원별로 쪼갰다(존 집합 / 시야 리스트 / 송신 큐 / 타이머 큐 / 드롭 풀 / 저장 대상).
원칙은 **"락은 짧게, 느린 I/O는 락 밖에서"**. 특히 DB 저장은 락 안에서 절대 하지 않는다.
몬스터 AI 활성화는 락 대신 **CAS**(`compare_exchange_strong`) 한 줄로 중복을 막았다 —
두 플레이어가 동시에 시야에 진입해도 타이머가 한 번만 걸린다.

### 게임 로직 / 서버 권위

**Q8. 서버 권위를 어디까지 적용했나?**
이동 검증, 시야, 데미지/HP/사망, 인벤토리, 장비/스탯, 드롭, 상점 가격, 경매 재고, 경험치 —
**결과에 영향을 주는 모든 것**. 클라는 "의사(클릭/구매/사용)"만 보내고 결과를 스냅샷으로 받는다.
이유는 단순하다. **클라는 신뢰할 수 없다.** 인벤을 클라가 계산하면 아이템 복사·골드 조작이 가능하다.

**Q9. 그런데 퀵슬롯만 클라 권위인 이유는?**
퀵슬롯은 **표시 정보**일 뿐이고, 조작해도 **얻을 이득이 없다**. 실제 사용은 `CS_USE_ITEM`이 따로 검증하므로
없는 아이템을 등록해봐야 눌렀을 때 서버가 거부한다. 반대로 서버 권위로 만들면 드래그할 때마다
왕복 지연 동안 UI가 굳는다. **보안 이득이 0인데 UX 비용만 드는 경우**라 클라 권위로 뒀다.
다만 서버가 **범위와 카테고리는 검증**하고(장비는 퀵슬롯에 못 넣는다), 계정에 남아야 하니 DB에 저장한다.
"서버 권위는 원칙이지만, **무엇을 지키려는지**를 먼저 묻고 적용해야 한다"는 게 배운 점이다.

**Q10. 위치를 매 틱 저장하지 않고 역산하는 이유는?**
**이동 시작 위치·목적지·속도·시작 시각** 네 개만 있으면 임의 시각의 위치를
`시작 + 방향×속도×경과시간`으로 계산할 수 있다. 매 틱 갱신하는 루프 자체가 없어진다.
그리고 이 함수(`GetCurrentPos`) 하나가 **몬스터 사거리, 어그로, 픽업 거리, 해킹 검증, 도착 판정**에
전부 재사용된다. 상태를 최소화하고 계산으로 대체한 설계다.

**Q11. 이동 핵(텔레포트/속도핵)은 어떻게 막나?**
클라가 보고한 위치를 서버 역산 위치와 비교해 **2타일을 넘으면 버리고 서버 값을 채택**한다.
목적지 타일이 통과 불가면 이동 자체를 거부하고 되돌린다.
**오차를 0으로 두지 않은 이유**는 네트워크 지연과 클라 로컬 예측 때문에 정상 상황에서도 차이가 나기 때문이다.
반응성과 치팅 차단의 절충점이다.

**Q12. 몬스터 길찾기는 왜 서버인가? 플레이어는 클라가 계산하던데?**
**둘 다 최종 권위는 서버**다. 차이는 "누가 의도를 만드느냐"다.
플레이어 이동은 사람의 입력이 원천이라 클라가 목적지를 정하고 **서버가 검증**한다(즉시 반응해야 하니까).
몬스터는 **입력 주체가 없다.** AI가 곧 로직이고, 그 로직이 클라에 있으면 (1) 클라마다 경로가 달라져 동기화가 안 되고
(2) 핵으로 몬스터를 유리하게 움직일 수 있다. 그래서 몬스터의 길찾기·AI·데미지는 전부 서버가 갖는다.

**Q13. 몬스터 공격을 왜 두 단계로 나눴나?**
`Broadcast_MonsterState`(모션 시작)와 `AddTimer(MonsterAttackHit, 350ms)`(실제 타격 판정)로 나눴다.
클라 애니메이션의 **타격 프레임 시각과 서버의 데미지 적용 시각을 맞추기 위해서**다.
안 그러면 "안 맞았는데 피가 깎이는" 느낌이 난다. 그리고 타격 시점에 **다시 검증**하므로
그 사이 도망가면 빗나가고, 무적 버프를 켜면 0 데미지가 된다. **회피가 가능한 전투**가 된다.

**Q14. 시야(AOI)는 어떻게 구현했나?**
`VIEW_RANGE=5` 타일의 체비쇼프 거리. **타일이 바뀔 때만** 재계산하고 old/new를 diff 해서
새로 보이면 `ADD`, 벗어나면 `REMOVE`를 **양방향**으로 보낸다.
양방향이 핵심이다 — 내가 움직여 상대가 보이기 시작했으면, 상대 입장에서도 내가 보이기 시작한 것이다.
몬스터 시야는 **별도 리스트**이고, **몬스터가 움직일 때도 대칭으로 갱신**해야 한다(안 하면 25장 사례 3).

**Q15. 존 전환 시 패킷 순서가 왜 중요한가?**
`SC_CHANGE_ZONE`(맵 로드 지시)을 **add 패킷들보다 먼저** 보내야 한다.
add가 먼저 도착하면 클라가 아직 옛 존이라고 믿는 상태에서 새 객체를 만들고, 이어서 존 전환 처리로
그것들을 지워버린다. "새 존에 갔는데 아무도 안 보이는" 버그가 순서 하나로 생긴다.

### DB

**Q16. 왜 저장 프로시저를 로그인에만 썼나?**
**도구를 작업 성격에 맞췄다.**
- 로그인은 "고정된 모양의 조회"다(인증 1회 + 고정 SELECT 4회). 프로시저에 딱 맞고, 인증 로직을 DB에 캡슐화할 수 있다.
- 저장은 **가변 길이 인벤토리**를 여러 테이블에 쓰는 작업이다. MySQL 프로시저는 **배열 인자를 못 받아서**
  40칸 인벤을 넘기기가 부자연스럽다. C++에서 루프 INSERT + **트랜잭션**이 자연스럽고 원자성도 얻는다.
- 경매 구매는 **동시성 제어**가 핵심이라 원자적 조건부 UPDATE가 필요하다.

보안 관점에서도 문제없다. SQL 인젝션의 실제 방어선은 프로시저가 아니라 **파라미터 바인딩(`?`)** 이고,
저장/경매 쿼리도 전부 바인딩을 쓴다.

**Q17. `sp_login`에서 `p_id = NULL` 트릭은 뭔가?**
인증 실패 시 조회 키를 NULL로 만들어 **뒤따르는 4개 SELECT가 전부 0행**이 되게 한다.
그러면 C++ 코드는 성공/실패 분기 없이 **"항상 결과셋 4개"** 라는 단일 구조로 처리하고,
"첫 결과셋에 행이 있나?"만 확인하면 된다. 예외 경로가 사라진다.

**Q18. 동시에 두 명이 같은 매물을 사면?**
순진하게 "재고 읽고 → 확인 → 감소"하면 둘 다 통과해 **오버셀/아이템 복사**가 난다.
**원자적 조건부 UPDATE**로 막는다.
```sql
UPDATE auction SET count = count - :qty, pending_gold = pending_gold + :total
WHERE listing_id = :id AND count >= :qty AND seller_name <> :buyer
```
InnoDB가 이 행을 잠가 두 UPDATE를 **직렬화**하므로 첫 구매만 1행 반영, 두 번째는 조건이 거짓이라 0행 → 거부.
**그리고 지급은 이 UPDATE가 성공한 뒤에만** 한다. 지급이 실패하지 않도록 골드·인벤 여유는 **UPDATE 전에** 확인한다.
이 **순서**가 아이템 증발과 복사를 동시에 막는 유일한 배치다.

**Q19. 주기 저장 시 살아있는 플레이어 데이터를 어떻게 안전하게 읽나?**
**스냅샷-언더-락.** 인벤/골드/장비/퀵슬롯/레벨을 바꾸는 모든 메서드와 `TakeSnapshot`이 같은 락을 공유하되,
**락 안에서는 값 복사만** 하고(수 µs) **느린 DB I/O는 락을 푼 뒤** 복사본으로 수행한다.
락 보유 시간을 최소화해 게임 성능 영향이 없다.
`Equip()`이 내부에서 `AddItem()`을 부르므로 **`recursive_mutex`** 가 필요했다.

**Q20. 왜 인벤을 델타가 아니라 DELETE→INSERT 스냅샷으로 저장하나?**
40칸 중 무엇이 바뀌었는지 추적하는 것보다 **통째로 지우고 다시 쓰는 게 단순하고 틀릴 여지가 없다.**
트랜잭션으로 묶여 있어 중간에 실패해도 "반쯤 지워진 인벤"이 남지 않는다.
저장 빈도가 초당 수천 건이 아니라 5초에 1명이라 비용도 문제가 안 된다.

**Q21. ODBC+nanodbc를 쓴 이유는? 네이티브 커넥터가 더 빠르지 않나?**
성능 극한이 목표가 아니라 **이식성과 명확성**을 택했다. ODBC는 표준 계층이라 엔진이 바뀌어도
애플리케이션 코드가 거의 그대로다. nanodbc는 **소스 2파일**짜리 얇은 래퍼라 의존성이 가볍다.
로그인/저장/경매는 초당 수천 건이 아니라 **이벤트성**이라 ODBC 오버헤드가 병목이 아니다.

**Q22. connection-per-call이면 매번 연결 비용이 크지 않나?**
맞다. 그래서 **의도적으로 이벤트성 작업에만** 적용했다. 로그인/로그아웃/경매는 사용자 행동 단위라
커넥션 오픈 비용이 지배적이지 않다. **주기 저장이 잦아지면 이 모델의 한계가 드러나므로**,
그때 전용 DB 스레드 + 커넥션 풀로 승격하도록 `CDB_Manager` 인터페이스를 단순하게 유지했다.
"지금 필요한 만큼만, 확장 지점은 열어둔다"는 결정이다.

### 클라이언트 / 설계

**Q23. 클라에서 네트워크 스레드와 렌더 스레드를 어떻게 분리했나?**
**D2D는 싱글스레드 렌더 모델**이라 recv 스레드에서 오브젝트/UI를 직접 만지면 크래시한다.
recv 스레드는 패킷을 파싱해 **`std::function` 태스크로 큐에 push만** 하고,
메인 스레드가 매 프레임 `Dispatch()`로 꺼내 실행한다. **모든 핸들러가 메인 스레드에서만** 돈다.
패킷 데이터는 `vector`로 **복사해서** 람다에 캡처한다 — 수신 버퍼는 곧 덮어써지므로.

**Q24. 왜 인벤/경매를 델타가 아니라 전체 스냅샷으로 보내나?**
**클라와 서버 상태가 어긋날 수가 없기 때문**이다. 패킷 하나를 놓쳐도 다음 스냅샷에서 복구된다.
"아이템 1개 추가" 같은 델타는 순서가 꼬이거나 유실되면 영구히 어긋난다.
인벤 40칸 = 약 320바이트인데, 인벤이 바뀌는 사건은 **사용자 행동 단위**라 대역폭이 병목이 아니다.
**정확성을 택하고 대역폭을 지불했다.**

**Q25. `Protocol.h`를 복제하는데 불일치 위험은 없나?**
직렬화 계약을 **바이트 단위로 공유**해야 해서 동일 복제본을 둔다. `#pragma pack(push,1)`로 패딩을 없애고,
enum도 숫자까지 일치시킨다. 한쪽만 바꾸면 즉시 깨지므로 "양쪽 동시 수정"을 **문서(CLAUDE.md)에 규칙으로 명시**했다.
규모가 커지면 **IDL(Protobuf/FlatBuffers) 기반 코드 생성**으로 옮기는 게 정석이지만,
현재 규모에선 단순 복제가 오버헤드 없이 명확하다. 다만 **복제해야 하는 테이블이 늘어난 것**(장비/포션/가격/블록맵)은
실제로 관리 부담이 됐고, 이게 스키마 기반으로 옮겨야 할 신호라고 본다.

---

## 25. 트러블슈팅 사례 (실제 겪은 것)

### 사례 1 — 위치는 저장되는데 로그인하면 항상 같은 자리

**증상**: 이동 후 로그아웃 → 재접속하면 마을 중앙. DB엔 이동한 좌표가 **정상 저장**돼 있었다.

**진단**: DB에 **눈에 띄는 값**(gold=7777, pos=25,25)을 직접 넣고 로그인했다.
→ **골드는 반영되는데 위치만 안 먹었다.** 로드 쿼리는 정상이고 "위치 적용" 단계의 문제로 좁혀졌다.

**원인**: 클라 초기화가 좌표를 하드코딩 값으로 **덮어쓰는 순서 버그**.

**배운 점**: **위치 영속화를 붙이자 비로소 드러난 잠복 버그**였다.
"골드는 되고 위치만 안 된다"는 **한 신호로 로드/적용을 분리 진단**한 게 핵심.

### 사례 2 — 동시 구매 시 아이템 복사 가능성

**증상(잠재)**: 초기 인메모리 경매는 "확인 → 지급 → 확정"이 별개 락이었다.
두 명이 동시에 마지막 재고를 사면 둘 다 확인을 통과하고, 한 명은 **지급받고도 확정에 실패**하는 창이 있었다.
(당시 코드 주석이 이 위험을 인지하고 있었다.)

**해결**: DB 이전 시 **원자적 조건부 UPDATE + 지급 순서 재배치**로 근본 해결.
`AuctionManager.h`(인메모리)는 통째로 폐기했다.

### 사례 3 — 멀티플레이 동기화 버그 5건 (2인 실기동으로 발견)

**공통 원인**: 전부 **"관찰자(다른 플레이어의 클라)에게 정보가 전달되지 않는"** 문제였다.
내 클라에서만 정상이라 혼자 테스트할 땐 절대 안 보인다.

| # | 증상 | 원인 | 해결 |
|---|---|---|---|
| 1 | 남이 공격할 때 방향이 엉뚱함 | `SC_PLAYER_STATE`에 **방향이 없음** → 관찰자가 마지막 이동 방향으로 재생 | 패킷에 `dir` + 확정 위치 추가. 서버가 몬스터 향한 방향을 계산해 브로드캐스트 |
| 2 | 남이 맞았는데 목적지까지 계속 걸어감 | `SC_PLAYER_HIT`에 **정지 위치가 없고** 관찰자가 `m_bMoving`을 안 끔 | 패킷에 정지 위치 추가 + 관찰자가 스냅 후 정지 |
| 3 | 남을 쫓는 몬스터가 **내 시야에 들어와도 안 보임** | `Broadcast_MoveMonster`가 **이미 시야에 있는** 사람에게만 전송 (플레이어가 움직여야만 시야 갱신) | **몬스터 기준으로도 시야 진입/이탈 대칭 처리** 추가 |
| 4 | 리스폰 후 남에게 안 보임 | 클라 remove는 **지연 삭제**(`Set_Dead`)인데 add는 "기존 객체 있으면 무시" → remove→add 연속에서 **add가 버려짐** | `Handle_SC_ADD_PLAYER`가 **기존 객체를 재사용(부활)**, `Initialize`에서 `m_bDead=false` |
| 5 | 시야에 들어온 몬스터 방향이 엉뚱 | `SC_ADD_MONSTER`에 `dir`이 없어 기본값 `DIR_B` | 패킷에 `dir` 추가 |

**배운 점**: **"1인 테스트로는 절대 못 잡는 버그 부류"** 가 존재한다.
클라이언트가 자기 자신에 대해 갖는 정보와 **남에 대해 갖는 정보가 다르다**는 걸
설계 단계에서 의식해야 한다. 이후로는 상태 변화 패킷에 **"관찰자가 재현하는 데 필요한 모든 정보"**
(방향, 확정 위치)를 반드시 싣는 것을 규칙으로 삼았다.

### 사례 4 — 경험치 바가 항상 꽉 차 있던 이유

경험치 시스템을 붙이려고 보니 **클라 `CPlayer` 생성자가 `m_iCurExp = m_iMaxExp`** 로 초기화하고 있었다.
UI만 있고 로직이 없던 시절의 흔적. 그래서 바가 늘 100%였다.

같이 발견한 것: `Handle_SC_PLAYER_HP`가 **HP/MP만 반영하고 MaxHP/MaxMP를 무시**하고 있었다.
그대로 뒀으면 **레벨업으로 MaxHP가 100→120이 돼도 클라는 100으로 나눠** HP바가 꽉 찬 채 멈춰 보였을 것이다.
**새 기능을 붙이면서 기존 코드의 잠복 버그가 드러난** 두 번째 사례.

### 사례 5 — CP949 인코딩 함정

대부분의 소스가 **CP949(BOM 없음)** 로 저장돼 있었다. 이런 파일을 UTF-8로 저장하는 에디터/도구로 수정하면
**기존 한글 주석과 `L"..."` 문자열이 전부 깨진다.**

→ 편집 전 **UTF-8 + BOM으로 변환**하는 절차를 규칙화했다(BOM이 있으면 MSVC가 로케일과 무관하게 UTF-8로 읽는다).
편집 후 **BOM이 유지됐는지 확인**하는 것까지 포함해서. CLAUDE.md에 "1번 규칙"으로 못 박아뒀다.

### 사례 6 — `NO_SERVER` 매크로가 이름과 정반대였던 것

`define.h`의 `#define NO_SERVER`는 이름만 보면 "오프라인 모드"다. 그런데 실제로는

- **정의돼 있어도 서버에 접속한다** (`Connect`는 이 매크로와 무관)
- 실제 효과는 `Monster::Set_ServerPos`에서 **몬스터 위치를 보간 없이 스냅**하는 것뿐
- **끄면 `#else` 분기가 참조하는 멤버(`m_fServerX/Z`)가 아예 선언돼 있지 않아 컴파일이 깨진다**
  → 즉 **한 번도 빌드된 적 없는 죽은 코드**

"온라인 테스트하려면 NO_SERVER를 꺼야지"라고 생각하면 빌드부터 실패한다.
**이름이 거짓말을 하는 매크로**의 전형. 문서에 경고로 남겨뒀다.

---

## 26. 현재 한계와 개선 방향

| 한계 | 개선 방향 |
|---|---|
| **비밀번호 평문 저장** | 해시(bcrypt/argon2) + 솔트. 컬럼은 이미 64자로 대비해 둠 |
| **connection-per-call** | 저장이 잦아지면 **전용 DB 스레드 + 커넥션 풀**로 승격 |
| **단일 프로세스** | 존 단위 샤딩 / 채널 분리 |
| **`Protocol.h` + 각종 테이블 수동 복제** | IDL(Protobuf/FlatBuffers) 기반 **스키마 코드 생성** |
| **경매 검색이 클라의 이름→코드 변환에 의존** | 아이템 메타(이름)를 서버/DB로 옮기면 **서버 단독 검색** 가능 |
| **주기 저장이 5초에 1명** | 접속자가 많아지면 저장 주기가 늘어짐 → 배치 저장 or 더티 플래그 |
| **`AuctionManager.h` 죽은 코드** | 삭제 필요 |
| **`NO_SERVER` `#else` 분기가 컴파일 불가** | 죽은 분기 제거 |
| **스크롤 아이템이 소비만 되고 효과 없음** | 마을 귀환 / 감정 효과 구현 예정 |
| **퀵슬롯 4~7번(스킬)이 비어 있음** | 스킬 시스템 |

---

## 27. 부록 — 파일별 역할 인덱스

### 서버 (`MMO_GameServer/MMO_GameServer/`)

| 파일 | 역할 |
|---|---|
| `MainServer.cpp` | `main()`. DB Init → Zone 생성 → IOCP 시작(7777) |
| `IOCP_Server.{h,cpp}` | 리슨/AcceptEx 풀, **워커 스레드 루프**, 타이머 스레드, 디버그 콘솔 스레드 |
| `Session.{h,cpp}` | `CIOEvent`, `CSession`(recv 링버퍼 / send 큐 / Disconnect 시 저장) |
| `Session_Manager.{h,cpp}` | 세션 슬롯 발급·반납 |
| `Packet_Handler.{h,cpp}` | **패킷 ID 디스패치**(static). 로그인/상점/경매/퀵슬롯 핸들러 |
| `Protocol.h` | **패킷 정의(클라와 동일 복제본)** |
| `Zone.{h,cpp}` | **가장 큰 파일**. 타일맵, 시야(AOI), 이동, 몬스터 AI, 전투, 드롭 풀, 브로드캐스트 |
| `Zone_Manager.{h,cpp}` | 5개 존 생성 + **블록맵 정의** + 몬스터 랜덤 배치 |
| `Player.{h,cpp}` | `CPlayer` — 위치/HP/MP/인벤40/장비6/퀵슬롯8/레벨·경험치/버프/스냅샷 |
| `Player_Manager.{h,cpp}` | 플레이어 슬롯 + **주기 저장 라운드로빈** |
| `Monster.{h,cpp}` | `CMonster` — 상태/어그로/공격쿨/경험치 보상/방향 계산 |
| `Monster_Manager.{h,cpp}` | 몬스터 생성 + **타입별 스탯**(HP/경험치) |
| `PathFinder.{h,cpp}` | A* / Theta* / Corner-based |
| `Timer.{h,cpp}` | 우선순위 큐 타이머 (`AddTimer`) |
| `DB_Manager.{h,cpp}` | nanodbc. `Login`(sp_login) / `Save`(트랜잭션) / **경매 6종** |
| `SaveData.h` | `FSaveSnapshot` — 저장용 스냅샷 구조체 |
| `AccountDB.h` | `FAccountData` + 하드코딩 계정(DB 이전의 폴백) |
| `ServerItem.h` | 아이템 코드 규칙, **장비/포션/가격 테이블**, 드롭 추첨 |
| `AuctionManager.h` | ⚠️ **죽은 코드** (인메모리 경매 → DB로 대체됨) |
| `nanodbc.{h,cpp}` | 벤더 라이브러리 (직접 작성 아님) |

### 클라이언트 (`MMO_Client/MMO_Client/`)

| 파일 | 역할 |
|---|---|
| `Game.cpp` | `WinMain`, **144FPS 루프**, `WndProc` |
| `MainApp.cpp` | D2D/DWrite 초기화, `Update`(Dispatch 먼저)/`Render` |
| `Network_Manager.{h,cpp}` | 소켓, **recv 스레드 + 태스크 큐**, 모든 `SC_*` 핸들러, 경매/퀵슬롯 캐시 |
| `Protocol.h` | 서버와 동일 복제본 |
| `Level_*.{h,cpp}` | MENU / LOGIN / CHOICE / TEST(인게임) |
| `GameObject.h` | 추상 기반 — ISO_INFO/COLLIDER/FRAME/HP/MP/**레벨·경험치** |
| `Player.{h,cpp}` | 내 캐릭터 — 입력, 길찾기(Corner), 공격, 인벤/장비, 버프 |
| `Other_Player.{h,cpp}` | 다른 플레이어 — **보간/스냅**, 이름표 |
| `Monster*.{h,cpp}` | Orc / Wing — 서버 패킷으로만 움직임 |
| `Object_Manager.{h,cpp}` | 오브젝트 리스트 + **Y소팅** + 몬스터/드롭 피킹 |
| `Map_Manager.{h,cpp}` | 타일맵, `Is_Movable`, **비동기 존 전환** |
| `Zone_*.{h,cpp}` | 존별 블록맵/오브젝트 배치 (Town / Test / Field_E/S/W) |
| `Camera.{h,cpp}` | **아이소 좌표 변환**(Screen↔IsoWorld), 타겟 추적 |
| `Input_Manager.{h,cpp}` | 키/마우스 엣지 검출, 드래그, 커서 모드 |
| `Img_Manager.{h,cpp}` | PNG/BMP 캐시, DirectWrite 폰트 |
| `Inventory / Equipment / ItemData_*` | 아이템 데이터 클래스(포션/장비/스크롤/기타), 코드↔객체 팩토리 |
| `UI_*.{h,cpp}` | HUD / Inventory / QuickSlot / Shop / Auction / LoginBox / 다이얼로그 |
| `PathFinder.{h,cpp}` | 서버와 같은 알고리즘 (클라는 CornerBased 사용) |
| `define.h` | 화면/타일 상수, `DIRECTION`, ⚠️ `NO_SERVER` 매크로 |
| `Item_define.h` | 아이템 enum (`POTION_TYPE`, `EQUIPMENT_TYPE` …) — **서버 테이블과 값 일치 필수** |

### DB (`db/`)

| 파일 | 역할 |
|---|---|
| `schema.sql` | 6개 테이블 정의 |
| `procedures.sql` | `sp_login` (결과셋 4개) |
| `seed.sql` / `auction_seed.sql` | 시드 계정(test1/test2, 비번 1234) + 기본 경매 매물 |
| `migration_2026-07-14_exp.sql` | `character`에 `level`/`exp` 추가 (재실행 안전) |
| `migration_2026-07-14_quickslot.sql` | `quickslot` 테이블 추가 |

---

## 마무리 — 이 프로젝트에서 말할 수 있는 것

1. **IOCP를 라이브러리 없이 직접 다뤘다.** AcceptEx 풀, `WSAOVERLAPPED` 확장, 완료 통지 분기,
   세션 수명(`shared_ptr`), TCP 패킷 경계 복원, send 직렬화까지 전부.
2. **게임 이벤트를 IOCP 워커에 태웠다.** 타이머 스레드 하나로 AI/리스폰/타격/저장을 처리해
   스레드 폭발을 피했다.
3. **서버 권위를 원칙으로 삼되, 무엇을 지키려는지 먼저 물었다.**
   인벤·전투·경매는 서버 권위, 퀵슬롯은 클라 권위 — 그 판단 근거를 설명할 수 있다.
4. **동시성 문제를 두 층위에서 풀었다.**
   애플리케이션은 스냅샷-언더-락과 CAS로, DB는 원자적 조건부 UPDATE와 트랜잭션으로.
5. **혼자서는 못 잡는 버그를 2인 실기동으로 잡아봤다.** 관찰자 관점의 정보 누락이라는
   분산 시스템 특유의 버그 부류를 경험했다.
6. **잠복 버그가 새 기능을 붙일 때 드러난다는 걸 두 번 겪었다.** (위치 영속화, 경험치 시스템)
