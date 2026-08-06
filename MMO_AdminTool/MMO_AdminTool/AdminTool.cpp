// ================================================================
//  MMORPG 운영 도구 (Admin Tool)
//
//  게임에서 재화(골드/아이템)가 이상하게 늘어났을 때
//    1) 누가 이상한지 찾고        (탐지)
//    2) 어디까지 퍼졌는지 보고    (범위)
//    3) 되돌리는                  (조치)
//  일을 콘솔 메뉴로 할 수 있게 만든 도구다.
//
//  원래는 db/ 폴더의 SQL 파일과 PowerShell 스크립트로 하던 일이다.
//  이 프로그램은 그것들을 C++ 로 옮긴 것이고,
//  대체가 끝난 db/backup.ps1 과 db/pitr.ps1 은 2026-08-04 에 삭제했다.
//
//  ★ 이 프로그램은 PowerShell 을 거치지 않는다
//    메뉴 6/7 도 .ps1 을 부르는 게 아니라, MySQL 이 설치하며 같이 깔아둔
//    mysqldump.exe / mysqlbinlog.exe / mysql.exe 를 직접 실행한다(AdminOps.cpp).
//    조사 쿼리가 든 db/*.sql 은 이 도구에 없는 쿼리가 남아 있어 그대로 둔다.
//
//  ★ 왜 계정이 두 개인가
//    조회는 mmo_analyst(읽기 전용)로 한다. 조사하는 동안에는 쿼리를
//    아주 많이 돌리게 되는데, 그때 실수로 데이터를 고치는 걸 권한으로 막는다.
//    실제로 되돌리거나 복구할 때만 root 비밀번호를 물어본다.
//
//  ★ 인코딩은 UTF-8 하나로 통일돼 있다
//    프로젝트에 /utf-8 옵션이 걸려 있어 소스의 한글이 UTF-8 그대로 컴파일되고,
//    콘솔도 main() 에서 UTF-8 로 맞추고, DB 연결도 utf8mb4 다.
//    셋 중 하나만 어긋나도 화면이 통째로 깨지므로 함부로 바꾸지 말 것.
//    (SQL 별칭이 전부 영문인 것은 인코딩 때문이 아니라 열 이름을 짧게 두려는 것이다.)
// ================================================================

#include "AdminDB.h"
#include "AdminOps.h"

#include <windows.h>
#include <conio.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

// ================================================================
//  입력 다듬기
//
//  사용자가 친 글자를 그대로 SQL 에 붙이면 SQL 인젝션이 된다.
//  예를 들어 계정 이름 칸에  ' OR 1=1 --  를 넣으면 조건이 무력화된다.
//  여기서는 허용할 글자만 남기는 방식으로 막는다(화이트리스트).
// ================================================================

static std::string SanitizeAccount(const std::string& in)
{
    std::string out;
    for (char c : in)
    {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_')
            out.push_back(c);
    }
    return out;
}

static std::string SanitizeDateTime(const std::string& in)
{
    std::string out;
    for (char c : in)
    {
        if ((c >= '0' && c <= '9') || c == '-' || c == ':' || c == ' ')
            out.push_back(c);
    }
    return out;
}

static std::string Prompt(const char* text)
{
    std::cout << text << std::flush;   // 프롬프트가 먼저 보여야 하므로 여기서 흘려보낸다
    return ReadLineUtf8();
}

