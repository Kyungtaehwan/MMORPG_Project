#include "pch.h"
#include "DB_Manager.h"
#include "nanodbc.h"
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>
#include <chrono>

#pragma comment(lib, "odbc32.lib")   // ODBC 네이티브 API 링크 (nanodbc 백엔드)

// 부하 테스트 빌드에선 저장 성공/실패 로그를 무음 처리

#ifdef STRESS_TEST
struct DbNullSink {
    template <class T> DbNullSink& operator<<(const T&) { return *this; }
    DbNullSink& operator<<(std::ostream& (*)(std::ostream&)) { return *this; }
};
#define SLOG (DbNullSink{})
#else
#define SLOG std::cout
#endif

CDB_Manager* CDB_Manager::m_pInstance = nullptr;

// ----------------------------------------------------------------
//  주어진 커넥션으로 저장 트랜잭션 1건 실행(공통 SQL).
//   동기 Save호출마다 새 커넥션와 전용 스레드가 공유한다.
// ----------------------------------------------------------------
static void SaveWithConn(nanodbc::connection& conn, const FSaveSnapshot& snap)
{
    const char* id = snap.id;

    nanodbc::transaction tx(conn);   // 시작: commit 안 하고 소멸되면 자동 롤백

    // 1) character UPDATE 계정당 1행 보장
    {
        nanodbc::statement s(conn);
        nanodbc::prepare(s, NANODBC_TEXT(
            "UPDATE `character` SET zone_id=?, spawn_x=?, spawn_z=?, gold=?, level=?, exp=? "
            "WHERE account_id=?"));
        int   zone  = snap.zoneID;
        float x     = snap.x;
        float z     = snap.z;
        int   gold  = snap.gold;
        int   level = snap.level;
        int   exp   = snap.exp;
        s.bind(0, &zone);
        s.bind(1, &x);
        s.bind(2, &z);
        s.bind(3, &gold);
        s.bind(4, &level);
        s.bind(5, &exp);
        s.bind(6, id);
        nanodbc::execute(s);
    }

    // 2) inventory : 전부 지우고 다시 채우기
    {
        nanodbc::statement d(conn);
        nanodbc::prepare(d, NANODBC_TEXT("DELETE FROM inventory WHERE account_id=?"));
        d.bind(0, id);
        nanodbc::execute(d);
    }
    for (int i = 0; i < FSaveSnapshot::INVEN; ++i)
    {
        if (snap.invenCode[i] <= 0) continue;   // 빈 칸은 저장 안 함
        nanodbc::statement s(conn);
        nanodbc::prepare(s, NANODBC_TEXT(
            "INSERT INTO inventory (account_id, slot, item_code, count) VALUES (?,?,?,?)"));
        int slot = i;
        int code = snap.invenCode[i];
        int cnt  = snap.invenCount[i];
        s.bind(0, id);
        s.bind(1, &slot);
        s.bind(2, &code);
        s.bind(3, &cnt);
        nanodbc::execute(s);
    }

    // 3) equipment : 전부 지우고 착용한 것만
    {
        nanodbc::statement d(conn);
        nanodbc::prepare(d, NANODBC_TEXT("DELETE FROM equipment WHERE account_id=?"));
        d.bind(0, id);
        nanodbc::execute(d);
    }
    for (int i = 0; i < FSaveSnapshot::EQUIP; ++i)
    {
        if (snap.equipCode[i] <= 0) continue;
        nanodbc::statement s(conn);
        nanodbc::prepare(s, NANODBC_TEXT(
            "INSERT INTO equipment (account_id, slot, item_code) VALUES (?,?,?)"));
        int slot = i;
        int code = snap.equipCode[i];
        s.bind(0, id);
        s.bind(1, &slot);
        s.bind(2, &code);
        nanodbc::execute(s);
    }

    // 4) quickslot : 전부 지우고 등록된 칸만 (인벤/장비와 같은 스냅샷 방식)
    {
        nanodbc::statement d(conn);
        nanodbc::prepare(d, NANODBC_TEXT("DELETE FROM quickslot WHERE account_id=?"));
        d.bind(0, id);
        nanodbc::execute(d);
    }
    for (int i = 0; i < FSaveSnapshot::QUICK; ++i)
    {
        if (snap.quickCode[i] <= 0) continue;   // 빈 칸은 저장 안 함
        nanodbc::statement s(conn);
        nanodbc::prepare(s, NANODBC_TEXT(
            "INSERT INTO quickslot (account_id, slot, item_code) VALUES (?,?,?)"));
        int slot = i;
        int code = snap.quickCode[i];
        s.bind(0, id);
        s.bind(1, &slot);
        s.bind(2, &code);
        nanodbc::execute(s);
    }

    tx.commit();   // 여기까지 다 성공 - 확정
}

