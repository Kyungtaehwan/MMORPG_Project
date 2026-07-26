#include "pch.h"
#include "DB_Manager.h"
#include "nanodbc.h"
#include <cstring>
#include <iostream>
#include <memory>

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
//주어진 커넥션으로 저장 트랜잭션 1건 실행(공통 SQL).
//   동기 Save호출마다 새 커넥션)와 전용 스레드가 공유한다.
//   - 실패 시 예외를 던진다
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
//   - 서버 시작 시 1회 불러서, 여기서 실패하면 DB 문제를 즉시 알 수 있다.
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
//   결과셋2 inventory  : 아이템 행들 - out.inven[]
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
            int idx = 0;
            while (r.next() && idx < 15)   // inven[16] 중 마지막 칸은 종료표식용
            {
                out.inven[idx].code  = r.get<int>(1);   // item_code
                out.inven[idx].count = r.get<int>(2);   // count
                ++idx;
            }
            out.inven[idx].code  = 0;   // code==0 - 목록 끝
            out.inven[idx].count = 0;
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
//  경매장 (DB 정본, write-through)
// ================================================================

// DB의 UTF-8 문자열 - CP949 바이트로 (클라가 sellerName을 CP_ACP=949로 디코드하므로).
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
    int32_t& outCode, int32_t& outUnitPrice)
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
