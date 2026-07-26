# 2D MMORPG 게임 서버 — 서버 개발 포트폴리오

> Windows IOCP 기반 C++ 게임 서버. 서버 권위(server-authoritative) 구조로 이동/전투/인벤토리/경매를 처리하고, MySQL(ODBC/nanodbc)로 계정·캐릭터·경매를 영속화한다. 본 문서는 **서버 구현**에 집중한다. 클라이언트는 서버-클라 계약(프로토콜, 왜 서버가 권위를 갖는가)을 설명하는 데 필요한 만큼만 언급한다.

---

## 목차
1. 프로젝트 개요
2. 기술 스택
3. 서버 전체 아키텍처
4. 네트워크 계층 (IOCP)
5. 동시성 모델
6. 게임 로직 (서버 권위)
7. DB 연동
8. 예상 면접 질문 & 답변
9. 트러블슈팅 사례

---

## 1. 프로젝트 개요

| 항목 | 내용 |
|---|---|
| 장르 | 2D 아이소메트릭 MMORPG |
| 구성 | 게임 서버(IOCP) + 클라이언트(Direct2D) 별도 솔루션 |
| 언어/환경 | C++17, MSVC v143, x64, Windows |
| 서버 포트 | 7777 (TCP) |
| DB | MySQL 8.4 LTS (ODBC + nanodbc + 저장 프로시저) |

**서버가 담당하는 것**
- 세션/접속 관리, 패킷 라우팅
- 이동 검증 및 브로드캐스트(서버 권위)
- 시야(AOI) 처리 — 관심 영역 내 플레이어에게만 이벤트 전송
- 몬스터 AI (길찾기 + 상태머신 + 타이머 기반 공격 판정)
- 전투/사망/리스폰
- 다중 존과 포탈 전환
- 아이템 드롭/획득/인벤토리/장비/버프 (전부 서버 권위)
- 계정 로그인, 캐릭터 저장(로그아웃 + 주기 자동저장), 경매장 영속화

---

## 2. 기술 스택

| 분류 | 기술 | 선택 이유 |
|---|---|---|
| I/O 모델 | **Windows IOCP** | 수천 커넥션을 소수 스레드로 처리하는 확장형 비동기 I/O |
| 동기화 | `std::mutex`, `std::recursive_mutex`, `std::atomic` | 존/세션/플레이어 단위의 세분화된 락 |
| 타이머 | 우선순위 큐 + 타이머 스레드 → IOCP 이벤트 | AI 틱/리스폰/공격 판정/자동저장을 워커에서 일괄 처리 |
| DB 접근 | **MySQL + ODBC + nanodbc** | ODBC 표준으로 엔진 독립, nanodbc는 얇은 C++ 래퍼(2파일) |
| DB 로직 | **저장 프로시저(로그인) + 파라미터 바인딩 SQL(저장/경매)** | 상황별로 도구를 나눠 사용(아래 Q&A 참조) |
| 길찾기 | **A\*, Theta\* (Any-Angle)** | 격자 최단경로 + 대각 자연스러움 |

---

## 3. 서버 전체 아키텍처

```
                        [ 클라이언트 N개 ]
                              | TCP
                    ┌─────────▼──────────┐
                    │   CIOCP_Server      │
                    │  - Listen + AcceptEx 풀
                    │  - Worker 스레드 풀 (하드웨어 코어 수)
                    │  - Timer 스레드 1
                    │  - Debug 콘솔 스레드 1
                    └─────────┬──────────┘
                              │ GetQueuedCompletionStatus
              ┌───────────────┼────────────────┐
              ▼               ▼                ▼
        CPacket_Handler   Timer 이벤트     Accept/Recv/Send
        (패킷 ID 분기)   (AI/리스폰/저장)   (세션 I/O)
              │
      ┌───────┼────────┬─────────┬──────────┐
      ▼       ▼        ▼         ▼          ▼
  Session  Zone    Player    Monster    DB(nanodbc)
  Manager  Manager  Manager   Manager
```

**매니저 싱글턴**
- `CSession_Manager` : 세션 ID 발급/조회/해제
- `CZone_Manager` : zoneID → `CZone*`
- `CPlayer_Manager` : 세션 슬롯과 1:1로 플레이어 관리 + 주기 저장 라운드로빈
- `CMonster_Manager` : 몬스터 ID 발급/조회
- `CDB_Manager` : 로그인/저장/경매 DB 접근

**핵심 클래스**
- `CIOCP_Server` — 리슨 소켓, AcceptEx 풀, 워커/타이머 스레드
- `CSession`(`shared_ptr`) — 소켓당 세션, recv 링버퍼 + 순차 send 큐
- `CIOEvent` — `WSAOVERLAPPED` 확장체, `IOType`으로 완료 종류 구분
- `CZone` — 맵 한 칸, 타일맵/플레이어·몬스터 집합/시야/AI/전투 담당
- `CPlayer`, `CMonster` — 서버 측 엔티티 상태
- `CPacket_Handler` — 패킷 ID 디스패처(static)

---

## 4. 네트워크 계층 (IOCP)

### 4.1 프로토콜 설계

