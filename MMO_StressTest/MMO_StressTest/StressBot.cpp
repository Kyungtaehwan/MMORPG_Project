
//=================================================================
//   - 우리 헤더는 PacketHeader{uint16 size; uint16 id}
//   - 로그인은 id="bot_<n>" 로 보내면 서버가 STRESS_TEST 빌드에서 DB 없이 입장시킴
//   - 좌표는 float 아이소, 플레이어 속도 1타일/초(서버 해킹검증 허용오차 2타일)
//   - moveTime = QPC 기반 uint32 ms 스탬프. 서버는 해석하지 않고 브로드캐스트에
//     그대로 복사해 돌려주므로(Zone.cpp) 이 값이 곧 왕복 측정용 도장이다.
//
//  측정(콘솔, 1초 주기):
//   - 접속 봇 수, 초당 송신/수신 패킷(pps), 수신 KB/s
//   - 지연은 SC_MOVE_PLAYER 의 moveTime 으로 재되, 반드시 두 가지를 갈라서 낸다.
//       왕복(self)  : 내가 보낸 이동이 서버를 돌아 나에게 온 시간.
//                     SLO(왕복 p99 <= 100ms) 판정과 램프 제어는 이것만 쓴다.
//       전파(bcast) : 남이 움직인 걸 내가 보기까지 걸린 시간. 참고 지표.
//     예전엔 둘을 섞어 한 통에 넣었는데, SC_MOVE_PLAYER 의 대부분이 브로드캐스트라
//     통계가 사실상 전파시간이었다(= 교수님 기준과 다른 값을 비교하고 있었다).
//
//  서버측 순수 처리지연(p50/p99)·워커별 건수는 서버 콘솔에서 따로 본다.
// ================================================================
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <thread>
#include <vector>
#include <atomic>

#include "Protocol.h"

#pragma comment(lib, "ws2_32.lib")

// ==================== 실험 설정 (여기만 고치면 됨) ====================
enum class EZone : int { Town = 1, RaidObstacle = 5, RaidFlat = 6 };  // 5=Orc(A*), 6=Wing(A*X)
enum class EMode : int { Hold = 0, Ramp = 1 };

static const char* CFG_SERVER_IP = "127.0.0.1";       // IP
static const int   CFG_BOT_COUNT = 100000;               // Hold=목표수 / Ramp=최대상한
static const EZone CFG_ZONE      = EZone::RaidFlat;   // RaidFlat(6) / RaidObstacle(5) / Town(1)
static const EMode CFG_MODE      = EMode::Ramp;       // Hold(고정) / Ramp(자동증설)
// =====================================================================

static const uint16_t SERVER_PORT = 7777;

// 아래는 위 CFG 로 초기화되는 런타임 값(직접 고치지 말 것)
enum { MODE_HOLD = 0, MODE_RAMP = 1 };
static char g_serverIp[64]  = "127.0.0.1";
static int  g_targetBots    = 300;
static char g_botPw[20]     = "6";
static int  g_mode          = MODE_HOLD;

static const int WORKER_THREADS   = 6;
static const int CONNECT_BATCH    = 15;   // hold: 한 틱에 붙일 봇 수(빠르게 채움)
static const int RAMP_INTERVAL_MS = 10;  // hold: 램프 간격
static const int CONTROL_TICK_MS  = 30;   // 이동 구동 루프 주기
static const int DEST_IDLE_MS     = 150;  // 목적지 도착 후 다음 목적지까지 대기
static const float MOVE_SPEED     = 1.0f; // 타일/초 (서버와 동일해야 함)

// ramp 모드:
//   제어 신호는 왕복 p50 을 쓴다. p99 는 표본이 적으면 크게 흔들려 제어기가 진동한다
//   (p50 은 수십 표본으로도 수렴). 대신 "p50 은 멀쩡한데 꼬리만 터지는" 경우를
//   놓치지 않으려고 p99 에도 넉넉한 상한을 따로 건다.
//   후퇴(접속 끊기)는 넣지 않는다 - 램프는 위치 탐색용이고 정원 확정은 hold 로 한다.
//   (교수님 코드는 후퇴까지 하지만, 우리는 대량 disconnect 시 서버가 죽는 버그가
//    아직 있어 램프 중에 끊는 건 측정 자체를 날린다.)
//   램프가 내놓는 숫자는 보고용이 아니다. "대략 어디쯤"만 알면 되고,
//   정원 확정은 그 근처에서 hold 로 3분씩 재서 한다. 그러니 램프를 정밀하게
//   만들 이유가 없다 - 빠르게 밀어 임계를 찾고 거기서 멈추면 끝이다.
static const int RAMP_ADD_BATCH   = 50;     // 결정마다 붙일 봇 수
static const int RAMP_ADD_SLOW    = 5;      // p50 이 SLOW 임계를 넘은 뒤의 증설 폭
static const int RAMP_DECISION_MS = 1000;   // 결정 주기
static const int RAMP_P50_SLOW_MS = 100;    // 왕복 p50 이 넘으면 증설 폭을 줄인다
static const int RAMP_P50_STOP_MS = 150;    // 왕복 p50 이 넘으면 포화로 보고 증설 중단
static const int RAMP_P99_STOP_MS = 300;    // 왕복 p99 가 넘으면 중단(꼬리 보호)

// 봇 이동: 스폰(홈) 위치를 중심으로 WANDER 반경 안에서 배회.
//   맵이 150x150 이어도 봇이 스폰 근처에 흩어져 머물게 해 밀도 유지.
static const float WANDER    = 15.0f;  // 배회 반경(타일)
static const float STEP_MAX  = 4.0f;   // 한 번에 정하는 목적지 오프셋 최대(타일)