// ----------------------------------------------------------------
//  Init : 연결 문자열 세팅 + 접속 테스트
//   - DSN 없이 드라이버 이름을 직접 박은 "DSN-less" 연결 문자열.
// ----------------------------------------------------------------
bool CDB_Manager::Init()
{
    m_connStr =
        "Driver={MySQL ODBC 9.7 Unicode Driver};"
        "Server=127.0.0.1;"
        "Port=3306;"
        "Database=mmorpg;"
        "User=mmo_server;"
        "Password=1234;"
        "CHARSET=utf8mb4;";//텍스트 UTF8

    // 백섭은 mmorpg 만 되돌리고 mmorpg_log 는 손대지 않는다 - 그래서 연결도 따로 잡는다.
    m_logConnStr =
        "Driver={MySQL ODBC 9.7 Unicode Driver};"
        "Server=127.0.0.1;"
        "Port=3306;"
        "Database=mmorpg_log;"
        "User=mmo_server;"
        "Password=1234;"
        "CHARSET=utf8mb4;";

    try
    {
        nanodbc::connection conn(m_connStr);
        std::cout << "[DB] connected: "
                  << conn.dbms_name() << " " << conn.dbms_version() << "\n";
        return true;
    }
    catch (const std::exception& e)
    {
        std::cout << "[DB] connect FAILED: " << e.what() << "\n";
        return false;
    }
}

// ----------------------------------------------------------------
//  Login : sp_login(id, pw) 호출 - 결과셋 3개 파싱
//   결과셋1 character  : 성공 시 1행(없으면 인증 실패)
//   결과셋2 inventory  : 아이템 행들 - DB slot 그대로 out.inven[slot]
//   결과셋3 equipment  : 장착 행들   - out.equip[]
// ----------------------------------------------------------------
bool CDB_Manager::Login(const char* id, const char* pw, FAccountData& out)
{
    try
    {
        nanodbc::connection conn(m_connStr);   // 모델 A: 호출마다 새 연결

        nanodbc::statement stmt(conn);
        nanodbc::prepare(stmt, NANODBC_TEXT("{CALL sp_login(?, ?)}"));
        stmt.bind(0, id);
        stmt.bind(1, pw);

        nanodbc::result r = nanodbc::execute(stmt);

        // ---- 결과셋1: character (성공 판정) ----
        if (!r.next())
            return false;   // 0행 = 아이디/비번 불일치

        std::memset(&out, 0, sizeof(out));
        out.id     = nullptr;   // DB 경로에선 id/pw 포인터 미사용
        out.pw     = nullptr;
        out.zoneID = r.get<int>(0);     // zone_id
        out.spawnX = r.get<float>(1);   // spawn_x
        out.spawnZ = r.get<float>(2);   // spawn_z
        out.gold   = r.get<int>(3);     // gold
        out.level  = r.get<int>(4);     // level
        out.exp    = r.get<int>(5);     // exp

        // ---- 결과셋2: inventory (slot, item_code, count) ----
        if (r.next_result())
        {
            while (r.next())
            {
                const int slot  = r.get<int>(0);
                const int code  = r.get<int>(1);
                const int count = r.get<int>(2);

                // 잘못된 DB 행은 플레이어 메모리를 오염시키지 않고 무시한다.
                if (slot < 0 || slot >= ACCOUNT_INVEN_SIZE) continue;
                if (code <= 0 || count <= 0) continue;

                out.inven[slot].code  = code;
                out.inven[slot].count = count;
            }
        }

        // ---- 결과셋3: equipment (slot, item_code) ----
        if (r.next_result())
        {
            while (r.next())
            {
                int slot = r.get<int>(0);   // slot (0~5)
                int code = r.get<int>(1);   // item_code
                if (slot >= 0 && slot < 6)
                    out.equip[slot] = code;
            }
        }

        // ---- 결과셋4: quickslot (slot, item_code) ----
        if (r.next_result())
        {
            while (r.next())
            {
                int slot = r.get<int>(0);   // slot (0~7)
                int code = r.get<int>(1);   // item_code
                if (slot >= 0 && slot < 8)
                    out.quick[slot] = code;
            }
        }

        return true;
    }
    catch (const std::exception& e)
    {
        std::cout << "[DB] Login error: " << e.what() << "\n";
        return false;
    }
}