모든 패킷은 고정 헤더 뒤에 페이로드가 붙는다. 구조체를 그대로 바이트 캐스팅하며, 패딩을 없애기 위해 `#pragma pack(push,1)`을 사용한다.

```cpp
#pragma pack(push, 1)
struct PacketHeader { uint16_t size; uint16_t id; };

struct CS_MOVE_DEST_PACKET { PacketHeader header; float fDestX, fDestZ; uint32_t moveTime; };
struct SC_MOVE_PLAYER_PACKET { /* 목적지 + 현재위치 + 방향 (보간용) */ };
#pragma pack(pop)
```

- ID 규칙: `CS_*` = 1000번대(Client→Server), `SC_*` = 2000번대(플레이어)/2100번대(몬스터)
- `Protocol.h`는 클라/서버 양쪽에 **동일 복제본**을 두어 직렬화 계약을 강제한다. 한쪽만 바꾸면 즉시 깨지므로 반드시 동시 수정한다.

### 4.2 세션과 링버퍼 기반 패킷 경계 처리

TCP는 스트림이라 한 번의 `recv`에 여러 패킷이 붙거나 하나가 잘려 올 수 있다. 세션은 남은 조각(`m_prevRemain`)을 버퍼 앞에 이어 붙여 **완전한 패킷 단위로만** 처리한다.

```cpp
class CSession : public std::enable_shared_from_this<CSession> {
    static constexpr int32_t RECV_BUF_SIZE = 4096;
    uint8_t m_recvBuf[RECV_BUF_SIZE];
    int32_t m_prevRemain = 0;   // 지난 recv에서 처리 못한 잔여 바이트

    // send: 세션당 순차 전송(앞 패킷 완료 후 다음) → 버퍼/오버랩 재사용 안전
    std::mutex m_sendLock;
    std::queue<std::vector<uint8_t>> m_sendQueue;
    bool m_sending = false;
};
```

`ProcessRecvData`는 `[헤더 size 확인 → 완전한 패킷이면 Handle → 커서 전진]`을 반복하고, 남은 바이트를 버퍼 앞으로 `memmove`해 다음 recv를 이어받는다.

### 4.3 완료 통지 분기

`CIOEvent`가 `WSAOVERLAPPED`를 상속(첫 멤버로 두어 캐스팅 가능)하고 `IOType`으로 어떤 작업의 완료인지 구분한다. 워커 스레드는 `GetQueuedCompletionStatus`로 완료를 꺼내 종류별로 분기한다.

```cpp
void CIOCP_Server::WorkerThread() {
    while (true) {
        DWORD bytes; ULONG_PTR key; OVERLAPPED* pOver;
        GetQueuedCompletionStatus(m_hIOCP, &bytes, &key, &pOver, INFINITE);
        CIOEvent* ev = reinterpret_cast<CIOEvent*>(pOver);

        if (ev->m_type == IOType::MonsterAI) { /* key=몬스터ID → 해당 존 AI 틱 */ }
        else if (ev->m_type == IOType::MonsterRespawn) { ... }
        else if (ev->m_type == IOType::MonsterAttackHit) { ... }
        else if (ev->m_type == IOType::PlayerAutoSave) { /* 주기 저장 */ }
        else { /* Accept / Recv / Send : 세션 I/O */ }
    }
}
```

세션 수명은 `shared_ptr`로 관리해, 진행 중인 I/O가 남아 있는 동안 세션이 파괴되지 않도록 한다.

---

## 5. 동시성 모델

여러 워커 스레드가 동시에 돌기 때문에 공유 자원마다 락 전략을 나눴다. **핵심은 "락은 짧게, 느린 작업(I/O)은 락 밖에서"** 이다.

| 자원 | 락 | 범위 |
|---|---|---|
| 존의 플레이어/몬스터 집합 | `CZone::m_zoneLock` | 집합 추가/삭제 |
| 플레이어 시야 리스트 | `CPlayer::m_viewLock`, `m_monsterViewLock` | 시야 목록 갱신 |
| 세션 송신 큐 | `CSession::m_sendLock` | 큐 조작 + 전송 시작 |
| 타이머 큐 | `g_timerLock` | 이벤트 push/pop |
| 플레이어 저장 대상 필드(인벤/골드/장비) | `CPlayer::m_saveLock` (recursive) | 스냅샷 복사 및 변경 |

### 타이머 스레드 → IOCP

객체마다 스레드를 두는 대신, **단일 타이머 스레드**가 우선순위 큐(만료 시각 오름차순)에서 만료된 이벤트를 꺼내 `PostQueuedCompletionStatus`로 IOCP에 넣는다. 그러면 남는 워커가 그 이벤트를 처리한다. 몬스터 AI 틱, 리스폰, 공격 모션 후 실제 Hit, 주기 저장이 모두 이 경로를 탄다.

```cpp
void AddTimer(int32_t nID, EEventType eType, uint32_t nDelayMs); // 큐에 (nID, 종류, 만료시각) push
// 처리 후 필요하면 스스로 다시 AddTimer 하여 주기적 반복(AI 0.5s, 자동저장 5s 등)
```

이 구조 덕분에 "몬스터 1000마리 = 스레드 1000개" 같은 폭발이 없다. 이벤트만 늘고 워커 수는 고정이다.

---