// 전투(실제 클라와 동일 규약): 사거리 안이면 멈춰서 CS_ATTACK_MONSTER.
static const float ATK_RANGE       = 2.5f;   // 서버 허용 3.0 안쪽으로
static const float SEEK_RANGE      = 12.0f;  // 이 안의 몬스터면 다가가 싸움
static const int   ATTACK_CD_MS    = 450;    // 서버 쿨 400ms 보다 약간 크게
static const int   DEST_RESEEK_MS  = 250;    // 몬스터 재조준 CS_MOVE_DEST 최소 간격

static const int MAX_BOTS      = 20000;  // 봇 배열 크기(= 최대 접속 상한). 서버 MAX_SESSION 과 맞춤.
static const int RECV_BUF_SIZE = 4096;  // 한 번 recv 버퍼
// 재조립 버퍼는 반드시 RECV_BUF_SIZE + 미완성 tail 보다 커야 한다.
// (한 번 recv 로 최대 RECV_BUF_SIZE 바이트가 오고, 앞의 미완성 조각이 더해지므로)
static const int ASM_SIZE      = 8192;  // 패킷 재조립 버퍼

// ---------------- 전역 ----------------
static HANDLE g_iocp = nullptr;

enum class IoOp { Recv };

struct OverlappedEx
{
    WSAOVERLAPPED over;
    WSABUF        wsabuf;
    char          buf[RECV_BUF_SIZE];
    IoOp          op;
};

struct Bot
{
    SOCKET   sock = INVALID_SOCKET;
    int      idx  = 0;

    std::atomic<bool> connected{ false };  // ENTER_GAME 수신 = 게임 입장
    std::atomic<bool> alive{ false };      // 소켓 살아있음

    uint32_t playerId = 0;

    // ---- 이동 상태 (control 스레드 전용) ----
    float    fx = 0.f, fz = 0.f;         // 현재 믿는 위치
    float    homeX = 0.f, homeZ = 0.f;   // 배회 중심(스폰/리스폰 위치)
    float    startX = 0.f, startZ = 0.f; // 이번 이동 시작점
    float    destX = 0.f, destZ = 0.f;
    bool     moving = false;
    uint32_t moveStartMs = 0;
    uint32_t nextDestMs = 0;
    int32_t  lastTileX = -9999;          // 마지막으로 CS_MOVE_POS 보낸 타일
    int32_t  lastTileZ = -9999;
    bool     ctrlDead = false;           // control 관점 사망(리스폰 대기)
    uint32_t lastAttackMs = 0;           // 마지막 CS_ATTACK_MONSTER 전송
    uint32_t lastDestMs = 0;             // 마지막 CS_MOVE_DEST 전송(재조준 스팸 방지)

    // ---- recv 스레드 -> control 스레드 신호 ----
    std::atomic<bool>  wantRespawn{ false };  // 내 HP<=0 감지
    std::atomic<bool>  gotRespawn{ false };   // SC_RESPAWN 도착
    std::atomic<float> rsX{ 0.f };
    std::atomic<float> rsZ{ 0.f };

    // 근처 몬스터 1기 추적(recv가 갱신, control이 읽어 접근·공격)
    std::atomic<int32_t> monId{ -1 };
    std::atomic<float>   monX{ 0.f };
    std::atomic<float>   monZ{ 0.f };

    // ---- 수신 재조립 (해당 소켓 워커 전용) ----
    OverlappedEx recvOver;
    unsigned char asmBuf[ASM_SIZE];
    int      asmLen = 0;
};

static Bot         g_bots[MAX_BOTS];
static std::atomic<int> g_numSockets{ 0 };  // connect 시도한 소켓 수
static std::atomic<int> g_active{ 0 };      // 게임 입장 완료 봇 수

// ---- 통계 카운터(구간 리셋) ----
static std::atomic<uint64_t> g_sentPkts{ 0 };
static std::atomic<uint64_t> g_recvPkts{ 0 };
static std::atomic<uint64_t> g_recvBytes{ 0 };
static std::atomic<uint64_t> g_monPkts{ 0 };   // 몬스터 패킷 수(AI 도는지 확인)
static std::atomic<uint64_t> g_atkSent{ 0 };   // 봇이 보낸 공격 패킷 수(전투 도는지 확인)

static std::atomic<bool> g_rampSaturated{ false };

// 지연 집계(ms). 버킷: 0..4999 = 1ms 단위, 5000 = 5000ms 이상.
//   범위를 넓혀 고부하에서 p50/p99 가 수백~수천 ms 까지 그대로 보이게.
//   self(내 왕복)와 bcast(남의 전파)를 절대 한 통에 섞지 않는다. 파일 첫머리 참고.
static const int LAT_BUCKETS = 5001;

struct LatStat
{
    std::atomic<uint32_t> bucket[LAT_BUCKETS];
    std::atomic<uint64_t> sum;
    std::atomic<uint64_t> count;
    std::atomic<uint32_t> max;
};
// 정적 저장기간이라 0 으로 초기화된다(기존 전역 배열과 같은 방식).
static LatStat g_self;    // 내 이동의 왕복      <- 1초 화면용
static LatStat g_bcast;   // 남의 이동의 전파    <- 참고
static LatStat g_ramp;    // 내 왕복(램프 제어 전용). 판정 주기가 콘솔과 달라 따로 둔다.
// 60초 창. 1초 창은 표본이 수천 개뿐이라 p99 가 상위 수십 개로 계산돼 크게 흔들린다
//  (실측: 같은 부하에서 1초 p99 가 115 -> 752ms 로 요동, 60초 창은 안정).
//  받아적을 값은 항상 이 요약이다.
static LatStat g_selfWin;