// ================================================================
//  1. 부정 재화 탐지 - 집계 이상치
//
//  "남들보다 비정상적으로 많이 번 사람" 을 찾는다.
//
//  ★ 왜 평균이 아니라 중앙값과 비교하나
//    악용자 한 명이 평균을 통째로 끌어올린다. 평균과 비교하면
//    정작 그 악용자가 "평균의 몇 배" 인지가 작아 보여서 묻힐 수 있다.
//    중앙값(줄 세웠을 때 한가운데 값)은 극단값에 흔들리지 않아
//    기준선으로 쓰기에 맞다.
//
//  ★ 왜 유저별이 아니라 유저 x 날짜별인가
//    오래 한 사람과 짧고 굵게 뽑아낸 사람을 구분하기 위해서다.
//    "하루에 얼마나 벌었나" 가 비교 가능한 단위다.
//
//  MySQL 에는 중앙값 함수가 없어서 직접 구한다.
//    ROW_NUMBER() 로 번호를 매기고, 가운데 번호의 값을 가져온다.
//    개수가 짝수면 가운데 둘의 평균을 쓴다.
// ================================================================
static const char* SQL_DETECT_OUTLIER = R"SQL(
WITH user_day AS (
    SELECT actor,
           DATE(created_at) AS day,
           COUNT(*)         AS drops,
           SUM(gold)        AS gold_gained
      FROM game_log
     WHERE log_type = 'DROP_GAIN' AND item_code = 9000
     GROUP BY actor, DATE(created_at)
),
ranked AS (
    SELECT gold_gained,
           ROW_NUMBER() OVER (ORDER BY gold_gained) AS rn,
           COUNT(*)     OVER ()                     AS total
      FROM user_day
),
baseline AS (
    SELECT AVG(gold_gained) AS median_gold
      FROM ranked
     WHERE rn IN (FLOOR((total + 1) / 2), CEILING((total + 1) / 2))
)
SELECT u.actor,
       u.day,
       u.drops,
       u.gold_gained,
       ROUND(b.median_gold)                    AS median_all,
       ROUND(u.gold_gained / b.median_gold, 1) AS times_median,
       IF(EXISTS(SELECT 1 FROM game_log r
                  WHERE r.actor = u.actor
                    AND r.log_type = 'ADMIN_ROLLBACK'
                    AND r.created_at > u.day),
          'HANDLED', 'OPEN')                   AS status
  FROM user_day u
 CROSS JOIN baseline b
 WHERE u.gold_gained > b.median_gold * 10
 ORDER BY u.gold_gained DESC
)SQL";

// ================================================================
//  2. 장부 검사 - 연속성 위반
//
//  로그에는 증감(gold)과 그 직후 잔액(gold_balance)이 같이 있다.
//  재화가 움직이는 모든 경로에 로그가 있다면 항상
//      직전 잔액 + 이번 증감 = 이번 잔액
//  이 성립해야 한다. 어긋난다면 로그를 안 거치고 골드가 바뀐 것이다.
//
//  LAG(gold_balance) OVER (PARTITION BY actor ORDER BY ...) 가
//  "같은 유저의 바로 앞 기록의 잔액" 을 가져온다.
//  PARTITION BY 가 없으면 다른 유저의 기록과 비교하게 되어 전부 어긋난다.
//
//  AUCTION_SOLD 를 빼는 이유:
//    판매 성사 로그는 판매자가 접속 중이 아닐 수 있어 잔액을 0 으로 남긴다.
//    잔액 검사의 대상이 아니다. 이 필터를 빼면 정상 거래가 전부 위반으로
//    잡혀서 진짜 사고가 묻힌다.
// ================================================================
static const char* SQL_DETECT_CONTINUITY = R"SQL(
SELECT log_id,
       actor,
       log_type,
       created_at,
       prev_balance,
       gold,
       gold_balance,
       (gold_balance - (prev_balance + gold)) AS unexplained
  FROM (
    SELECT log_id, actor, log_type, gold, gold_balance, created_at,
           LAG(gold_balance) OVER (PARTITION BY actor
                                   ORDER BY created_at, log_id) AS prev_balance
      FROM game_log
     WHERE log_type <> 'AUCTION_SOLD'
  ) t
 WHERE prev_balance IS NOT NULL
   AND prev_balance + gold <> gold_balance
 ORDER BY ABS(gold_balance - (prev_balance + gold)) DESC
)SQL";

// ================================================================
//  메뉴 동작
// ================================================================