## 6. 게임 로직 (서버 권위)

### 6.1 이동 동기화 — 서버 권위 + 위치 역산 + 해킹 방지

**계약**: 클라가 목적지 클릭 시 `CS_MOVE_DEST`(목적지, 이동시작 시각)를, 타일이 바뀔 때 `CS_MOVE_POS`(현재 위치)를 보낸다. 서버가 검증하고 시야 내 다른 플레이어에게 브로드캐스트한다.

서버는 위치를 매 프레임 저장하지 않고, **"이동 시작 위치 + 방향 × 속도 × 경과시간"** 으로 언제든 정확히 역산한다.

```cpp
void CPlayer::GetCurrentPos(uint32_t nCurrentTime, float& outX, float& outZ) const {
    if (!m_bMoving) { outX = m_fCurX; outZ = m_fCurZ; return; }
    float dx = m_fDestX - m_fMoveStartX, dz = m_fDestZ - m_fMoveStartZ;
    float dist = sqrtf(dx*dx + dz*dz);
    float moved = m_fSpeed * (nCurrentTime - m_nMoveStartTime) / 1000.f;
    if (moved >= dist) { outX = m_fDestX; outZ = m_fDestZ; return; }
    outX = m_fMoveStartX + (dx/dist)*moved;
    outZ = m_fMoveStartZ + (dz/dist)*moved;
}
```

**해킹 방지**: 클라가 보낸 현재 위치를 서버가 역산한 위치와 비교해, 허용 오차를 넘으면 거부하고 정상 위치로 되돌린다.

```cpp
void CZone::OnMovePos(PlayerRef p, float fCurX, float fCurZ, uint32_t moveTime) {
    if (p->m_bMoving) {
        float sx, sz; p->GetCurrentPos(moveTime, sx, sz);
        float diff = sqrtf((fCurX-sx)*(fCurX-sx) + (fCurZ-sz)*(fCurZ-sz));
        constexpr float MAX_TOLERANCE = 2.f;
        if (diff > MAX_TOLERANCE) { /* 텔레포트/속도핵 → 서버 위치로 복귀 브로드캐스트 */ }
    }
    p->m_fCurX = fCurX; p->m_fCurZ = fCurZ;
    // 목적지 블록 검증(IsMovable)도 수행: 벽으로 이동 요청 시 되돌림
}
```

### 6.2 시야 처리 (AOI, Area Of Interest)

모든 이벤트를 전체 접속자에게 뿌리면 O(N²)로 폭발한다. 각 플레이어는 `VIEW_RANGE`(=5) 내 대상만 시야 리스트로 유지하고, 이동/상태 변화는 **시야에 든 대상에게만** 보낸다. 타일이 바뀔 때마다 시야를 재계산해 새로 보이는 대상은 add, 벗어난 대상은 remove를 양방향으로 전송한다.

```cpp
// 타일 변경 시: 이전 시야 vs 새 시야 diff
//  - 새로 보임 → SC_ADD_PLAYER 양방향
//  - 시야 이탈 → SC_REMOVE_PLAYER 양방향
//  - 계속 보임 → SC_MOVE_PLAYER
```

### 6.3 길찾기 — 서버에서 몬스터 경로 계산 (A\* / Theta\*)

몬스터의 추격 경로는 **서버**가 계산한다(플레이어 이동은 클라 입력 기반 + 서버 검증). `CPathFinder`는 격자 A\*와 Any-Angle Theta\*를 제공한다. Theta\*는 부모 노드로 직접 시야가 통하면 중간 노드를 건너뛰어(line-of-sight 검사) 격자 계단 현상 없는 자연스러운 대각 경로를 만든다.

```cpp
enum class EPathMode { AStar, ThetaStar, CornerBased };

static std::vector<std::pair<float,float>> FindPath(
    int startX, int startZ, int endX, int endZ,
    float realStartX, float realStartZ,
    IsMovableFunc fnIsMovable, EPathMode mode = EPathMode::AStar);

// Theta*: 확장 시 "부모에서 현재 이웃까지 직선 시야가 통하면 부모를 그대로 계승"
static bool HasLineOfSight(float ax, float az, float bx, float bz, IsMovableFunc);
```

맵의 통과 가능 여부(`IsMovable`)를 `std::function`으로 주입받아, 길찾기 알고리즘을 존/맵 구현과 분리했다.

### 6.4 몬스터 AI — 상태머신 + 타이머 큐

몬스터는 `IDLE / WALK / ATTACK / HIT / DEAD` 상태머신으로 동작한다. AI 틱은 타이머로 0.5초마다 예약되고, 추격 사거리·공격 사거리·공격 쿨다운·경직(hit delay)을 상태로 관리한다.

```
플레이어 시야 진입 → 활성화(CAS로 중복 방지)
IDLE → 어그로 → WALK(경로 추종) → 사거리 진입 → ATTACK
ATTACK: 공격 모션 시작 즉시 상태 전환, 실제 타격은
        AddTimer(MonsterAttackHit, hitDelay) 로 모션 타이밍에 맞춰 판정
HIT: 피격 경직 동안 AI 스킵, 경직 종료 후 IDLE 복귀
```