// ----------------------------------------------------------------
//   플레이어 상태를 DB 저장 (트랜잭션)
//   1) character 1행 UPDATE (존/위치/골드)
//   2) inventory : 그 계정 행 전부 DELETE - 채워진 칸만 INSERT (스냅샷)
//   3) equipment : 그 계정 행 전부 DELETE - 착용한 것만 INSERT
//   위 전부를 한 트랜잭션으로 - 중간 실패 시 롤백(부분 저장 방지).
// ----------------------------------------------------------------
bool CDB_Manager::Save(const FSaveSnapshot& snap)
{
    try
    {
        nanodbc::connection conn(m_connStr);   // 동기 경로: 호출마다 새 연결
        SaveWithConn(conn, snap);
        SLOG << "[DB] saved: " << snap.id << " (zone=" << snap.zoneID
             << " gold=" << snap.gold
             << " Lv" << snap.level << " exp=" << snap.exp << ")\n";
        return true;
    }
    catch (const std::exception& e)
    {
        // tx 소멸자가 롤백 - DB는 저장 전 상태 그대로
        SLOG << "[DB] Save error(" << snap.id << "): " << e.what() << "\n";
        return false;
    }
}

// ----------------------------------------------------------------
//  저장 전용 스레드
// ----------------------------------------------------------------
void CDB_Manager::StartSaveThread()
{
    if (m_saveRunning.exchange(true)) return;   // 이미 돌고 있으면 무시
    m_saveThread = std::thread(&CDB_Manager::SaveThreadFunc, this);
    std::cout << "[DB] 저장 전용 스레드 시작\n";
}

void CDB_Manager::StopSaveThread()
{
    if (!m_saveRunning.exchange(false)) return;
    m_queueCv.notify_all();                      // 대기 중이면 깨워서 종료 확인
    if (m_saveThread.joinable()) m_saveThread.join();
    std::cout << "[DB] 저장 전용 스레드 종료\n";
}

void CDB_Manager::EnqueueSave(const FSaveSnapshot& snap)
{
    {
        std::lock_guard<std::mutex> lk(m_queueLock);
        m_saveQueue.push(snap);                  // 스냅샷 복사본을 큐에(호출 스레드는 즉시 반환)
    }
    m_queueCv.notify_one();
}

void CDB_Manager::SaveThreadFunc()
{
    // 이 스레드 전용 영속 커넥션 1개 — 재사용해서 connection-per-call 비용 제거.
    // 연결 실패/끊김 시 다음 저장 때 재연결을 시도한다.
    std::unique_ptr<nanodbc::connection> conn;
    auto EnsureConn = [&]() -> bool
    {
        if (conn && conn->connected()) return true;
        try
        {
            conn = std::make_unique<nanodbc::connection>(m_connStr);
            return true;
        }
        catch (const std::exception& e)
        {
            std::cout << "[DB thread] connect FAILED: " << e.what() << "\n";
            conn.reset();
            return false;
        }
    };

    while (true)
    {
        FSaveSnapshot snap;
        {
            std::unique_lock<std::mutex> lk(m_queueLock);
            m_queueCv.wait(lk, [&] { return !m_saveQueue.empty() || !m_saveRunning.load(); });
            // 종료 요청 + 큐 비었으면 루프 탈출(남은 건 아래에서 처리됨)
            if (m_saveQueue.empty())
            {
                if (!m_saveRunning.load()) break;
                continue;
            }
            snap = m_saveQueue.front();
            m_saveQueue.pop();
        }

        if (!EnsureConn())
            continue;   // DB 미접속: 이 저장은 버림(스트레스 편의). 재시도는 추후.

        try
        {
            SaveWithConn(*conn, snap);
            SLOG << "[DB] saved: " << snap.id << " (zone=" << snap.zoneID
                 << " gold=" << snap.gold
                 << " Lv" << snap.level << " exp=" << snap.exp << ")\n";
        }
        catch (const std::exception& e)
        {
            SLOG << "[DB thread] save error(" << snap.id << "): " << e.what() << "\n";
            conn.reset();   // 커넥션이 깨졌을 수 있으니 다음 루프에서 재연결
        }
    }
}