// ---- 시계 ------------------------------------------------------------
//  GetTickCount64() 를 쓰면 안 된다. 이 함수는 윈도우 시스템 타이머로만 갱신되고
//  그 주기가 기본 약 15.625ms 라, 1ms 버킷 히스토그램에 넣으면 지연이 아니라
//  "시계 눈금"을 재게 된다. (실측 증상: 봇 수를 1000/1500/2000 으로 바꿔도
//   p99 가 32/31/32ms 로 고정 = 15.625 x 2. 값이 안 변한 게 아니라 못 본 것.)
//  QueryPerformanceCounter 는 100ns 이하 정밀도라 진짜 1ms 해상도가 나온다.
//  반환 단위는 그대로 uint32 ms(프로세스 시작 기준). moveTime 필드 규약은
//  바뀌지 않는다 - 서버는 이 값을 해석하지 않고 그대로 돌려주기만 한다.
static inline int64_t QpcFreq()
{
    static const int64_t f = [] {
        LARGE_INTEGER li; QueryPerformanceFrequency(&li); return li.QuadPart;
    }();
    return f;
}

static inline uint32_t NowMs()
{
    static const int64_t s_base = [] {
        LARGE_INTEGER li; QueryPerformanceCounter(&li); return li.QuadPart;
    }();
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return static_cast<uint32_t>((li.QuadPart - s_base) * 1000 / QpcFreq());
}

static float RandRange(float lo, float hi)
{
    return lo + (hi - lo) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
}

static float Clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// 블로킹 send (control 스레드만 송신 -> 소켓당 동시 send 없음)
template <typename T>
static void SendPacket(Bot& b, T& pkt)
{
    int len = pkt.header.size;
    int off = 0;
    const char* p = reinterpret_cast<const char*>(&pkt);
    while (off < len)
    {
        int n = send(b.sock, p + off, len - off, 0);
        if (n == SOCKET_ERROR) return;   // 끊긴 소켓 -> 조용히 무시
        off += n;
    }
    g_sentPkts.fetch_add(1, std::memory_order_relaxed);
}

static void RecordInto(LatStat& st, uint32_t lat)
{
    int bkt = (lat >= 5000) ? 5000 : static_cast<int>(lat);
    st.bucket[bkt].fetch_add(1, std::memory_order_relaxed);
    st.sum.fetch_add(lat, std::memory_order_relaxed);
    st.count.fetch_add(1, std::memory_order_relaxed);
    uint32_t cur = st.max.load(std::memory_order_relaxed);
    while (lat > cur && !st.max.compare_exchange_weak(cur, lat, std::memory_order_relaxed)) {}
}

// 내 이동의 왕복. 교수님 코드의 if (ci == my_id) 분기에 해당하는 값이다.
//  건수가 브로드캐스트의 1/N 수준이라 표본을 버리지 않고 전부 센다
//  (여기서 1/4 만 남기면 p99 가 표본 부족으로 요동친다).
static void RecordLatencySelf(uint32_t moveTime)
{
    uint32_t lat = NowMs() - moveTime;       // uint32 랩 안전
    if (lat > 60000) return;                 // 명백한 노이즈(파싱/시계) 무시
    RecordInto(g_self, lat);
    RecordInto(g_selfWin, lat);   // 60초 창(받아적을 값)

    // 램프 판정도 왕복으로만 한다. 전파시간을 섞으면 임계의 의미가 흐려진다.
    RecordInto(g_ramp, lat);
}

// 남의 이동이 내 화면에 보이기까지. 건수가 압도적(N배)이라 1/4 만 표본으로 쓴다.
//  고부하에서 워커들이 공유 원자변수를 매 패킷 두들기면 경합이 봇 자체를 느리게 한다.
static void RecordLatencyBroadcast(uint32_t moveTime)
{
    thread_local uint32_t s_skip = 0;
    if ((s_skip++ & 3u) != 0u) return;

    uint32_t lat = NowMs() - moveTime;
    if (lat > 60000) return;
    RecordInto(g_bcast, lat);
}

// ---- 지연 통계 스냅샷 + 리셋 ----
//  버킷을 exchange(0) 하므로 "직전 구간"의 통계가 된다.
//  주의: 백분위수는 여기서 구간마다 새로 계산된다. 여러 구간의 p99 를 평균내는 건
//        통계적으로 의미가 없다 - 측정창 전체 p99 가 필요하면 버킷을 창 내내
//        누적한 뒤 마지막에 한 번만 계산해야 한다.
struct LatSnap
{
    uint64_t n;
    double   avg;
    int      p50, p99, p999;
    uint32_t max;
};

static LatSnap SnapLat(LatStat& st)
{
    LatSnap s{};
    uint32_t buckets[LAT_BUCKETS];
    uint64_t total = 0;
    for (int i = 0; i < LAT_BUCKETS; ++i)
    {
        buckets[i] = st.bucket[i].exchange(0);
        total += buckets[i];
    }
    uint64_t sum = st.sum.exchange(0);
    st.count.exchange(0);

    s.n   = total;
    s.max = st.max.exchange(0);
    s.avg = total ? static_cast<double>(sum) / total : 0.0;

    // 낮은 값부터 개수를 더해가다 전체의 p 비율을 넘기는 순간의 ms 값
    auto pct = [&](double p) -> int {
        if (!total) return 0;
        uint64_t target = static_cast<uint64_t>(total * p);
        if (target == 0) target = 1;
        uint64_t acc = 0;
        for (int i = 0; i < LAT_BUCKETS; ++i)
        {
            acc += buckets[i];
            if (acc >= target) return i;   // ms (5000 = 5000 이상)
        }
        return LAT_BUCKETS - 1;
    };
    s.p50  = pct(0.50);
    s.p99  = pct(0.99);
    s.p999 = pct(0.999);
    return s;
}