공격을 "모션 시작"과 "실제 데미지 판정" 두 단계로 분리해, 클라 애니메이션 타이밍과 서버 데미지 적용을 일치시킨다(맞는 순간에 데미지). 길찾기 없이 직선 추격하는 부유 몬스터(WING) 타입도 별도 분기로 지원한다.

### 6.5 전투 / 사망 / 리스폰 (서버 권위)

데미지, HP, 사망 판정 모두 서버가 계산한다. 클라는 공격 의사(`CS_ATTACK_MONSTER`)만 보내고 결과(`SC_..._HIT`, `SC_PLAYER_STATE`)를 받는다.

```cpp
int32_t CPlayer::Get_Atk() const {           // 기본 + 장비 + 버프
    int32_t atk = m_baseAtk;
    for (int i = 0; i < EQUIP_SLOTS; ++i) if (m_equipCode[i]) atk += EquipAtk(m_equipCode[i]);
    if (GetTickCount64() < m_nAtkBuffEnd) atk += m_nAtkBuffAmt;
    return atk;
}
// 몬스터 피격 데미지 = max(1, 몬스터공격력 - Get_Def()), 무적 버프 시 0
```

사망 시 사망 플래그를 세우고 리스폰을 타이머로 예약한다.

### 6.6 다중 존 + 포탈 (서버 권위 전환)

허브인 마을을 중심으로 동서남북 4개 필드가 있고, 포탈로 전환한다. 전환은 서버 권위이며 **패킷 순서가 중요**하다.

```
CS_PORTAL(목표존, 좌표) 수신
  → LeaveZone(옛 존)
  → SC_CHANGE_ZONE 먼저 전송 (클라가 맵 로드 + 위치 이동)
  → EnterZone(새 존): 새 시야의 add + 옛 존 몬스터 remove 전송
```

`SC_CHANGE_ZONE`을 add들보다 **먼저** 보내야 클라가 새 존의 객체를 실수로 지우지 않는다. 객체 정리는 클라가 임의로 하지 않고 서버 remove 패킷에 의존한다.

### 6.7 아이템 / 인벤토리 / 장비 (전부 서버 권위)

인벤토리는 서버가 유일한 진실(single source of truth)이며, 동적 할당 없이 **값 배열**로 관리한다.

```cpp
class CPlayer {
    static constexpr int32_t INVEN_SIZE = 40;
    int32_t m_invenCode[INVEN_SIZE]  = {};  // 0 = 빈 슬롯
    int32_t m_invenCount[INVEN_SIZE] = {};
    int32_t m_equipCode[EQUIP_SLOTS] = {};
    int32_t m_gold = 0;

    bool AddItem(int32_t code, int32_t amount);   // 스택 가능(포션/스크롤/기타)은 99까지 누적
    bool RemoveItemSlot(int32_t slot, int32_t cnt);
    bool Equip(int32_t invenSlot);                // 장착(기존 장비는 인벤 반환)
};
```

몬스터 사망 → 서버 `RollDrop` → 존 고정 드롭 풀(new 없음) → `SC_ADD_DROP` 브로드캐스트. 플레이어가 드롭 접촉 후 클릭 → `CS_PICKUP` → 서버 거리 검증 → 인벤 반영 → `SC_INVEN_UPDATE`(전체 스냅샷)로 클라 동기화. 장착/포션 사용도 클라는 의사만 보내고 서버가 인벤/HP/버프를 바꿔 스냅샷으로 회신한다.

---

## 7. DB 연동

### 7.1 스택 결정: MySQL + ODBC + nanodbc + 저장 프로시저

- **ODBC**: DB 엔진이 바뀌어도 애플리케이션 코드(연결/쿼리 호출)는 그대로. 드라이버와 SQL 방언만 교체.
- **nanodbc**: ODBC C API를 C++ 클래스 3개(`connection`/`statement`/`result`)로 감싼 얇은 래퍼. 소스 2파일이라 별도 빌드 설정 없이 프로젝트에 포함하고 `odbc32.lib`만 링크.
- **저장 프로시저는 로그인에만**, 저장/경매는 파라미터 바인딩 SQL + 트랜잭션(이유는 Q&A 참조).

연결은 **호출마다 연결(connection-per-call)** 모델 A로 시작했다. 로그인/저장/경매는 빈번하지 않아 충분하며, 부하가 커지면 커넥션 풀로 승격할 수 있게 인터페이스를 단순화했다.

### 7.2 로그인 로드 — 저장 프로시저 sp_login

`sp_login(id, pw)`은 인증 후 **결과셋 3개(character / inventory / equipment)** 를 반환한다. 인증 실패 시 내부에서 조회 키를 NULL로 만들어 세 결과셋 모두 0행이 되게 해, 서버 코드가 항상 "결과셋 3개"라는 단일 구조로 처리하도록 통일했다.

```sql
CREATE PROCEDURE sp_login(IN p_id VARCHAR(20), IN p_pw VARCHAR(64))
BEGIN
    DECLARE v_ok INT DEFAULT 0;
    SELECT COUNT(*) INTO v_ok FROM account WHERE account_id=p_id AND password=p_pw;
    IF v_ok = 0 THEN SET p_id = NULL; END IF;       -- 실패 시 이후 조회 0행
    SELECT zone_id, spawn_x, spawn_z, gold FROM `character` WHERE account_id=p_id;
    SELECT slot, item_code, count FROM inventory WHERE account_id=p_id ORDER BY slot;
    SELECT slot, item_code       FROM equipment WHERE account_id=p_id ORDER BY slot;
END
```