// ================================================================
//  로그 전용 스레드 (거래 로그 -> mmorpg_log.game_log)
// ================================================================

// 배치 튜닝값
static constexpr size_t LOG_BATCH_MAX = 100;      // 이만큼 모이면 즉시 기록
static constexpr int    LOG_FLUSH_MS  = 1000;     // 다 안 차도 이 주기로는 기록(한산할 때 지연 방지)
static constexpr size_t LOG_QUEUE_MAX = 100000;   // 큐 상한. 넘으면 버린다
static constexpr short  LOG_COLS      = 8;        // 한 행당 바인딩할 컬럼 수

// 파라미터 인덱스가 short 라 배치가 커지면 넘침
static_assert(LOG_BATCH_MAX * LOG_COLS < 32767, "배치가 너무 커서 파라미터 인덱스가 넘친다");

// N행짜리 다중행 INSERT 문을 만든다.
// 값을 문자열로 이어붙이지 않고 전부 ? 로 두는 이유: 문자열 조립은 이스케이프를
// 한 군데만 빠뜨려도 구문이 깨지거나 주입이 된다. 바인딩은 그 여지가 없다.
static std::string BuildLogInsertSql(size_t rows)
{
    std::string sql =
        "INSERT INTO game_log "
        "(log_type, actor, target, item_code, quantity, gold, gold_balance, detail) VALUES ";
    for (size_t i = 0; i < rows; ++i)
    {
        if (i > 0) sql += ",";
        sql += "(?,?,?,?,?,?,?,?)";
    }
    return sql;
}

void CDB_Manager::StartLogThread()
{
    if (m_logRunning.exchange(true)) return;   // 이미 돌고 있으면 무시
    m_logThread = std::thread(&CDB_Manager::LogThreadFunc, this);
    std::cout << "[LOG] 로그 전용 스레드 시작 (mmorpg_log)\n";
}

void CDB_Manager::StopLogThread()
{
    if (!m_logRunning.exchange(false)) return;
    m_logQueueCv.notify_all();                   // 대기 중이면 깨워서 종료 확인
    if (m_logThread.joinable()) m_logThread.join();   // join 안에서 큐 잔여분을 다 쓰고 끝난다
    std::cout << "[LOG] 로그 전용 스레드 종료 (기록 " << m_logWritten.load()
              << "건, 유실 " << m_logDropped.load() << "건)\n";
}

void CDB_Manager::EnqueueLog(const FGameLog& log)
{
    {
        std::lock_guard<std::mutex> lk(m_logQueueLock);

        // 로그가 꽉 차면 로그를 버리고 유실 카운터를 적는다
        if (m_logQueue.size() >= LOG_QUEUE_MAX)
        {
            m_logDropped.fetch_add(1);
            return;
        }
        m_logQueue.push(log);   // 복사본을 큐에 (호출한 IOCP 워커는 즉시 반환)
    }
    m_logQueueCv.notify_one();
}