// ---- 수신 패킷 1건 처리 ----
static void ProcessPacket(Bot& b, unsigned char* data, int size)
{
    g_recvPkts.fetch_add(1, std::memory_order_relaxed);
    g_recvBytes.fetch_add(size, std::memory_order_relaxed);

    PacketHeader* h = reinterpret_cast<PacketHeader*>(data);

    // 몬스터 패킷(2100~2104)은 종류 무관하게 카운트 → 몬스터 AI 가동 증거
    if (h->id >= SC_ADD_MONSTER && h->id <= SC_MONSTER_HIT)
        g_monPkts.fetch_add(1, std::memory_order_relaxed);

    switch (h->id)
    {
    case SC_LOGIN_OK:
    {
        SC_LOGIN_OK_PACKET* p = reinterpret_cast<SC_LOGIN_OK_PACKET*>(data);
        b.playerId = p->playerID;
        break;
    }
    case SC_ENTER_GAME:
    {
        SC_ENTER_GAME_PACKET* p = reinterpret_cast<SC_ENTER_GAME_PACKET*>(data);
        b.fx = b.homeX = b.destX = p->fCurX;
        b.fz = b.homeZ = b.destZ = p->fCurZ;
        b.lastTileX = static_cast<int32_t>(floorf(p->fCurX));
        b.lastTileZ = static_cast<int32_t>(floorf(p->fCurZ));
        b.moving = false;
        b.ctrlDead = false;
        b.nextDestMs = NowMs();
        b.connected.store(true, std::memory_order_release);  // 마지막에 set
        g_active.fetch_add(1, std::memory_order_relaxed);
        break;
    }
    case SC_LOGIN_FAIL:
        // 우회 실패(서버가 STRESS_TEST 빌드가 아닐 때 등) -> 그냥 놔둠
        break;
    case SC_MOVE_PLAYER:
    {
        SC_MOVE_PLAYER_PACKET* p = reinterpret_cast<SC_MOVE_PLAYER_PACKET*>(data);
        // 내 것과 남의 것을 반드시 가른다 - 재는 대상 자체가 다르다.
        // (교수님 NetworkModule.cpp 의 if (ci == my_id) 와 같은 판별)
        // moveTime 은 어느 쪽이든 같은 프로세스/같은 시계가 찍은 값이라 유효하다.
        if (p->playerID == b.playerId) RecordLatencySelf(p->moveTime);
        else                           RecordLatencyBroadcast(p->moveTime);
        break;
    }
    case SC_PLAYER_HIT:
    {
        // 내가 몬스터한테 맞아 HP<=0 이면 리스폰 요청(control 스레드가 실제 전송)
        SC_PLAYER_HIT_PACKET* p = reinterpret_cast<SC_PLAYER_HIT_PACKET*>(data);
        if (p->playerID == b.playerId && p->nHp <= 0)
            b.wantRespawn.store(true, std::memory_order_relaxed);
        break;
    }
    case SC_RESPAWN:
    {
        SC_RESPAWN_PACKET* p = reinterpret_cast<SC_RESPAWN_PACKET*>(data);
        b.rsX.store(p->fCurX, std::memory_order_relaxed);
        b.rsZ.store(p->fCurZ, std::memory_order_relaxed);
        b.gotRespawn.store(true, std::memory_order_release);
        break;
    }
    case SC_ADD_MONSTER:
    {
        SC_ADD_MONSTER_PACKET* p = reinterpret_cast<SC_ADD_MONSTER_PACKET*>(data);
        b.monX.store(p->fCurX, std::memory_order_relaxed);
        b.monZ.store(p->fCurZ, std::memory_order_relaxed);
        b.monId.store(p->monsterID, std::memory_order_relaxed);  // 최근 본 몬스터를 타겟
        break;
    }
    case SC_MOVE_MONSTER:
    {
        SC_MOVE_MONSTER_PACKET* p = reinterpret_cast<SC_MOVE_MONSTER_PACKET*>(data);
        // 현재 타겟이면 위치 갱신, 없으면 이 몬스터를 타겟
        if (b.monId.load(std::memory_order_relaxed) == p->monsterID ||
            b.monId.load(std::memory_order_relaxed) < 0)
        {
            b.monX.store(p->fCurX, std::memory_order_relaxed);
            b.monZ.store(p->fCurZ, std::memory_order_relaxed);
            b.monId.store(p->monsterID, std::memory_order_relaxed);
        }
        break;
    }
    case SC_REMOVE_MONSTER:
    {
        SC_REMOVE_MONSTER_PACKET* p = reinterpret_cast<SC_REMOVE_MONSTER_PACKET*>(data);
        if (b.monId.load(std::memory_order_relaxed) == p->monsterID)
            b.monId.store(-1, std::memory_order_relaxed);   // 시야밖 → 타겟 해제
        break;
    }
    case SC_MONSTER_STATE:
    {
        // 타겟이 죽으면(state=MON_DEAD=5) 해제 → 봇이 그 자리에 안 서고 바로 이동/다음 몬스터
        SC_MONSTER_STATE_PACKET* p = reinterpret_cast<SC_MONSTER_STATE_PACKET*>(data);
        if (p->state == 5 && b.monId.load(std::memory_order_relaxed) == p->monsterID)
            b.monId.store(-1, std::memory_order_relaxed);
        break;
    }
    default:
        break;   // 그 외(ADD/REMOVE_PLAYER 등)는 배출만
    }
}