C++ 측은 nanodbc로 `{CALL sp_login(?,?)}`을 실행하고 `next()`/`next_result()`로 결과셋을 순회한다. 첫 결과셋에 행이 있으면 로그인 성공.

```cpp
nanodbc::statement stmt(conn);
nanodbc::prepare(stmt, NANODBC_TEXT("{CALL sp_login(?, ?)}"));
stmt.bind(0, id); stmt.bind(1, pw);
nanodbc::result r = nanodbc::execute(stmt);
if (!r.next()) return false;                 // 인증 실패
out.zoneID = r.get<int>(0); out.spawnX = r.get<float>(1); ...
if (r.next_result()) { while (r.next()) { /* inventory */ } }
if (r.next_result()) { while (r.next()) { /* equipment */ } }
```

로그인 지점(seam)을 인터페이스 하나로 격리해, 하드코딩 계정 → DB 조회로 데이터 출처만 갈아끼웠다.

### 7.3 세이브 — 로그아웃 저장 + 주기 자동저장

로드의 짝인 저장이 없으면 매 로그인 초기 상태로 리셋된다. 두 시점에 저장한다.

**(1) 로그아웃 저장** — 접속 종료 시점(`CSession::Disconnect`)에서 현재 상태를 기록. 여러 테이블(character UPDATE + inventory/equipment DELETE→INSERT)을 **하나의 트랜잭션**으로 묶어, 중간에 실패하면 롤백되어 "인벤이 반쯤 지워진" 파손을 막는다.

```cpp
bool CDB_Manager::Save(const FSaveSnapshot& snap) {
    nanodbc::connection conn(m_connStr);
    nanodbc::transaction tx(conn);               // 실패 시 소멸자가 롤백
    // 1) character UPDATE (존/위치/골드)
    // 2) inventory: DELETE 후 채운 칸만 INSERT (스냅샷)
    // 3) equipment: DELETE 후 착용분만 INSERT
    tx.commit();                                 // 전부 성공해야 확정
    return true;
}
```

**(2) 주기 자동저장** — 크래시 대비. 기존 타이머 큐를 재사용해 `PlayerAutoSave` 틱을 5초마다 예약하고, **한 틱에 온라인 1명씩 라운드로빈**으로 저장해 워커 블로킹을 분산한다.

여기서 동시성 문제가 생긴다: 저장 대상은 **살아서 플레이 중인** 플레이어라, 다른 스레드(타이머 워커)가 그 인벤/골드를 읽는 동안 게임 스레드가 그것을 수정하면 반쪽 상태(torn snapshot)가 저장될 수 있다. 이를 **스냅샷-언더-락**으로 해결했다. 짧은 락으로 값만 로컬 구조체에 복사하고, 느린 DB I/O는 락을 푼 뒤 그 복사본으로 수행한다.

```cpp
// CPlayer: 저장 대상 변경 메서드(AddItem/AddGold/Equip 등)와 TakeSnapshot 이 같은 락 공유
mutable std::recursive_mutex m_saveLock;  // Equip이 내부에서 AddItem 호출 → 재귀 락 필요

void CPlayer::TakeSnapshot(FSaveSnapshot& s) const {
    std::lock_guard<std::recursive_mutex> lk(m_saveLock);
    s.gold = m_gold; s.zoneID = m_nZoneID; s.x = m_fCurX; s.z = m_fCurZ;
    for (int i=0;i<INVEN_SIZE;++i){ s.invenCode[i]=m_invenCode[i]; s.invenCount[i]=m_invenCount[i]; }
    for (int i=0;i<EQUIP_SLOTS;++i) s.equipCode[i]=m_equipCode[i];
}

void CPlayer_Manager::AutoSaveNext() {          // 라운드로빈: 온라인 1명만
    PlayerRef target;
    { std::lock_guard lk(m_lock); /* 커서 전진하며 다음 온라인 1명 shared_ptr 복사 */ }
    if (!target) return;
    FSaveSnapshot snap; target->TakeSnapshot(snap);   // 짧은 락
    CDB_Manager::Get_Instance()->Save(snap);          // 느린 I/O는 락 밖
}
```

### 7.4 경매장 — DB 정본 + 페이지네이션 + 동시 구매 방지 + 검색

경매는 데이터가 계속 쌓이므로 전체를 메모리에 들고 있지 않고 **DB를 유일한 정본**으로 두고, 사건마다 즉시 DB에 반영(write-through)한다.

**페이지네이션(서버측)**: 열기/새로고침/다음 페이지 모두 서버가 해당 페이지만 쿼리한다. 다음 페이지 존재 여부(`hasNext`)는 페이지 크기보다 1개 더 읽어서 판단한다.

```cpp
// ORDER BY listing_id DESC (최신순) + LIMIT pageSize+1 OFFSET page*pageSize
// 탭별 WHERE: 구매=seller<>me AND count>0, 내판매=seller=me(완판 포함)
// 검색 시: AND item_code IN (검색어와 이름이 일치하는 코드들)
```