void CDB_Manager::LogThreadFunc()
{
    using clock = std::chrono::steady_clock;

    // 이 스레드 전용 영속 커넥션 1개 (저장 스레드와 같은 이유로 재사용)
    std::unique_ptr<nanodbc::connection> conn;
    nanodbc::statement stmt;
    size_t preparedRows = 0;   // stmt 가 몇 행짜리로 준비돼 있나 (0 = 미준비)

    auto EnsureConn = [&]() -> bool
    {
        if (conn && conn->connected()) return true;
        try
        {
            conn = std::make_unique<nanodbc::connection>(m_logConnStr);
            preparedRows = 0;   // 커넥션이 바뀌면 준비된 문장도 무효
            std::cout << "[LOG] connected: mmorpg_log\n";
            return true;
        }
        catch (const std::exception& e)
        {
            std::cout << "[LOG thread] connect FAILED: " << e.what() << "\n";
            conn.reset();
            return false;
        }
    };

    std::vector<FGameLog> batch;
    batch.reserve(LOG_BATCH_MAX);

    // 배치 한 덩어리를 다중행 INSERT 한 방으로 기록. 실패하면 예외.
    auto WriteBatch = [&]() -> bool
    {
        if (!EnsureConn()) return false;

        const size_t rows = batch.size();
        if (preparedRows != rows)
        {
            // 행 수가 바뀔 때만 다시 준비한다. 부하가 걸리면 대부분 꽉 찬 배치라
            // 같은 문장이 계속 재사용된다.
            stmt.prepare(*conn, BuildLogInsertSql(rows));
            preparedRows = rows;
        }

        for (size_t i = 0; i < rows; ++i)
        {
            FGameLog&   g    = batch[i];
            const short base = static_cast<short>(i * LOG_COLS);

            // 바인딩은 값을 복사하지 않고 주소를 잡아둔다.
            // batch 는 execute 가 끝날 때까지 그대로 살아 있어야 한다(여기서는 그렇다).
            stmt.bind(base + 0, g.type);
            stmt.bind(base + 1, g.actor);

            // 거래 상대가 없는 로그(드롭 획득 등)는 빈 문자열이 아니라 NULL 로.
            // 빈 문자열로 넣으면 "상대가 없음"과 "상대 이름이 비어있음"이 구분되지 않는다.
            if (g.target[0] != '\0') stmt.bind(base + 2, g.target);
            else                     stmt.bind_null(base + 2);

            stmt.bind(base + 3, &g.itemCode);
            stmt.bind(base + 4, &g.quantity);
            stmt.bind(base + 5, &g.gold);
            stmt.bind(base + 6, &g.goldBalance);

            if (g.detail[0] != '\0') stmt.bind(base + 7, g.detail);
            else                     stmt.bind_null(base + 7);
        }

        nanodbc::result res = nanodbc::execute(stmt);   // 왕복 1번

        // DB 가 실제로 몇 행을 넣었는지 확인한다(추가 왕복 없이 응답에 같이 온다).
        // 예외 없이 끝났다고 다 들어간 게 아니라서, 이걸 안 보면
        // 기록 카운터가 사실과 다른 값을 말하게 된다. 감사 로그에선 그게 제일 나쁘다.
        // 여기서 예외를 던지지 않는 이유: 재시도하면 이미 들어간 행이 중복된다.
        const long affected = res.affected_rows();
        if (affected >= 0 && static_cast<size_t>(affected) != rows)
        {
            std::cout << "[LOG thread] 경고: " << rows << "건 보냈는데 "
                      << affected << "행 반영됨\n";
        }
        return true;
    };

    auto Flush = [&]()
    {
        if (batch.empty()) return;

        // 커넥션이 끊겼을 수 있으니 재연결 후 한 번 더 시도한다.
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            try
            {
                if (WriteBatch())
                {
                    m_logWritten.fetch_add(batch.size());
                    batch.clear();
                    return;
                }
            }
            catch (const std::exception& e)
            {
                // 로그 실패는 SLOG(부하 테스트에서 무음)가 아니라 항상 출력한다.
                // 기록이 유실됐다는 사실 자체가 절대 묻히면 안 되는 정보다.
                std::cout << "[LOG thread] insert error: " << e.what() << "\n";
            }
            conn.reset();
            preparedRows = 0;
        }

        // 두 번 다 실패 - 이 배치는 포기한다. 버린 건수는 반드시 남긴다.
        m_logDropped.fetch_add(batch.size());
        std::cout << "[LOG thread] " << batch.size() << "건 기록 실패 - 버림\n";
        batch.clear();
    };

    clock::time_point deadline;   // 지금 모으는 중인 배치를 늦어도 언제까지는 써야 하는가

    while (true)
    {
        bool stopping = false;
        {
            std::unique_lock<std::mutex> lk(m_logQueueLock);

            // 배치가 비었으면 새 로그가 올 때까지, 모으는 중이면 남은 마감까지만 기다린다.
            auto wait = std::chrono::milliseconds(LOG_FLUSH_MS);
            if (!batch.empty())
            {
                auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - clock::now());
                wait = (left.count() > 0) ? left : std::chrono::milliseconds(0);
            }

            if (m_logQueue.empty() && wait.count() > 0)
            {
                m_logQueueCv.wait_for(lk, wait,
                    [&] { return !m_logQueue.empty() || !m_logRunning.load(); });
            }

            while (!m_logQueue.empty() && batch.size() < LOG_BATCH_MAX)
            {
                if (batch.empty())
                    deadline = clock::now() + std::chrono::milliseconds(LOG_FLUSH_MS);
                batch.push_back(m_logQueue.front());
                m_logQueue.pop();
            }

            // 종료 요청 + 큐까지 비었으면 이번 배치를 쓰고 끝낸다.
            stopping = !m_logRunning.load() && m_logQueue.empty();
        }

        // 기록 조건: 100건이 찼거나 / 마감이 지났거나 / 종료 중이거나
        if (batch.size() >= LOG_BATCH_MAX || clock::now() >= deadline || stopping)
            Flush();

        if (stopping) break;
    }

    Flush();   // 방어: 위 루프에서 빠져나올 때 남은 게 있으면 마저 쓴다
}