static void RunDetectOutlier(CAdminDB& db)
{
    PrintTitle("1. 부정 재화 탐지 - 집계 이상치");
    std::cout << "  하루에 번 골드가 전체 중앙값의 10배를 넘는 경우를 찾는다.\n";
    std::cout << "  평균이 아니라 중앙값을 쓰는 이유는, 악용자가 평균을 끌어올려\n";
    std::cout << "  자기 자신을 덜 튀어 보이게 만들기 때문이다.\n\n";

    FTable t;
    std::string err;
    if (!db.Query(SQL_DETECT_OUTLIER, t, err))
    {
        std::cout << "  질의 실패: " << err << "\n";
        return;
    }

    PrintTable(t);

    if (t.empty())
        std::cout << "\n  이상치가 없다. 정상 범위 안에서만 움직이고 있다.\n";
    else
    {
        std::cout << "\n  status 열 설명\n";
        std::cout << "    OPEN    : 아직 조치하지 않음\n";
        std::cout << "    HANDLED : 이미 되돌린 적이 있음\n";
        std::cout << "  로그는 지워지지 않으므로(append-only) 조치한 사건도 계속 잡힌다.\n";
        std::cout << "  그래서 지우는 대신 처리 여부를 표시한다.\n";
    }
}

static void RunDetectContinuity(CAdminDB& db)
{
    PrintTitle("2. 장부 검사 - 연속성 위반");
    std::cout << "  '직전 잔액 + 이번 증감 = 이번 잔액' 이 안 맞는 기록을 찾는다.\n";
    std::cout << "  어긋난 금액이 곧 '로그를 안 거치고 생겨난 금액' 이다.\n\n";

    FTable t;
    std::string err;
    if (!db.Query(SQL_DETECT_CONTINUITY, t, err))
    {
        std::cout << "  질의 실패: " << err << "\n";
        return;
    }

    PrintTable(t);

    if (t.empty())
    {
        std::cout << "\n  위반 0 건. 장부가 깨끗하다.\n";
        std::cout << "  로그를 믿을 수 있다는 뜻이고, 사고가 나도 선별 환수가 가능하다.\n";
    }
    else
    {
        std::cout << "\n  unexplained 열이 설명되지 않는 금액이다.\n";
        std::cout << "  복제 버그이거나, 로그를 안 거치고 골드를 바꾸는 코드가 있다는 뜻이다.\n";
        std::cout << "  되돌리기 전에 원인부터 찾아야 한다.\n";
    }
}

static void RunAccountDetail(CAdminDB& db)
{
    PrintTitle("3. 계정 조회");

    std::string acc = SanitizeAccount(Prompt("  계정 이름 (예: test1): "));
    if (acc.empty()) { std::cout << "  이름이 비었다.\n"; return; }

    std::string err;

    // 지금 상태 (게임 DB)
    {
        std::ostringstream sql;
        sql << "SELECT c.account_id, c.gold, c.level, c.exp, "
               "(SELECT COUNT(*) FROM mmorpg.inventory i WHERE i.account_id=c.account_id) AS inven_rows "
               "FROM mmorpg.`character` c WHERE c.account_id='" << acc << "'";

        FTable t;
        if (db.Query(sql.str(), t, err))
        {
            std::cout << "\n  [지금 상태 - 게임 DB]\n";
            PrintTable(t);
        }
        else std::cout << "  질의 실패: " << err << "\n";
    }

    // 최근 거래 (로그 DB)
    {
        std::ostringstream sql;
        sql << "SELECT log_id, created_at, log_type, item_code, quantity, gold, gold_balance, "
               "IFNULL(target,'-') AS target, IFNULL(detail,'-') AS detail "
               "FROM game_log WHERE actor='" << acc << "' "
               "ORDER BY log_id DESC LIMIT 30";

        FTable t;
        if (db.Query(sql.str(), t, err))
        {
            std::cout << "\n  [최근 거래 30건 - 로그 DB]\n";
            PrintTable(t);
        }
        else std::cout << "  질의 실패: " << err << "\n";
    }

    // 게임 DB 와 로그가 맞는지
    //  둘이 어긋나 있으면 되돌리기 도구가 엉뚱한 값을 낸다.
    {
        std::ostringstream sql;
        sql << "SELECT c.gold AS db_gold, "
               "(SELECT g.gold_balance FROM game_log g "
               " WHERE g.actor=c.account_id AND g.log_type<>'AUCTION_SOLD' "
               " ORDER BY g.created_at DESC, g.log_id DESC LIMIT 1) AS last_log_balance "
               "FROM mmorpg.`character` c WHERE c.account_id='" << acc << "'";

        FTable t;
        if (db.Query(sql.str(), t, err))
        {
            std::cout << "\n  [정합성 - 두 값이 같아야 정상]\n";
            PrintTable(t);
            std::cout << "  다르면 로그를 안 거치고 골드를 바꾼 일이 있었다는 뜻이다.\n";
        }
    }
}