**동시 구매 방지(핵심)**: 재고 감소를 "읽고-확인-쓰기"로 하면 두 명이 거의 동시에 사면 오버셀/복사가 난다. 이를 **원자적 조건부 UPDATE** 하나로 해결한다. `WHERE count >= qty`가 가드이고, InnoDB 행 잠금이 두 UPDATE를 직렬화한다. 첫 구매만 1행 반영, 나머지는 0행 → 깔끔히 거부.

```cpp
bool CDB_Manager::Auction_CommitBuy(int listingID, int qty, int total, const char* buyer) {
    nanodbc::statement s(conn);
    nanodbc::prepare(s, NANODBC_TEXT(
        "UPDATE auction SET count = count - ?, pending_gold = pending_gold + ? "
        "WHERE listing_id = ? AND count >= ? AND seller_name <> ?"));
    // ... bind ...
    nanodbc::result r = nanodbc::execute(s);
    return r.affected_rows() >= 1;   // 1행=성공, 0행=경쟁 탈락/재고부족/본인
}
```

구매 전체 흐름은 **지급 실패 여지를 없애도록** 순서를 잡았다: `PeekBuy`(코드/가격 조회) → 서버에서 골드·인벤 여유 사전 확인 → **원자적 CommitBuy** → 성공했을 때만 아이템 지급/골드 차감. 취소도 "삭제 후 지급"으로 아이템이 사라지지 않도록 사전 확인 후 트랜잭션으로 삭제한다.

**인코딩**: 기본 매물 판매자명이 한글("경매장")이라 연결에 `CHARSET=utf8mb4`를 주고, DB의 UTF-8 문자열을 클라가 기대하는 코드페이지로 변환해 패킷에 담는다(플레이어명은 ASCII라 무관).

---

## 8. 예상 면접 질문 & 답변 (설계 의도)

### Q1. 왜 IOCP를 선택했나?
Windows에서 수천 커넥션을 **소수 스레드**로 처리하는 확장형 비동기 I/O 모델이기 때문이다. `select`는 fd 수에 따라 O(N) 스캔이 생기고, 커넥션당 스레드는 컨텍스트 스위칭과 메모리가 폭발한다. IOCP는 커널이 완료를 큐로 넘겨주고 워커 풀이 꺼내 쓰므로 커넥션 수와 스레드 수가 분리된다. 게임 서버는 동시 접속이 핵심 축이라 IOCP가 정석이다.

### Q2. 왜 몬스터 길찾기는 서버, 플레이어 이동은 클라 입력 기반인가?
**둘 다 최종 권위는 서버**다. 차이는 "누가 의도를 만드느냐"이다.
- 플레이어 이동은 사람의 입력(클릭)이 원천이라 클라가 목적지를 정하고 서버가 검증(블록/속도)한다. 입력 지연을 줄이려 클라는 즉시 움직이되, 서버가 위치를 역산해 오차가 크면 되돌린다(해킹 방지).
- 몬스터는 입력 주체가 없다. AI가 곧 로직이고, 그 로직은 반드시 서버에 있어야 조작 불가능하다. 만약 몬스터 경로를 클라가 계산하면 클라마다 경로가 달라지고(동기화 불가), 핵으로 몬스터를 유리하게 움직일 수 있다. 그래서 몬스터 길찾기·AI·데미지는 전부 서버가 권위를 갖는다.

### Q3. 왜 저장 프로시저를 로그인에만 쓰고, 저장/경매는 애플리케이션 SQL로 했나?
**도구를 작업 성격에 맞췄다.**
- 로그인은 "고정된 모양의 조회"다(인증 1회 + 고정 SELECT 3회). 프로시저에 딱 맞고, 인증 로직을 DB 안에 캡슐화하는 이점도 있다.
- 저장은 **가변 길이 인벤토리**를 여러 테이블에 걸쳐 쓰는 작업이다. MySQL 프로시저는 배열 인자를 못 받아, 40칸 인벤을 프로시저 하나로 넘기기가 부자연스럽다. C++에서 루프 INSERT + **트랜잭션**으로 처리하는 편이 자연스럽고, 중간 실패 롤백이라는 원자성도 얻는다.
- 경매 구매는 **동시성 제어**가 핵심이라 원자적 조건부 UPDATE가 필요하다. 이 역시 애플리케이션에서 명시적으로 다루는 게 명확하다.

보안 관점에서도 문제없다. SQL 인젝션을 막는 실제 방어선은 프로시저가 아니라 **파라미터 바인딩(`?`)** 이고, 저장/경매 쿼리도 전부 바인딩을 쓴다. 실무 게임 서버도 "고정 조회는 프로시저, 여러 테이블 걸친 가변 저장은 트랜잭션"으로 나누는 경우가 많다.

### Q4. ODBC+nanodbc를 쓴 이유는? 네이티브 커넥터가 더 빠르지 않나?
성능 극한이 목표가 아니라 **이식성과 학습/유지보수 가치**를 택했다. ODBC는 표준 계층이라 엔진이 바뀌어도 애플리케이션 코드가 거의 그대로다(드라이버·SQL 방언만 교체). nanodbc는 소스 2파일짜리 얇은 래퍼라 의존성이 가볍고 코드가 명확하다. 지금 규모에서 네이티브 커넥터 대비 성능 차이는 병목이 아니며, 저장/로그인은 초당 수천 건이 아니라 이벤트성이라 ODBC 오버헤드가 문제되지 않는다.