static void PostRecv(Bot& b)
{
    ZeroMemory(&b.recvOver.over, sizeof(b.recvOver.over));
    b.recvOver.op = IoOp::Recv;
    b.recvOver.wsabuf.buf = b.recvOver.buf;
    b.recvOver.wsabuf.len = RECV_BUF_SIZE;
    DWORD flags = 0;
    int ret = WSARecv(b.sock, &b.recvOver.wsabuf, 1, nullptr, &flags,
                      &b.recvOver.over, nullptr);
    if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        if (b.alive.exchange(false))
        {
            if (b.connected.load()) g_active.fetch_sub(1, std::memory_order_relaxed);
            closesocket(b.sock);
        }
    }
}

static void OnRecvComplete(Bot& b, int ioSize)
{
    if (ioSize <= 0)
    {
        if (b.alive.exchange(false))
        {
            if (b.connected.load()) g_active.fetch_sub(1, std::memory_order_relaxed);
            closesocket(b.sock);
        }
        return;
    }

    // 재조립 버퍼에 붙이기.
    // asmLen(미완성 tail) + ioSize 가 버퍼를 넘으면 memcpy 오버런 → 크래시.
    // ASM_SIZE >= RECV_BUF_SIZE + tail 이라 정상 트래픽에선 안 넘지만,
    // 넘으면(비정상) 이 봇을 정리하고 종료해 메모리 오염을 막는다.
    if (b.asmLen < 0 || ioSize < 0 || b.asmLen + ioSize > ASM_SIZE)
    {
        if (b.alive.exchange(false))
        {
            if (b.connected.load()) g_active.fetch_sub(1, std::memory_order_relaxed);
            closesocket(b.sock);
        }
        return;
    }
    memcpy(b.asmBuf + b.asmLen, b.recvOver.buf, ioSize);
    b.asmLen += ioSize;

    int cursor = 0;
    while (b.asmLen - cursor >= static_cast<int>(sizeof(PacketHeader)))
    {
        PacketHeader* h = reinterpret_cast<PacketHeader*>(b.asmBuf + cursor);
        int psize = h->size;
        if (psize < static_cast<int>(sizeof(PacketHeader)) || psize > ASM_SIZE)
        {
            b.asmLen = 0;   // 손상 -> 리셋
            cursor = 0;
            break;
        }
        if (b.asmLen - cursor < psize) break;   // 아직 덜 옴

        ProcessPacket(b, b.asmBuf + cursor, psize);
        cursor += psize;
    }
    if (cursor > 0)
    {
        b.asmLen -= cursor;
        if (b.asmLen > 0) memmove(b.asmBuf, b.asmBuf + cursor, b.asmLen);
    }

    PostRecv(b);
}

static void WorkerThread()
{
    while (true)
    {
        DWORD       ioSize = 0;
        ULONG_PTR   key = 0;
        OVERLAPPED* pover = nullptr;
        BOOL ok = GetQueuedCompletionStatus(g_iocp, &ioSize, &key, &pover, INFINITE);

        if (pover == nullptr) continue;
        Bot& b = g_bots[static_cast<int>(key)];

        if (!ok)
        {
            if (b.alive.exchange(false))
            {
                if (b.connected.load()) g_active.fetch_sub(1, std::memory_order_relaxed);
                closesocket(b.sock);
            }
            continue;
        }
        OnRecvComplete(b, static_cast<int>(ioSize));
    }
}

// 봇 1개 접속 + 로그인
static void ConnectOneBot(int idx)
{
    Bot& b = g_bots[idx];
    b.idx = idx;
    b.asmLen = 0;

    b.sock = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (b.sock == INVALID_SOCKET) return;

    // Nagle 끄기. 서버와 양쪽 다 꺼야 효과가 있다.
    // 켜져 있으면 LAN 측정에서 지연에 수십 ms 가 인위적으로 깔려
    // 서버 최적화로 아낀 수 us 가 그 밑에 묻혀버린다.
    BOOL bNoDelay = TRUE;
    setsockopt(b.sock, IPPROTO_TCP, TCP_NODELAY,
               reinterpret_cast<const char*>(&bNoDelay), sizeof(bNoDelay));

    sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, g_serverIp, &addr.sin_addr);

    if (WSAConnect(b.sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr),
                   nullptr, nullptr, nullptr, nullptr) == SOCKET_ERROR)
    {
        closesocket(b.sock);
        b.sock = INVALID_SOCKET;
        return;
    }

    CreateIoCompletionPort(reinterpret_cast<HANDLE>(b.sock), g_iocp,
                           static_cast<ULONG_PTR>(idx), 0);
    b.alive.store(true);
    PostRecv(b);

    // 로그인: id = "bot_<idx>"  (서버 STRESS_TEST 우회)
    CS_LOGIN_PACKET login;
    ZeroMemory(&login, sizeof(login));
    login.header.size = sizeof(login);
    login.header.id = CS_LOGIN;
    sprintf_s(login.id, sizeof(login.id), "bot_%d", idx);
    strcpy_s(login.pw, sizeof(login.pw), g_botPw);   // pw = 존번호(서버가 이 존으로 입장)
    SendPacket(b, login);
    // 인덱스(=g_numSockets)는 커넥터 스레드가 관리한다. 여기선 증가시키지 않는다.
}