// ================================================================
//  경매장 (DB 정본, write-through)
// ================================================================

// DB의 UTF-8 문자열 - CP949 바이트로
// ASCII(플레이어 계정명)는 두 인코딩에서 동일해 그대로 통과. 한글 "경매장"만 변환됨.
static void Utf8ToCp949(const std::string& utf8, char* out, size_t outCap)
{
    if (outCap == 0) return;
    out[0] = '\0';
    if (utf8.empty()) return;
    wchar_t wbuf[64] = {};
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wbuf, 64);
    if (wlen <= 0) return;
    WideCharToMultiByte(949, 0, wbuf, -1, out, static_cast<int>(outCap), nullptr, nullptr);
    out[outCap - 1] = '\0';
}

// 한 페이지 조회(최신 등록순). 다음 페이지 유무 판단 위해 pageSize+1 개를 읽는다.
int32_t CDB_Manager::Auction_GetPage(int32_t page, int32_t tab, const char* myName,
    const int32_t* searchCodes, int32_t searchCount,
    FAuctionEntry* out, bool& outHasNext)
{
    outHasNext = false;
    if (page < 0) page = 0;
    try
    {
        nanodbc::connection conn(m_connStr);
        int32_t limit  = AUCTION_PAGE_SIZE + 1;      // 한 개 더 읽어 다음 페이지 유무 판단
        int32_t offset = page * AUCTION_PAGE_SIZE;

        // 탭별 WHERE 절 구성. 내이름은 ? 로 바인딩, 검색코드(정수)는 안전하게 인라인.
        std::string where;
        if (tab == 1)   // 내판매: 내 매물 전부(완판=count 0 도 미수령골드 수령 위해 포함)
            where = "seller_name = ?";
        else            // 구매: 남의 매물 + 재고 있는 것
        {
            where = "seller_name <> ? AND count > 0";
            if (searchCount > 0 && searchCodes)
            {
                where += " AND item_code IN (";
                for (int32_t i = 0; i < searchCount && i < AUCTION_SEARCH_MAX; ++i)
                {
                    if (i) where += ",";
                    where += std::to_string(searchCodes[i]);
                }
                where += ")";
            }
        }

        std::string q =
            "SELECT listing_id, item_code, count, unit_price, pending_gold, seller_name "
            "FROM auction WHERE " + where + " ORDER BY listing_id DESC "
            "LIMIT " + std::to_string(limit) + " OFFSET " + std::to_string(offset);

        nanodbc::statement s(conn);
        nanodbc::prepare(s, q);
        s.bind(0, myName ? myName : "");
        nanodbc::result r = nanodbc::execute(s);
        int32_t n = 0;
        while (r.next())
        {
            if (n >= AUCTION_PAGE_SIZE) { outHasNext = true; break; }  // 초과분 = 다음 페이지 있음
            FAuctionEntry& e = out[n];
            e.listingID   = r.get<int>(0);
            e.itemCode    = r.get<int>(1);
            e.count       = r.get<int>(2);
            e.unitPrice   = r.get<int>(3);
            e.pendingGold = r.get<int>(4);
            Utf8ToCp949(r.get<std::string>(5, std::string()), e.sellerName, sizeof(e.sellerName));
            ++n;
        }
        return n;
    }
    catch (const std::exception& ex)
    {
        std::cout << "[DB] Auction_GetPage error: " << ex.what() << "\n";
        return 0;
    }
}

