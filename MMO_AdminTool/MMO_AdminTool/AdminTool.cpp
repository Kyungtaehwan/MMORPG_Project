// ================================================================
//  MMORPG 운영 도구 (Admin Tool)
//
//  게임에서 재화(골드/아이템)가 이상하게 늘어났을 때
//    1) 누가 이상한지 찾고        (탐지)
//    2) 어디까지 퍼졌는지 보고    (범위)
//    3) 되돌리는                  (조치)
//  일을 콘솔 메뉴로 할 수 있게 만든 도구다.
// 
//    조회는 mmo_analyst(읽기 전용)로 한다
//    실제로 되돌리거나 복구할 때만 root 비밀번호를 물어본다.
//
// 인코딩은 UTF-8 하나로 통일
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
//   중앙값으로 유저 탐지.
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
        std::cout << "    OPEN    : 아직 조치하지 않음\n";
        std::cout << "    HANDLED : 이미 되돌린 적이 있음\n";
    }
}

static void RunDetectContinuity(CAdminDB& db)
{
    PrintTitle("2. DB 검사 - 연속성 위반");
    std::cout << "  직전 잔액 + 이번 증감 = 이번 잔액 이 어긋난 기록을 찾는다\n";

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
        std::cout << "\n  위반 0 건 -> 통과\n";
    }
    else
    {
        std::cout << "\n  unexplained : 설명되지 않는 금액\n";

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
        else std::cout << "  실패: " << err << "\n";
    }

    // 최근 거래 (로그 DB)
    {
        std::ostringstream sql;
        sql << "SELECT log_id, created_at, log_type, item_code, quantity, gold, gold_balance, "
               "IFNULL(target,'-') AS target, IFNULL(detail,'-') AS detail "
               "FROM game_log WHERE actor='" << acc << "' "
               "ORDER BY log_id DESC LIMIT 500";

        FTable t;
        if (db.Query(sql.str(), t, err))
        {
            std::cout << "\n  [최근 거래 500건 - 로그 DB]\n";
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

    std::string acc = SanitizeAccount(Prompt("  계정 이름: "));
    if (acc.empty()) { std::cout << "  이름이 비었다.\n"; return; }

    // 사고 시작 시각 이후만 봐야 한다.
    //  그 전의 거래는 정상 재화가 오간 것이라 전파가 아니다. 전 기간을 세면
    //  평소 거래가 많은 계정일수록 전파가 부풀려지고, 선별 환수로 끝날 사건을
    //  전면 복구로 오판하게 된다.
    //  시각은 메뉴 3 에서 첫 이상 로그의 created_at 을 보고 그 직전으로 잡는다.
    //  (메뉴 5 에 넣을 시각과 같은 값이다)
    std::string when = SanitizeDateTime(Prompt("  기준 시각 (비우면 전체 기간): "));

    std::string cond;
    if (when.size() >= 10)
    {
        cond = " AND created_at >= '" + when + "'";
        std::cout << "  기준: " << when << " 이후\n";
    }
    else
    {
        std::cout << "  기준: 전체 기간 (사고 이전 정상 거래도 함께 잡힌다)\n";
    }

    std::string err;

    {
        std::ostringstream sql;
        sql << "SELECT log_type, target AS counterparty, COUNT(*) AS trades, "
               "SUM(quantity) AS items, SUM(gold) AS gold "
               "FROM game_log WHERE actor='" << acc << "' AND target IS NOT NULL "
            << cond
            << " GROUP BY log_type, target ORDER BY trades DESC LIMIT 30";

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
               "FROM game_log WHERE target='" << acc << "'" << cond;

        FTable t;
        if (db.Query(sql.str(), t, err))
        {
            std::cout << "\n  [이 계정과 거래한 상대 수]\n";
            PrintTable(t);
        }
    }

    {
        std::ostringstream sql;
        // 아직 안 팔린 매물은 "미래의 전파" 다. 다만 사고 이전에 올린 매물은
        // 정상 재화이므로, 로그와 같은 기준 시각을 적용한다.
        // (sp_rollback_account 도 그 시점 이후 등록분만 함께 내린다)
        sql << "SELECT listing_id, item_code, count, unit_price, created_at "
               "FROM mmorpg.auction WHERE seller_name='" << acc << "'" << cond;

        FTable t;
        if (db.Query(sql.str(), t, err))
        {
            std::cout << "\n  [경매장에 남아 있는 매물]\n";
            PrintTable(t);
        }
    }
}

static void RunRollback(CAdminDB& db)
{
    PrintTitle("5. 계정 되돌리기");
    std::cout << "  지정한 시각의 상태로 계정 하나를 되감는다.\n";

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

    std::cout << "\n  되돌린 사실도 ADMIN_ROLLBACK 으로 로그에 남았다\n";
}

// ================================================================
//  8. 부정 이득 몰수 (구간 지정)
//
//  5번(되돌리기)과 언제 갈리나
//    사고 뒤에 정상 플레이가 없다  -> 5번. 그 시점 상태로 정확히 복원된다.
//    사고 뒤에 정상 플레이가 있다  -> 8번. 5번을 쓰면 그것까지 날아간다.
//
//  5번은 아귀가 안 맞으면 멈추지만 8번은 멈추지 않는다.
//  이미 써버린 재화는 못 걷고, 못 걷은 만큼은 confiscate_pending 에 쌓인다.
// ================================================================
static void RunConfiscate(CAdminDB& db)
{
    PrintTitle("8. 부정 이득 몰수 - 구간에 번 것만 걷어낸다");
    std::cout << "  이후 정상 플레이는 건드리지 않는다.\n";

    if (!db.HasRoot())
    {
        std::cout << "  몰수는 데이터를 바꾸므로 root 권한이 필요하다.\n";
        std::string pw = ReadPassword("  root 비밀번호: ");
        if (!db.ConnectAsRoot(pw)) return;
    }

    std::string acc = SanitizeAccount(Prompt("  계정 이름: "));
    if (acc.empty()) { std::cout << "  이름이 비었다.\n"; return; }

    std::string from = SanitizeDateTime(Prompt("  시작 시각 (예: 2026-08-03 20:00:00): "));
    if (from.size() < 10) { std::cout << "  시각 형식이 잘못됐다.\n"; return; }

    // 끝을 비우면 그 시점부터 지금까지 전부가 대상이다.
    std::string to = SanitizeDateTime(Prompt("  끝 시각 (비우면 끝까지): "));
    std::string toArg = (to.size() >= 10) ? ("'" + to + "'") : std::string("NULL");

    auto call = [&](int apply)
    {
        std::ostringstream sql;
        sql << "CALL mmorpg.sp_confiscate_gains('" << acc << "','" << from << "',"
            << toArg << "," << apply << ")";
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

    const char* names[] = { "요약", "아이템별 회수 계획", "미회수 누적" };
    for (size_t i = 0; i < tables.size(); ++i)
    {
        std::cout << "\n  [" << (i < 3 ? names[i] : "결과") << "]\n";
        PrintTable(tables[i]);
    }

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
    for (size_t i = 0; i < tables.size(); ++i)
    {
        std::cout << "\n  [" << (i < 3 ? names[i] : "결과") << "]\n";
        PrintTable(tables[i]);
    }

    std::cout << "\n  몰수한 사실도 ADMIN_ROLLBACK 으로 로그에 남았다\n";
    std::cout << "  못 걷은 몫은 confiscate_pending 에 쌓였다 (아직 읽는 곳은 없다)\n";
}

// ================================================================
//  9. 백섭 후 장부 재연결 + 보상 지급
//    로그의 마지막 잔액 != 현재 DB 골드 인 계정이 곧 백섭이 건드린 계정이다.
//    계정 목록을 넘겨주지 않는다. 그래서 두 번 눌러도 중복 지급되지 않는다.
// ================================================================
static void RunCompensate(CAdminDB& db)
{
    PrintTitle("9. 백섭 후 장부 재연결 + 보상 지급");
    std::cout << "  PITR 은 로그를 남기지 않는다. 끊어진 장부를 잇고 보상을 지급한다.\n";

    if (!db.HasRoot())
    {
        std::cout << "  지급은 데이터를 바꾸므로 root 권한이 필요하다.\n";
        std::string pw = ReadPassword("  root 비밀번호: ");
        if (!db.ConnectAsRoot(pw)) return;
    }

    //  보상 산정 구간의 시작. 메뉴 7 에 넣었던 복구 시각과 같은 값이 아니라,
    //  로그상의 사건 시각이다(로그는 되돌리지 않았으므로 그대로 남아 있다).
    std::string from = SanitizeDateTime(Prompt("  보상 산정 시작 시각 (사건 시각): "));
    if (from.size() < 10) { std::cout << "  시각 형식이 잘못됐다.\n"; return; }

    //  최초 감염자는 보상 대상이 아니다. 장부 재연결은 해준다.
    std::string zero = SanitizeAccount(Prompt("  보상에서 제외할 계정 (최초 감염자, 없으면 엔터): "));
    std::string zeroArg = zero.empty() ? std::string("NULL") : ("'" + zero + "'");

    auto call = [&](int apply)
    {
        std::ostringstream sql;
        sql << "CALL mmorpg.sp_compensate_rollback('" << from << "'," << zeroArg << "," << apply << ")";
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

    const char* names[] = { "요약", "계정별 (상위 15)", "장부 검사" };
    for (size_t i = 0; i < tables.size(); ++i)
    {
        std::cout << "\n  [" << (i < 3 ? names[i] : "결과") << "]\n";
        PrintTable(tables[i]);
    }

    std::cout << "\n  accounts 가 0 이면 이미 정리된 상태다. 중복 지급되지 않는다.\n";
    std::cout << "  delta 는 백섭이 없앤 금액, comp_gold 는 돌려줄 사냥 소득이다.\n";

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
    for (size_t i = 0; i < tables.size(); ++i)
    {
        std::cout << "\n  [" << (i < 3 ? names[i] : "결과") << "]\n";
        PrintTable(tables[i]);
    }

    std::cout << "\n  newly_broken 이 0 이어야 한다.\n";
    std::cout << "  사건 기록 자체는 append-only 라 위반으로 계속 남는다 - 지우면 안 된다.\n";
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

    std::cout << "  백업을 깔고 binlog를 원하는 시각까지만 재생한다\n";

    if (dbDir.empty())
    {
        std::cout << "  db 폴더를 찾지 못했다. 저장소 안에서 실행할 것.\n";
        return;
    }

    if (!IsRunningAsAdmin())
    {
        std::cout << "  지금은 관리자 권한이 아니다.\n";
    }

    std::string when = SanitizeDateTime(
        Prompt("  되돌릴 시각 (예: 2026-08-04 14:51:53): "));
    if (when.size() < 10) { std::cout << "  시각 형식이 잘못됐다.\n"; return; }

    std::string pw = ReadPassword("  root 비밀번호: ");
    std::cout << "\n";

    // 먼저 미리보기
    if (!DoPitr(dbDir, pw, when, false))
        return;

    std::string yes = Prompt("\n  실적용하려면 APPLY 라고 입력: ");
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
    std::cout << "  MMORPG 운영 도구\n";
    std::cout << "==================================================================\n\n";

    CAdminDB db;
    if (!db.ConnectAsAnalyst())
    {
        std::cout << "\n아무 키나 누르면 종료.\n";
        (void)_getwch();
        return 1;
    }

    std::string dbDir = FindDbDir();
    std::cout << "\n" << "[권한] 관리자: " << (IsRunningAsAdmin() ? "예" : "아니오")
              << "   게임서버 실행중: " << (IsGameServerRunning() ? "예" : "아니오") << "\n";

    for (;;)
    {
        std::cout << "\n";
        std::cout << "------------------------------------------------------------------\n";
        std::cout << "  [1] 부정 재화 탐지   집계 이상치로 악용자 찾기\n";
        std::cout << "  [2] 로그 검사        연속성 위반으로 복제 버그 찾기\n";
        std::cout << "  [3] 계정 조회        한 계정의 상태와 거래 내역\n";
        std::cout << "  [4] 전파 범위        재화가 남에게 넘어갔는지 확인\n";
        std::cout << "  [5] 계정 되돌리기    지정 시각으로 복구 (root 필요)\n";
        std::cout << "  [6] DB 백업          (root 필요)\n";
        std::cout << "  [7] 시점 복구 PITR   DB 전체 (root + 관리자 권한 필요)\n";
        std::cout << "  [8] 부정 이득 몰수   구간에 번 것만 걷어냄 (root 필요)\n";
        std::cout << "  [9] 백섭 후 보상     장부 재연결 + 보상 지급 (root 필요)\n";
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
        case '8': RunConfiscate(db);       break;
        case '9': RunCompensate(db);       break;
        case '0':
            std::cout << "  종료.\n";
            return 0;
        default:
            std::cout << "  0 ~ 9 중에서 고를 것.\n";
            break;
        }
    }
}