// 새 목적지로 "클릭"(CS_MOVE_DEST). 실제 클라처럼 목적지 바뀔 때마다 1회.
static void SendMoveDest(Bot& b, float dx, float dz, uint32_t now)
{
    b.startX = b.fx; b.startZ = b.fz;
    b.destX = dx;    b.destZ = dz;
    b.moveStartMs = now;
    b.lastDestMs = now;
    b.moving = true;

    CS_MOVE_DEST_PACKET pkt;
    ZeroMemory(&pkt, sizeof(pkt));
    pkt.header.size = sizeof(pkt);
    pkt.header.id = CS_MOVE_DEST;
    pkt.fDestX = dx;
    pkt.fDestZ = dz;
    pkt.moveTime = now;
    SendPacket(b, pkt);
}

// 현재 위치 보고(CS_MOVE_POS). 실제 클라처럼 타일 변경/도착 시에만.
static void SendMovePos(Bot& b, uint32_t now)
{
    b.lastTileX = static_cast<int32_t>(floorf(b.fx));
    b.lastTileZ = static_cast<int32_t>(floorf(b.fz));
    CS_MOVE_POS_PACKET pkt;
    ZeroMemory(&pkt, sizeof(pkt));
    pkt.header.size = sizeof(pkt);
    pkt.header.id = CS_MOVE_POS;
    pkt.fCurX = b.fx;
    pkt.fCurZ = b.fz;
    pkt.moveTime = now;
    SendPacket(b, pkt);
}

// 봇 1개 구동: 리스폰 → 전투(다가가 공격) → 배회 → 이동보간+타일변경전송
static void DriveBot(Bot& b, uint32_t now)
{
    if (!b.connected.load(std::memory_order_acquire)) return;
    if (!b.alive.load()) return;

    // ---- 리스폰 신호 처리 ----
    if (b.gotRespawn.exchange(false, std::memory_order_acquire))
    {
        b.fx = b.homeX = b.rsX.load(std::memory_order_relaxed);
        b.fz = b.homeZ = b.rsZ.load(std::memory_order_relaxed);
        b.lastTileX = static_cast<int32_t>(floorf(b.fx));
        b.lastTileZ = static_cast<int32_t>(floorf(b.fz));
        b.moving = false;
        b.ctrlDead = false;
        b.monId.store(-1, std::memory_order_relaxed);
        b.nextDestMs = now;
    }
    if (b.wantRespawn.exchange(false, std::memory_order_relaxed))
    {
        CS_RESPAWN_PACKET pkt;
        ZeroMemory(&pkt, sizeof(pkt));
        pkt.header.size = sizeof(pkt);
        pkt.header.id = CS_RESPAWN;
        SendPacket(b, pkt);
        b.ctrlDead = true;
        b.moving = false;
    }
    if (b.ctrlDead) return;   // 죽어있으면 리스폰 대기(이동/공격 안 함)

    // ---- 전투: 근처 몬스터가 있으면 다가가 공격 ----
    bool bSeeking = false;
    int32_t mid = b.monId.load(std::memory_order_relaxed);
    if (mid >= 0)
    {
        float mx = b.monX.load(std::memory_order_relaxed);
        float mz = b.monZ.load(std::memory_order_relaxed);
        float d = sqrtf((b.fx - mx) * (b.fx - mx) + (b.fz - mz) * (b.fz - mz));

        if (d <= ATK_RANGE)
        {
            // 사거리 안: 멈춰서 공격(서버도 공격 시 플레이어 정지시킴)
            b.moving = false;
            if (now - b.lastAttackMs >= static_cast<uint32_t>(ATTACK_CD_MS))
            {
                b.lastAttackMs = now;
                CS_ATTACK_MONSTER_PACKET pkt;
                ZeroMemory(&pkt, sizeof(pkt));
                pkt.header.size = sizeof(pkt);
                pkt.header.id = CS_ATTACK_MONSTER;
                pkt.monsterID = mid;
                pkt.fCurX = b.fx;
                pkt.fCurZ = b.fz;
                SendPacket(b, pkt);
                g_atkSent.fetch_add(1, std::memory_order_relaxed);
            }
            return;
        }
        if (d <= SEEK_RANGE)
        {
            // 다가가기: 몬스터에서 1.5타일 앞(내 쪽)으로 목적지 설정
            float ux = b.fx - mx, uz = b.fz - mz;
            float ul = sqrtf(ux * ux + uz * uz);
            if (ul < 0.001f) ul = 1.f;
            float tx = mx + (ux / ul) * 1.5f;
            float tz = mz + (uz / ul) * 1.5f;
            if (tx < 1.5f) tx = 1.5f;
            if (tz < 1.5f) tz = 1.5f;

            bool bDestStale = (fabsf(tx - b.destX) + fabsf(tz - b.destZ)) > 1.0f;
            if ((!b.moving || bDestStale) &&
                now - b.lastDestMs >= static_cast<uint32_t>(DEST_RESEEK_MS))
                SendMoveDest(b, tx, tz, now);
            bSeeking = true;
        }
    }

    // ---- 배회: 쫓을 몬스터 없으면 홈 주변 랜덤 목적지 ----
    if (!bSeeking && !b.moving && now >= b.nextDestMs)
    {
        float dx = Clampf(b.fx + RandRange(-STEP_MAX, STEP_MAX),
                          (b.homeX - WANDER < 1.5f ? 1.5f : b.homeX - WANDER),
                          b.homeX + WANDER);
        float dz = Clampf(b.fz + RandRange(-STEP_MAX, STEP_MAX),
                          (b.homeZ - WANDER < 1.5f ? 1.5f : b.homeZ - WANDER),
                          b.homeZ + WANDER);
        SendMoveDest(b, dx, dz, now);
    }

    if (!b.moving) return;   // 진행할 이동 없음

    // ---- 이동 보간(속도 = MOVE_SPEED 타일/초) + 타일 변경/도착 시 전송 ----
    float dx = b.destX - b.startX;
    float dz = b.destZ - b.startZ;
    float dist = sqrtf(dx * dx + dz * dz);
    float travelled = MOVE_SPEED * ((now - b.moveStartMs) / 1000.f);

    if (dist < 0.001f || travelled >= dist)
    {
        b.fx = b.destX;
        b.fz = b.destZ;
        b.moving = false;
        b.nextDestMs = now + DEST_IDLE_MS;
        SendMovePos(b, now);   // 실제 클라: 도착 시 항상 1회
        return;
    }

    float t = travelled / dist;
    b.fx = b.startX + dx * t;
    b.fz = b.startZ + dz * t;

    // 실제 클라: 이동 중엔 타일이 바뀔 때만 전송
    int32_t tileX = static_cast<int32_t>(floorf(b.fx));
    int32_t tileZ = static_cast<int32_t>(floorf(b.fz));
    if (tileX != b.lastTileX || tileZ != b.lastTileZ)
        SendMovePos(b, now);
}