### Q5. "연결마다 연결(connection-per-call)"이면 매번 커넥션 비용이 크지 않나?
맞다. 그래서 **의도적으로 이벤트성 작업에만** 적용했다. 로그인/로그아웃/경매 액션은 초당 수천 건이 아니라 사용자 행동 단위라 커넥션 오픈 비용이 지배적이지 않다. 반대로 **주기 저장이 잦아지면** 이 모델의 한계가 드러나므로, 그때는 전용 DB 스레드 + 커넥션 풀로 승격하도록 인터페이스(`CDB_Manager`)를 단순하게 유지했다. "지금 필요한 만큼만, 확장 지점은 열어둔다"는 결정이다.

### Q6. 동시에 두 명이 같은 매물을 사면 어떻게 되나?
순진하게 "재고 읽고 → 확인 → 감소" 하면 둘 다 통과해 오버셀/아이템 복사가 난다. 이를 **원자적 조건부 UPDATE**로 막는다.
```sql
UPDATE auction SET count = count - :qty
WHERE listing_id = :id AND count >= :qty AND seller_name <> :buyer
```
InnoDB가 이 행을 잠가 두 UPDATE를 직렬화하므로, 첫 구매만 1행 반영되고 두 번째는 재고 조건이 거짓이라 0행이 되어 거부된다. 그리고 **아이템 지급은 이 UPDATE가 성공한 뒤에만** 하므로 복사가 원천 차단된다. 지급이 실패하지 않도록 골드·인벤 여유는 UPDATE 전에 미리 확인한다.

### Q7. 주기 저장 시 살아있는 플레이어의 데이터를 어떻게 안전하게 읽나?
타이머 워커가 인벤/골드를 읽는 동안 게임 스레드가 그것을 수정하면 반쪽 상태가 저장될 수 있다(데이터 레이스). **스냅샷-언더-락**으로 해결했다. 인벤/골드/장비를 바꾸는 메서드와 `TakeSnapshot`이 같은 락을 공유하되, 락 안에서는 **값 복사만** 하고(수 마이크로초) 느린 DB I/O는 락을 푼 뒤 복사본으로 수행한다. 락 보유 시간을 최소화해 게임 성능 영향을 없앴다. `Equip`이 내부에서 `AddItem`을 호출하므로 재귀 락(`recursive_mutex`)을 썼다.

### Q8. 왜 객체마다 타이머 스레드를 두지 않고 단일 타이머 스레드 + IOCP인가?
몬스터/이벤트마다 스레드를 두면 "몬스터 1000마리 = 스레드 1000개"로 컨텍스트 스위칭이 폭발한다. 단일 타이머 스레드가 우선순위 큐에서 만료 이벤트만 꺼내 IOCP에 던지고, 남는 워커가 처리한다. 이러면 **이벤트 수는 늘어도 스레드 수는 고정**이다. 공격 모션 후 실제 타격 판정, 리스폰, AI 틱, 주기 저장이 전부 이 한 경로로 통일된다.

### Q9. 위치를 매 틱 저장하지 않고 역산하는 이유는?
이동 중 위치를 계속 갱신·전송하면 트래픽과 상태가 늘고, 어느 시점의 위치인지 기준이 흔들린다. 대신 **이동 시작 위치·방향·속도·시작 시각**만 저장하면 서버는 임의 시각의 정확한 위치를 `시작 + 방향×속도×경과시간`으로 언제든 역산할 수 있다. 몬스터 사거리 계산, 해킹 검증, 도착 판정이 모두 이 한 함수(`GetCurrentPos`)로 해결된다. 상태를 최소화하고 계산으로 대체한 설계다.

### Q10. 서버 권위(server-authoritative)를 어디까지 적용했고 이유는?
이동 검증, 시야, 데미지/HP/사망, 인벤토리, 장비/스탯, 드롭, 경매 재고까지 **결과에 영향을 주는 모든 것**을 서버가 판정한다. 클라는 "의사(클릭/구매/사용)"만 보내고 결과를 스냅샷으로 받는다. 이유는 단순하다 — 클라는 신뢰할 수 없다. 인벤을 클라가 계산하면 아이템 복사·골드 조작이 가능하다. 그래서 인벤은 서버가 유일한 진실이고, 변경 후 전체 스냅샷(`SC_INVEN_UPDATE`)으로 클라를 맞춘다.

### Q11. 인벤토리를 왜 동적 할당 없이 값 배열로 관리하나?
`m_invenCode[40]`, `m_invenCount[40]` 같은 고정 배열은 (1) 할당/해제 비용과 단편화가 없고 (2) 캐시 지역성이 좋고 (3) 스냅샷 복사(`memcpy` 수준)와 직렬화가 단순하다. 슬롯 기반 인벤은 크기가 고정이라 값 배열이 자연스럽다. 드롭 풀도 존별 고정 배열(`new` 없음)로 관리해 런타임 할당을 피했다.