bool CDB_Manager::Auction_Register(const char* seller, int32_t itemCode, int32_t count, int32_t unitPrice)
{
    if (!seller || itemCode <= 0 || count <= 0 || unitPrice <= 0) return false;
    try
    {
        nanodbc::connection conn(m_connStr);
        nanodbc::statement s(conn);
        nanodbc::prepare(s, NANODBC_TEXT(
            "INSERT INTO auction (item_code, count, unit_price, pending_gold, seller_name) "
            "VALUES (?,?,?,0,?)"));
        int code = itemCode, cnt = count, price = unitPrice;
        s.bind(0, &code);
        s.bind(1, &cnt);
        s.bind(2, &price);
        s.bind(3, seller);
        nanodbc::execute(s);
        return true;
    }
    catch (const std::exception& ex)
    {
        std::cout << "[DB] Auction_Register error: " << ex.what() << "\n";
        return false;
    }
}

bool CDB_Manager::Auction_PeekBuy(int32_t listingID, const char* buyer,
    int32_t& outCode, int32_t& outUnitPrice, std::string* outSeller)
{
    try
    {
        nanodbc::connection conn(m_connStr);
        nanodbc::statement s(conn);
        nanodbc::prepare(s, NANODBC_TEXT(
            "SELECT item_code, unit_price, seller_name FROM auction WHERE listing_id=?"));
        int id = listingID;
        s.bind(0, &id);
        nanodbc::result r = nanodbc::execute(s);
        if (!r.next()) return false;                       // 매물 없음
        outCode      = r.get<int>(0);
        outUnitPrice = r.get<int>(1);
        std::string seller = r.get<std::string>(2, std::string());
        if (buyer && seller == buyer) return false;        // 본인 매물 구매 불가
        if (outSeller) *outSeller = seller;                // AUCTION_SOLD 로그용
        return true;
    }
    catch (const std::exception& ex)
    {
        std::cout << "[DB] Auction_PeekBuy error: " << ex.what() << "\n";
        return false;
    }
}

bool CDB_Manager::Auction_CommitBuy(int32_t listingID, int32_t qty, int32_t total, const char* buyer)
{
    if (qty <= 0) return false;
    try
    {
        nanodbc::connection conn(m_connStr);
        nanodbc::statement s(conn);
        // 원자적 조건부 UPDATE: count>=qty 이고 본인 아닌 경우에만 감소.
        // 동시 구매 시 DB가 이 행을 잠가 직렬화 - 한 명만 1행 반영, 나머지는 0행(거부).
        nanodbc::prepare(s, NANODBC_TEXT(
            "UPDATE auction SET count = count - ?, pending_gold = pending_gold + ? "
            "WHERE listing_id = ? AND count >= ? AND seller_name <> ?"));
        int q1 = qty, tot = total, id = listingID, q2 = qty;
        s.bind(0, &q1);
        s.bind(1, &tot);
        s.bind(2, &id);
        s.bind(3, &q2);
        s.bind(4, buyer);
        nanodbc::result r = nanodbc::execute(s);
        return r.affected_rows() >= 1;   // 1행=구매 성공, 0행=경쟁 탈락/수량부족/본인
    }
    catch (const std::exception& ex)
    {
        std::cout << "[DB] Auction_CommitBuy error: " << ex.what() << "\n";
        return false;
    }
}