static void RunSpread(CAdminDB& db)
{
    PrintTitle("4. 전파 범위 - 이 재화가 남에게 넘어갔나");
    std::cout << "  이 결과가 조치를 정한다.\n";
    std::cout << "    0 건        -> 혼자 갖고 있다 -> 그 계정만 되돌리면 끝\n";
    std::cout << "    수십~수백 건 -> 이미 퍼졌다   -> 선별 불가 -> 전면 복구 검토\n\n";

    std::string acc = SanitizeAccount(Prompt("  계정 이름: "));
    if (acc.empty()) { std::cout << "  이름이 비었다.\n"; return; }

    std::string err;

    {
        std::ostringstream sql;
        sql << "SELECT log_type, target AS counterparty, COUNT(*) AS trades, "
               "SUM(quantity) AS items, SUM(gold) AS gold "
               "FROM game_log WHERE actor='" << acc << "' AND target IS NOT NULL "
               "GROUP BY log_type, target ORDER BY trades DESC LIMIT 30";

        FTable t;
        if (db.Query(sql.str(), t, err))
        {
            std::cout << "\n  [이 계정이 남에게 넘긴 것]\n";
            PrintTable(t);
        }
        else std::cout << "  질의 실패: " << err << "\n";
    }

    {
        std::ostringstream sql;
        sql << "SELECT COUNT(DISTINCT actor) AS counterparties "
               "FROM game_log WHERE target='" << acc << "'";

        FTable t;
        if (db.Query(sql.str(), t, err))
        {
            std::cout << "\n  [이 계정과 거래한 상대 수]\n";
            PrintTable(t);
        }
    }

    {
        std::ostringstream sql;
        sql << "SELECT listing_id, item_code, count, unit_price, created_at "
               "FROM mmorpg.auction WHERE seller_name='" << acc << "'";

        FTable t;
        if (db.Query(sql.str(), t, err))
        {
            std::cout << "\n  [경매장에 아직 남아 있는 매물 - 사가면 퍼진다]\n";
            PrintTable(t);
        }
    }
}