### Q12. 트랜잭션은 어디에 썼고 왜 필요한가?
여러 테이블/여러 문장을 원자적으로 처리해야 하는 곳에 썼다. 저장(character UPDATE + inventory/equipment DELETE→INSERT)과 경매 수령/취소(조회 후 UPDATE/DELETE)가 그렇다. 트랜잭션이 없으면 "인벤은 지웠는데 새로 넣기 전에 크래시" 같은 파손이 남는다. nanodbc `transaction` 객체는 `commit()` 없이 소멸되면 자동 롤백하므로, 예외가 나면 저장 전 상태가 그대로 보존된다.

### Q13. `Protocol.h`를 클라/서버에 복제하는데, 불일치 위험은?
직렬화 계약을 **바이트 단위로 공유**해야 해서 동일 복제본을 둔다. `#pragma pack(push,1)`로 패딩을 없애 컴파일러/플랫폼 차이를 제거하고, enum 값도 숫자까지 일치시킨다. 한쪽만 바꾸면 즉시 깨지므로 "양쪽 동시 수정"을 규칙으로 문서화했다. 규모가 커지면 IDL(FlatBuffers/Protobuf) 같은 스키마 기반 코드 생성으로 옮기는 게 정석이지만, 현재 규모에선 단순 복제가 오버헤드 없이 명확하다.

### Q14. 세션 수명은 어떻게 관리하나? (dangling I/O 문제)
`CSession`을 `shared_ptr`로 관리한다. IOCP는 완료가 비동기로 도착하므로, 진행 중인 I/O가 남았는데 세션을 파괴하면 크래시가 난다. `shared_ptr` 참조가 살아있는 동안 세션이 유지되고, 모든 I/O가 정리되면 해제된다. 송신은 세션당 순차(앞 패킷 완료 후 다음)로 큐잉해 send 버퍼와 오버랩 구조체 재사용을 안전하게 했다.

### Q15. 접속이 액션 도중에 끊기면?
`Disconnect`에서 (1) 로그인 상태면 현재 상태를 스냅샷으로 저장하고 (2) 존에서 LeaveZone 하고 (3) 플레이어/세션을 정리한다. 저장을 remove보다 먼저 해 데이터 유실을 막는다. 경매 구매처럼 서버가 원자적으로 처리하는 액션은, 끊김이 UPDATE 전이면 아무 일도 없었던 것과 같고 UPDATE 후면 이미 확정되어 다음 로그인에 반영된다.

### Q16. 지금 구조의 한계와 개선 방향은?
- **단일 프로세스/단일 DB 연결 모델**: 접속·저장이 급증하면 전용 DB 스레드 + 커넥션 풀, 나아가 존 단위 샤딩/채널 분리가 필요하다.
- **`Protocol.h` 수동 복제**: 스키마 기반 직렬화로 전환하면 불일치 위험이 사라진다.
- **경매 검색이 이름→코드 변환에 의존**: 아이템 메타(이름)를 서버/DB로 옮기면 서버 단독 검색이 가능하다.
- **비밀번호 평문**: 현재 학습용. 실제로는 해시(bcrypt 등) + 솔트가 필수이며, 컬럼은 이미 64자로 대비해 두었다.

---

## 9. 트러블슈팅 사례

### 사례 1: 위치 저장은 되는데 로그인하면 항상 같은 자리
증상 — 이동 후 로그아웃했다가 재접속하면 마을 중앙으로 복귀. DB에는 이동한 좌표가 정상 저장돼 있었다(저장은 정상). **DB에 distinctive 값(gold=7777, pos=25,25)을 직접 넣고** 로그인하니 골드는 반영되는데 위치만 안 먹었다 → 로드 쿼리는 정상, "위치 적용" 단계 문제로 좁혀짐. 원인은 클라 초기화가 좌표를 하드코딩 값으로 덮어쓰는 순서 버그였다. **위치 영속화를 붙이자 비로소 드러난 잠복 버그**로, "골드는 되고 위치만 안 된다"는 한 신호로 로드/적용을 분리 진단한 게 핵심.

### 사례 2: 동시 구매 시 아이템 복사 가능성
초기 인메모리 경매는 "확인 → 지급 → 확정"이 별개 락이라, 두 명이 동시에 마지막 재고를 사면 둘 다 확인을 통과하고 한 명은 지급받고도 확정에 실패하는 복사 창이 있었다(코드 주석이 이를 인지하고 있었다). DB 이전 시 **원자적 조건부 UPDATE + 지급 순서 재배치**로 근본 해결.

### 사례 3: 저장 스냅샷의 데이터 레이스
주기 저장을 도입하자 살아있는 플레이어의 인벤을 다른 스레드가 읽는 문제가 생겼다. **스냅샷-언더-락**(짧은 락으로 복사, I/O는 락 밖)으로 게임 성능 영향 없이 해결. `Equip→AddItem` 재귀 호출 때문에 `recursive_mutex`가 필요했던 것도 이 과정에서 정리.

---

*본 문서는 서버 구현에 초점을 맞췄다. 클라이언트(Direct2D 렌더링/UI/보간)는 서버-클라 계약을 설명하는 범위에서만 다뤘다.*