// 인덱스 하나 claim 해서 봇 1개 접속 (커넥터 전용)
static void SpawnNextBot()
{
    int idx = g_numSockets.fetch_add(1, std::memory_order_relaxed);
    if (idx >= MAX_BOTS) { g_numSockets.store(MAX_BOTS); return; }
    ConnectOneBot(idx);   // 실패해도 그 슬롯은 alive=false 로 남고 DriveBot이 건너뜀
}

// 커넥터 스레드: 접속만 담당(블로킹 connect가 이동 구동을 막지 않게 분리).
static void ConnectorThread()
{
    uint32_t lastRamp     = NowMs();
    uint32_t lastDecision = NowMs();
    while (true)
    {
        uint32_t now = NowMs();
        int cur = g_numSockets.load();

        if (cur >= g_targetBots) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }

        if (g_mode == MODE_HOLD)
        {
            if (now - lastRamp >= static_cast<uint32_t>(RAMP_INTERVAL_MS))
            {
                lastRamp = now;
                for (int i = 0; i < CONNECT_BATCH && g_numSockets.load() < g_targetBots; ++i)
                    SpawnNextBot();
            }
        }
        else // MODE_RAMP: 교수님 STRESS_TEST 와 같은 구조 - 밀다가 임계 넘으면 멈춘다
        {
            if (g_rampSaturated.load())
            {
                // 포화 확정 후에는 그 인원을 그대로 유지한다(끊지 않는다).
                // 여기서부터가 hold 구간이므로 서버 콘솔의 60초 요약을 받아적으면 된다.
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            if (now - lastDecision >= static_cast<uint32_t>(RAMP_DECISION_MS))
            {
                lastDecision = now;
                const LatSnap r = SnapLat(g_ramp);   // 직전 1초의 왕복

                if (r.n == 0)
                {
                    // 아직 왕복 표본이 없다(막 시작). 판정 없이 붙인다.
                    // 여기서 멈추면 봇 0명 -> 표본 0 -> 영원히 증설 안 하는 데드락이 된다.
                    for (int i = 0; i < RAMP_ADD_BATCH && g_numSockets.load() < g_targetBots; ++i)
                        SpawnNextBot();
                }
                else if (r.p50 > RAMP_P50_STOP_MS || r.p99 > RAMP_P99_STOP_MS)
                {
                    g_rampSaturated.store(true);
                    printf(">>> [RAMP] 포화: 접속 %d 에서 왕복 p50 %dms / p99 %dms "
                           "(임계 p50 %d / p99 %d) <<<\n",
                           g_active.load(), r.p50, r.p99,
                           RAMP_P50_STOP_MS, RAMP_P99_STOP_MS);
                    printf(">>> 이 인원을 유지한다. 이제 서버 콘솔의 60초 요약을 받아적으면 된다. <<<\n");
                    fflush(stdout);
                }
                else
                {
                    const int batch = (r.p50 > RAMP_P50_SLOW_MS) ? RAMP_ADD_SLOW
                                                                 : RAMP_ADD_BATCH;
                    printf("    [RAMP] 접속 %d  왕복 p50 %dms p99 %dms (표본 %llu) -> +%d\n",
                           g_active.load(), r.p50, r.p99,
                           static_cast<unsigned long long>(r.n), batch);
                    fflush(stdout);

                    for (int i = 0; i < batch && g_numSockets.load() < g_targetBots; ++i)
                        SpawnNextBot();
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// 이동/전투 구동 스레드 (접속과 분리 — 접속이 막혀도 이동은 계속 돈다)
static void ControlThread()
{
    while (true)
    {
        uint32_t now = NowMs();
        int n = g_numSockets.load();
        if (n > MAX_BOTS) n = MAX_BOTS;
        for (int i = 0; i < n; ++i)
            DriveBot(g_bots[i], now);

        std::this_thread::sleep_for(std::chrono::milliseconds(CONTROL_TICK_MS));
    }
}

// ---- 통계 스냅샷 + 리셋, 콘솔 출력 ----
static void PrintStats(double dtSec)
{
    uint64_t sent  = g_sentPkts.exchange(0);
    uint64_t recv  = g_recvPkts.exchange(0);
    uint64_t bytes = g_recvBytes.exchange(0);
    uint64_t mon   = g_monPkts.exchange(0);
    uint64_t atk   = g_atkSent.exchange(0);

    const LatSnap self  = SnapLat(g_self);
    const LatSnap bcast = SnapLat(g_bcast);

    double sendPps = sent / dtSec;
    double recvPps = recv / dtSec;
    double kbps    = (bytes / 1024.0) / dtSec;

    printf("접속 %4d/%d | 송신 %6.0f 수신 %7.0f pps %6.1f KB/s | 몬 %6.0f 공격 %5.0f /s\n"
           "   왕복 n %7llu  avg %6.1f  p50 %4d  p99 %4d  p99.9 %4d  max %5u ms   <- SLO 판정\n"
           "   전파 n %7llu  avg %6.1f  p50 %4d  p99 %4d  p99.9 %4d  max %5u ms\n",
           g_active.load(), g_targetBots,
           sendPps, recvPps, kbps,
           mon / dtSec, atk / dtSec,
           static_cast<unsigned long long>(self.n),
           self.avg,  self.p50,  self.p99,  self.p999,  self.max,
           static_cast<unsigned long long>(bcast.n),
           bcast.avg, bcast.p50, bcast.p99, bcast.p999, bcast.max);
    fflush(stdout);   // 파일/파이프 리다이렉트 시 즉시 반영

    // ---- 60초 창 요약: 표에 받아적을 값은 위 1초 줄이 아니라 이것 ----
    static double s_winSec = 0.0;
    static int    s_winIdx = 0;
    s_winSec += dtSec;
    if (s_winSec >= 60.0)
    {
        const LatSnap w = SnapLat(g_selfWin);
        printf("=== [봇 요약 %d] %.0fs  접속 %d  표본 %llu  |  왕복 "
               "p50 %d  p99 %d  p99.9 %d  max %u ms  <- 받아적을 값\n",
               ++s_winIdx, s_winSec, g_active.load(),
               static_cast<unsigned long long>(w.n),
               w.p50, w.p99, w.p999, w.max);
        fflush(stdout);
        s_winSec = 0.0;
    }
}

int main(int argc, char** argv)
{
    // 1) 기본값 = 위 CFG 설정 (VS에서 F5로 실행하면 이대로 동작)
    strncpy_s(g_serverIp, sizeof(g_serverIp), CFG_SERVER_IP, _TRUNCATE);
    g_targetBots = CFG_BOT_COUNT;
    sprintf_s(g_botPw, sizeof(g_botPw), "%d", static_cast<int>(CFG_ZONE));
    g_mode = (CFG_MODE == EMode::Ramp) ? MODE_RAMP : MODE_HOLD;

    // 2) 명령줄 인자가 있으면 우선 적용 (노트북 배치 실행용, 선택)
    //    <서버IP> <봇수> <존> [hold|ramp]
    if (argc >= 2) strncpy_s(g_serverIp, sizeof(g_serverIp), argv[1], _TRUNCATE);
    if (argc >= 3)
    {
        int t = atoi(argv[2]);
        if (t > 0 && t <= MAX_BOTS) g_targetBots = t;
    }
    if (argc >= 4) strncpy_s(g_botPw, sizeof(g_botPw), argv[3], _TRUNCATE);
    if (argc >= 5) g_mode = (_stricmp(argv[4], "ramp") == 0) ? MODE_RAMP : MODE_HOLD;

    // 봇 배열 상한 넘지 않게 클램프(초과 시 배열 밖 접근 방지)
    if (g_targetBots > MAX_BOTS) g_targetBots = MAX_BOTS;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("WSAStartup 실패\n");
        return 1;
    }

    g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);

    printf("=== MMO 부하 봇 ===  대상 %s:%d  존 %s  %s  %s %d\n",
           g_serverIp, SERVER_PORT, g_botPw,
           (g_mode == MODE_RAMP ? "[RAMP 자동증설]" : "[HOLD 고정]"),
           (g_mode == MODE_RAMP ? "최대" : "목표"), g_targetBots);
    fflush(stdout);

    // 수신 워커 수 = 하드웨어 스레드 수(코어). 고부하에서 recv 를 최대한 병렬로 뺀다.
    //   단, 서버와 같은 머신(localhost)이면 서버 워커와 코어를 나눠 쓰니 주의.
    unsigned hc = std::thread::hardware_concurrency();
    int nWorkers = (hc > 0) ? static_cast<int>(hc) : WORKER_THREADS;
    printf("수신 워커 스레드: %d\n", nWorkers);
    fflush(stdout);

    std::vector<std::thread> workers;
    for (int i = 0; i < nWorkers; ++i)
        workers.emplace_back(WorkerThread);

    std::thread control(ControlThread);
    std::thread connector(ConnectorThread);   // 접속 전담(이동 구동과 분리)

    // 메인: 1초마다 통계 출력
    uint32_t last = NowMs();
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        uint32_t now = NowMs();
        double dt = (now - last) / 1000.0;
        if (dt < 0.001) dt = 0.001;
        last = now;
        PrintStats(dt);
    }

    // (도달하지 않음) 정리
    control.join();
    connector.join();
    for (auto& w : workers) w.join();
    WSACleanup();
    return 0;
}