static void RunRollback(CAdminDB& db)
{
    PrintTitle("5. 계정 되돌리기");
    std::cout << "  지정한 시각의 상태로 계정 하나를 되돌린다.\n";
    std::cout << "  방식: 지금 상태에서 그 시점 이후 로그를 거꾸로 되감는다.\n";
    std::cout << "        (되돌릴 값 = 지금 값 - 그 시점 이후 증감 합계)\n";
    std::cout << "  백업 파일이 필요 없다.\n\n";

    if (!db.HasRoot())
    {
        std::cout << "  되돌리기는 데이터를 바꾸므로 root 권한이 필요하다.\n";
        std::string pw = ReadPassword("  root 비밀번호: ");
        if (!db.ConnectAsRoot(pw)) return;
    }

    std::string acc = SanitizeAccount(Prompt("  계정 이름: "));
    if (acc.empty()) { std::cout << "  이름이 비었다.\n"; return; }

    std::string when = SanitizeDateTime(Prompt("  되돌릴 시각 (예: 2026-08-03 20:00:59): "));
    if (when.size() < 10) { std::cout << "  시각 형식이 잘못됐다.\n"; return; }

    // 먼저 미리보기. 되돌리기는 되돌릴 수 없으므로 항상 확인부터 한다.
    auto call = [&](int apply)
    {
        std::ostringstream sql;
        sql << "CALL mmorpg.sp_rollback_account('" << acc << "','" << when << "'," << apply << ")";
        return sql.str();
    };

    std::vector<FTable> tables;
    std::string err;

    std::cout << "\n  [미리보기 - 아무것도 바꾸지 않는다]\n";
    if (!db.QueryMulti(call(0), tables, err, true))
    {
        std::cout << "  실패: " << err << "\n";
        return;
    }

    const char* names[] = { "요약", "아이템 변화", "함께 내려갈 경매 매물" };
    for (size_t i = 0; i < tables.size(); ++i)
    {
        std::cout << "\n  [" << (i < 3 ? names[i] : "결과") << "]\n";
        PrintTable(tables[i]);
    }

    std::cout << "\n  gold_check 열이 'OK - two methods agree' 여야 안전하다.\n";
    std::cout << "  두 가지 방법으로 계산한 값이 서로 다르면, 로그를 안 거치고\n";
    std::cout << "  골드를 바꾸는 코드가 있다는 뜻이라 되돌리면 안 된다.\n";

    std::string yes = Prompt("\n  실제로 적용하려면 APPLY 라고 입력: ");
    if (yes != "APPLY")
    {
        std::cout << "  취소했다. DB 는 그대로다.\n";
        return;
    }

    tables.clear();
    if (!db.QueryMulti(call(1), tables, err, true))
    {
        std::cout << "  실패: " << err << "\n";
        return;
    }

    std::cout << "\n  [적용 결과]\n";
    if (!tables.empty()) PrintTable(tables[0]);

    std::cout << "\n  되돌린 사실도 ADMIN_ROLLBACK 으로 로그에 남았다.\n";
    std::cout << "  안 남기면 다음 장부 검사에서 이 조치가 복제 버그로 오해받는다.\n";
}

static void RunBackup(const std::string& dbDir)
{
    PrintTitle("6. DB 백업");
    std::cout << "  게임 DB(mmorpg)와 로그 DB(mmorpg_log)를 각각 다른 파일로 뽑는다.\n";
    std::cout << "  복구할 때 게임 DB 만 되돌리고 로그는 그대로 둬야 하기 때문이다.\n";
    std::cout << "  한 파일에 같이 있으면 실수로 로그까지 되돌리게 된다.\n\n";

    if (dbDir.empty())
    {
        std::cout << "  db 폴더를 찾지 못했다. 저장소 안에서 실행할 것.\n";
        return;
    }

    std::string pw = ReadPassword("  root 비밀번호: ");
    std::cout << "\n";
    DoBackup(dbDir, pw);
}

static void RunPitr(const std::string& dbDir)
{
    PrintTitle("7. 시점 복구 (PITR)");
    std::cout << "  백업만으로는 '마지막 백업 시점' 으로밖에 못 돌아간다.\n";
    std::cout << "  백업을 깔고 binlog(MySQL 이 자동으로 적는 변경 일지)를\n";
    std::cout << "  원하는 시각까지만 재생하면 그 사이 아무 시점으로나 갈 수 있다.\n\n";
    std::cout << "  계정 하나가 아니라 DB 전체가 되돌아간다는 점에 주의할 것.\n\n";

    if (dbDir.empty())
    {
        std::cout << "  db 폴더를 찾지 못했다. 저장소 안에서 실행할 것.\n";
        return;
    }

    if (!IsRunningAsAdmin())
    {
        std::cout << "  [안내] 지금은 관리자 권한이 아니다.\n";
        std::cout << "         binlog 폴더가 MySQL 서비스 계정 소유라 읽을 수 없다.\n";
        std::cout << "         이 프로그램을 관리자 권한으로 다시 실행해야 한다.\n\n";
    }

    std::string when = SanitizeDateTime(
        Prompt("  되돌릴 시각 (예: 2026-08-04 14:51:53): "));
    if (when.size() < 10) { std::cout << "  시각 형식이 잘못됐다.\n"; return; }

    std::string pw = ReadPassword("  root 비밀번호: ");
    std::cout << "\n";

    // 먼저 미리보기
    if (!DoPitr(dbDir, pw, when, false))
        return;

    std::string yes = Prompt("\n  실제로 적용하려면 APPLY 라고 입력: ");
    if (yes != "APPLY")
    {
        std::cout << "  취소했다. DB 는 그대로다.\n";
        return;
    }

    DoPitr(dbDir, pw, when, true);
}