bool CDB_Manager::Auction_Collect(int32_t listingID, const char* seller, int32_t& outGold)
{
    try
    {
        nanodbc::connection conn(m_connStr);
        nanodbc::transaction tx(conn);

        int32_t pending = 0, cnt = 0;
        {
            nanodbc::statement s(conn);
            nanodbc::prepare(s, NANODBC_TEXT(
                "SELECT pending_gold, count FROM auction WHERE listing_id=? AND seller_name=?"));
            int id = listingID;
            s.bind(0, &id);
            s.bind(1, seller);
            nanodbc::result r = nanodbc::execute(s);
            if (!r.next()) return false;      // 본인 매물 아님/없음 - 롤백
            pending = r.get<int>(0);
            cnt     = r.get<int>(1);
        }
        if (pending <= 0) return false;       // 수령할 골드 없음 - 롤백
        outGold = pending;

        {
            nanodbc::statement s(conn);
            if (cnt <= 0)   // 전량 판매 + 수령 완료 - 매물 제거
                nanodbc::prepare(s, NANODBC_TEXT("DELETE FROM auction WHERE listing_id=?"));
            else            // 남은 수량 있음 - 미수령골드만 0으로
                nanodbc::prepare(s, NANODBC_TEXT("UPDATE auction SET pending_gold=0 WHERE listing_id=?"));
            int id = listingID;
            s.bind(0, &id);
            nanodbc::execute(s);
        }
        tx.commit();
        return true;
    }
    catch (const std::exception& ex)
    {
        std::cout << "[DB] Auction_Collect error: " << ex.what() << "\n";
        return false;
    }
}

bool CDB_Manager::Auction_PeekCancel(int32_t listingID, const char* seller,
    int32_t& outCode, int32_t& outCount, int32_t& outGold)
{
    try
    {
        nanodbc::connection conn(m_connStr);
        nanodbc::statement s(conn);
        nanodbc::prepare(s, NANODBC_TEXT(
            "SELECT item_code, count, pending_gold FROM auction "
            "WHERE listing_id=? AND seller_name=?"));
        int id = listingID;
        s.bind(0, &id);
        s.bind(1, seller);
        nanodbc::result r = nanodbc::execute(s);
        if (!r.next()) return false;   // 본인 매물 아님/없음
        outCode  = r.get<int>(0);
        outCount = r.get<int>(1);
        outGold  = r.get<int>(2);
        return true;
    }
    catch (const std::exception& ex)
    {
        std::cout << "[DB] Auction_PeekCancel error: " << ex.what() << "\n";
        return false;
    }
}

bool CDB_Manager::Auction_Cancel(int32_t listingID, const char* seller,
    int32_t& outCode, int32_t& outCount, int32_t& outGold)
{
    try
    {
        nanodbc::connection conn(m_connStr);
        nanodbc::transaction tx(conn);

        {
            nanodbc::statement s(conn);
            nanodbc::prepare(s, NANODBC_TEXT(
                "SELECT item_code, count, pending_gold FROM auction "
                "WHERE listing_id=? AND seller_name=?"));
            int id = listingID;
            s.bind(0, &id);
            s.bind(1, seller);
            nanodbc::result r = nanodbc::execute(s);
            if (!r.next()) return false;   // 본인 매물 아님/없음 - 롤백
            outCode  = r.get<int>(0);
            outCount = r.get<int>(1);
            outGold  = r.get<int>(2);
        }
        {
            nanodbc::statement s(conn);
            nanodbc::prepare(s, NANODBC_TEXT("DELETE FROM auction WHERE listing_id=?"));
            int id = listingID;
            s.bind(0, &id);
            nanodbc::execute(s);
        }
        tx.commit();
        return true;
    }
    catch (const std::exception& ex)
    {
        std::cout << "[DB] Auction_Cancel error: " << ex.what() << "\n";
        return false;
    }
}