// ================================================================
//  main
// ================================================================

int main()
{
    // 콘솔을 UTF-8 로 맞춘다.
    //
    //  ★ 왜 CP949 가 아니라 UTF-8 인가 (2026-08-04 에 겪은 것)
    //    처음에는 "한국어 윈도우니까 CP949" 로 맞췄는데 글자가 전부 깨졌다.
    //    요즘 Windows Terminal 은 65001(UTF-8)로 뜨고, 프로그램이 도중에
    //    코드페이지를 949 로 바꿔도 터미널 쪽이 따라오지 않기 때문이다.
    //    출력을 파일이나 다른 프로그램으로 넘길 때도 UTF-8 이라야 안 깨진다.
    //    그래서 리터럴(/utf-8) - 콘솔 - DB(utf8mb4) 를 전부 UTF-8 로 통일했다.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << "==================================================================\n";
    std::cout << "  MMORPG 운영 도구 (Admin Tool)\n";
    std::cout << "  재화 이상 탐지 / 계정 되돌리기 / 백업 / 시점 복구\n";
    std::cout << "==================================================================\n\n";

    CAdminDB db;
    if (!db.ConnectAsAnalyst())
    {
        std::cout << "\n아무 키나 누르면 종료.\n";
        (void)_getwch();
        return 1;
    }

    std::string dbDir = FindDbDir();
    std::cout << "[경로] db 폴더: " << (dbDir.empty() ? "(못 찾음)" : dbDir) << "\n";
    std::cout << "[권한] 관리자: " << (IsRunningAsAdmin() ? "예" : "아니오")
              << "   게임서버 실행중: " << (IsGameServerRunning() ? "예" : "아니오") << "\n";

    for (;;)
    {
        std::cout << "\n";
        std::cout << "------------------------------------------------------------------\n";
        std::cout << "  [1] 부정 재화 탐지   집계 이상치로 악용자 찾기\n";
        std::cout << "  [2] 장부 검사        연속성 위반으로 복제 버그 찾기\n";
        std::cout << "  [3] 계정 조회        한 계정의 상태와 거래 내역\n";
        std::cout << "  [4] 전파 범위        재화가 남에게 넘어갔는지 확인\n";
        std::cout << "  [5] 계정 되돌리기    지정 시각으로 복구 (root 필요)\n";
        std::cout << "  [6] DB 백업          (root 필요)\n";
        std::cout << "  [7] 시점 복구 PITR   DB 전체 (root + 관리자 권한 필요)\n";
        std::cout << "  [0] 종료\n";
        std::cout << "------------------------------------------------------------------\n";

        std::string sel = Prompt("  선택: ");
        if (sel.empty()) continue;

        switch (sel[0])
        {
        case '1': RunDetectOutlier(db);    break;
        case '2': RunDetectContinuity(db); break;
        case '3': RunAccountDetail(db);    break;
        case '4': RunSpread(db);           break;
        case '5': RunRollback(db);         break;
        case '6': RunBackup(dbDir);        break;
        case '7': RunPitr(dbDir);          break;
        case '0':
            std::cout << "  종료.\n";
            return 0;
        default:
            std::cout << "  0 ~ 7 중에서 고를 것.\n";
            break;
        }
    }
}
