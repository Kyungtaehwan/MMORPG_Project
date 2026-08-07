#include "AdminOps.h"
#include "AdminDB.h"

#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <cstdlib>

namespace fs = std::filesystem;

// ================================================================
//  경로와 인코딩

//    이 프로그램의 std::string 은 전부 UTF-8
//    그런데 Windows 의 옛 API 들은 UTF-8 을 모르고 시스템 코드페이지(CP949)로 읽는다.
//      - std::system()          : 명령 문자열을 CP949 로 해석
//      - fs::path::string()     : 경로를 CP949 바이트로 내려준다
//      - fs::path(std::string)  : 준 바이트를 CP949 로 읽는다
//      - std::ifstream(문자열)  : 위와 같다
//
//    이 프로젝트 경로에는 "문서" 가 들어 있어서, 한 군데라도 섞이면
//    파일을 못 찾거나 명령이 통째로 실패한다.
//    그래서 경계에서는 반드시 UTF-16(wide) 판을 쓴다.
// ================================================================

// UTF-8 문자열 -> fs::path
static fs::path U8Path(const std::string& utf8)
{
    return fs::path(Utf8ToWide(utf8));
}

// fs::path -> UTF-8 문자열
static std::string PathU8(const fs::path& p)
{
    return WideToUtf8(p.wstring());
}

// ================================================================
//  경로 찾기
// ================================================================

std::string FindDbDir()
{
    // 실행 파일은 MMO_AdminTool/x64/Debug/ 같은 곳에 있고
    // db 폴더는 저장소 최상단에 있다. 위로 올라가며 찾는다.
    std::error_code ec;
    fs::path cur = fs::current_path(ec);
    if (ec) return std::string();

    for (int depth = 0; depth < 8; ++depth)
    {
        fs::path candidate = cur / "db" / "setup.sql";
        if (fs::exists(candidate, ec))
            return PathU8(cur / "db");

        if (!cur.has_parent_path()) break;
        fs::path parent = cur.parent_path();
        if (parent == cur) break;
        cur = parent;
    }
    return std::string();
}

std::string FindMySqlTool(const std::string& name)
{
    std::error_code ec;

    // MySQL 은 보통 여기에 설치된다. 버전 폴더 이름을 모르므로 훑는다.
    const char* roots[] = {
        "C:\\Program Files\\MySQL",
        "C:\\Program Files (x86)\\MySQL"
    };

    for (const char* root : roots)
    {
        if (!fs::exists(root, ec)) continue;

        for (const auto& entry : fs::directory_iterator(root, ec))
        {
            if (!entry.is_directory(ec)) continue;

            fs::path exe = entry.path() / "bin" / (name + ".exe");
            if (fs::exists(exe, ec))
                return PathU8(exe);
        }
    }
    return std::string();
}

// ================================================================
//  안전 확인
// ================================================================

bool IsRunningAsAdmin()
{
    BOOL bAdmin = FALSE;
    PSID pAdminGroup = nullptr;

    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2,
            SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &pAdminGroup))
    {
        CheckTokenMembership(nullptr, pAdminGroup, &bAdmin);
        FreeSid(pAdminGroup);
    }
    return bAdmin == TRUE;
}

bool IsGameServerRunning()
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    bool found = false;
    if (Process32FirstW(snap, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, L"MMO_GameServer.exe") == 0)
            {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

// ================================================================
//  외부 프로그램 실행
// ================================================================

// cmd.exe 는 명령 안에 따옴표가 여러 개 있으면 헷갈려한다.
// 그래서 명령 전체를 따옴표로 한 번 더 감싸주는 것이 관례다.
//
// std::system 이 아니라 wide 판인 _wsystem 을 쓴다.
// 명령에 백업 파일 경로가 들어가는데 그 경로에 한글("문서")이 있어서,
// narrow 판으로 보내면 CP949 로 해석되어 파일을 못 찾는다.
static int RunCommand(const std::string& cmd)
{
    std::wstring wrapped = L"\"" + Utf8ToWide(cmd) + L"\"";
    return _wsystem(wrapped.c_str());
}

// 비밀번호를 명령줄 인자로 주면 작업 관리자나 프로세스 목록에 그대로 노출된다.
// MySQL 도구들은 MYSQL_PWD 환경변수를 읽으므로 그쪽으로 넘긴다.
static void SetMysqlPassword(const std::string& pw)
{
    _wputenv_s(L"MYSQL_PWD", Utf8ToWide(pw).c_str());
}

static void ClearMysqlPassword()
{
    _wputenv_s(L"MYSQL_PWD", L"");
}

// 현재 시각을 20260804_145145 형태로
static std::string NowStamp()
{
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
    localtime_s(&tmv, &t);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tmv);
    return buf;
}

// ================================================================
//  백업 목록 읽기
// ================================================================

// 덤프 파일 앞부분에서 binlog 좌표를 뽑는다.
//  mysqldump --source-data=2 가 이런 주석을 남긴다.
//   -- CHANGE REPLICATION SOURCE TO SOURCE_LOG_FILE='xxx-bin.000009', SOURCE_LOG_POS=962826;
//  이 좌표가 "백업을 뜬 순간이 변경 일지의 어디인가" 를 알려준다.
//  시점 복구는 여기서부터 일지를 재생하므로 없으면 복구를 시작할 수 없다.
static void ExtractBinlogPos(const std::string& dumpPath,
                             std::string& outFile, std::string& outPos)
{
    outFile.clear();
    outPos.clear();

    std::ifstream in(U8Path(dumpPath));   // 한글 경로를 열려면 wide 경로여야 한다
    if (!in) return;

    std::string line;
    int guard = 0;
    while (std::getline(in, line) && guard++ < 300)
    {
        size_t f = line.find("SOURCE_LOG_FILE='");
        if (f == std::string::npos) continue;

        f += strlen("SOURCE_LOG_FILE='");
        size_t fEnd = line.find('\'', f);
        if (fEnd == std::string::npos) continue;
        outFile = line.substr(f, fEnd - f);

        size_t p = line.find("SOURCE_LOG_POS=");
        if (p == std::string::npos) continue;

        p += strlen("SOURCE_LOG_POS=");
        size_t pEnd = line.find_first_not_of("0123456789", p);
        outPos = (pEnd == std::string::npos) ? line.substr(p)
                                             : line.substr(p, pEnd - p);
        return;
    }
}

std::vector<FBackupSet> ListBackups(const std::string& dbDir)
{
    std::vector<FBackupSet> out;
    std::error_code ec;

    fs::path backupDir = U8Path(dbDir) / "backups";
    if (!fs::exists(backupDir, ec)) return out;

    for (const auto& entry : fs::directory_iterator(backupDir, ec))
    {
        if (!entry.is_regular_file(ec)) continue;

        std::string name = PathU8(entry.path().filename());

        // mmorpg_20260804_145145.sql 만 고른다.
        // mmorpg_log_... 는 로그 DB 덤프라 여기서는 짝으로만 다룬다.
        if (name.rfind("mmorpg_", 0) != 0) continue;
        if (name.rfind("mmorpg_log_", 0) == 0) continue;
        if (entry.path().extension() != ".sql") continue;

        // 이름에서 20260804_145145 부분만 떼어낸다.
        std::string stamp = name.substr(strlen("mmorpg_"));
        stamp = stamp.substr(0, stamp.size() - 4);   // ".sql" 제거
        if (stamp.size() != 15) continue;            // 8 + '_' + 6

        FBackupSet set;
        set.stamp    = stamp;
        set.gamePath = PathU8(entry.path());
        set.logPath  = PathU8(backupDir / ("mmorpg_log_" + stamp + ".sql"));

        // 20260804_145145 -> 20260804145145 (숫자로 만들어 비교하기 좋게)
        std::string digits = stamp.substr(0, 8) + stamp.substr(9);
        set.sortKey = std::atoll(digits.c_str());

        ExtractBinlogPos(set.gamePath, set.binlogFile, set.binlogPos);
        out.push_back(std::move(set));
    }

    // 최신이 앞으로
    std::sort(out.begin(), out.end(),
              [](const FBackupSet& a, const FBackupSet& b) { return a.sortKey > b.sortKey; });
    return out;
}

// ================================================================
//  백업
// ================================================================

bool DoBackup(const std::string& dbDir, const std::string& rootPassword)
{
    std::string mysqldump = FindMySqlTool("mysqldump");
    if (mysqldump.empty())
    {
        std::cout << "  mysqldump 를 찾을 수 없다. MySQL Server 설치를 확인할 것.\n";
        return false;
    }

    std::error_code ec;
    fs::path backupDir = U8Path(dbDir) / "backups";
    fs::create_directories(backupDir, ec);

    std::string stamp    = NowStamp();
    std::string gamePath = PathU8(backupDir / ("mmorpg_" + stamp + ".sql"));
    std::string logPath  = PathU8(backupDir / ("mmorpg_log_" + stamp + ".sql"));

    SetMysqlPassword(rootPassword);

    // 옵션 설명
    //  --single-transaction : DB 를 잠그지 않고 한 시점의 스냅샷을 뜬다.
    //                         서버를 켠 채로 백업해도 되는 이유가 이것.
    //  --source-data=2      : 백업 시점의 binlog 좌표를 주석으로 남긴다.
    //                         시점 복구에 반드시 필요하다.
    //  --routines           : 저장 프로시저도 같이 뜬다.
    //                         빼먹으면 복구한 DB 에 sp_login 이 없어 로그인이 안 된다.
    //  --result-file        : 결과를 이 파일에 쓴다. 화면 리다이렉션을 쓰면
    //                         인코딩이 바뀌어 덤프가 깨질 수 있다.
    std::ostringstream cmd;
    cmd << "\"" << mysqldump << "\""
        << " -u root"
        << " --single-transaction"
        << " --source-data=2"
        << " --routines"
        << " --triggers"
        << " --events"
        << " --hex-blob"
        << " --default-character-set=utf8mb4"
        << " --databases mmorpg"
        << " --result-file=\"" << gamePath << "\"";

    std::cout << "  [1/2] mmorpg 덤프 중...\n";
    if (RunCommand(cmd.str()) != 0)
    {
        std::cout << "  실패. 비밀번호나 권한을 확인할 것.\n";
        ClearMysqlPassword();
        return false;
    }

    // 로그 DB 는 별도 파일로 뜬다.
    //  복구할 때 게임 DB 만 되돌리고 로그는 손대지 않아야 하기 때문이다.
    //  한 파일에 같이 들어 있으면 실수로 로그까지 되돌리게 된다.
    std::ostringstream cmd2;
    cmd2 << "\"" << mysqldump << "\""
         << " -u root"
         << " --single-transaction"
         << " --default-character-set=utf8mb4"
         << " --databases mmorpg_log"
         << " --result-file=\"" << logPath << "\"";

    std::cout << "  [2/2] mmorpg_log 덤프 중...\n";
    if (RunCommand(cmd2.str()) != 0)
    {
        std::cout << "  로그 DB 덤프 실패.\n";
        ClearMysqlPassword();
        return false;
    }

    ClearMysqlPassword();

    std::string binFile, binPos;
    ExtractBinlogPos(gamePath, binFile, binPos);

    auto sizeKb = [&](const std::string& p) -> long long
    {
        std::error_code e2;
        auto sz = fs::file_size(U8Path(p), e2);
        return e2 ? 0 : static_cast<long long>(sz / 1024);
    };

    std::cout << "\n  백업 완료\n";
    std::cout << "    게임 DB : " << gamePath << "  (" << sizeKb(gamePath) << " KB)\n";
    std::cout << "    로그 DB : " << logPath  << "  (" << sizeKb(logPath)  << " KB)\n";
    std::cout << "    binlog  : " << (binFile.empty() ? "(못 찾음)" : binFile)
              << " : " << (binPos.empty() ? "-" : binPos) << "\n";
    std::cout << "\n  복구할 때는 반드시 게임 서버부터 끌 것.\n";
    return true;
}

// ================================================================
//  시점 복구 (PITR)
// ================================================================

bool DoPitr(const std::string& dbDir, const std::string& rootPassword,
            const std::string& stopDateTime, bool bApply)
{
    // ---- 안전 확인 ----
    if (!IsRunningAsAdmin())
    {
        std::cout << "  [중단] 관리자 권한이 아니다.\n";
        std::cout << "         binlog 파일이 있는 폴더가 MySQL 서비스 계정 소유라\n";
        std::cout << "         일반 권한으로는 읽을 수 없다.\n";
        std::cout << "         이 프로그램을 관리자 권한으로 다시 실행할 것.\n";
        return false;
    }

    if (IsGameServerRunning())
    {
        std::cout << "  [중단] 게임 서버가 실행 중이다.\n";
        std::cout << "         5초 주기 자동 저장이 메모리 상태로 DB 를 덮어쓰므로\n";
        std::cout << "         켜진 채로 복구하면 조용히 원복된다. 먼저 종료할 것.\n";
        return false;
    }

    std::string mysqlbinlog = FindMySqlTool("mysqlbinlog");
    std::string mysql       = FindMySqlTool("mysql");
    if (mysqlbinlog.empty() || mysql.empty())
    {
        std::cout << "  mysqlbinlog / mysql 을 찾을 수 없다.\n";
        return false;
    }

    // ---- 쓸 백업 고르기 ----
    //  되돌릴 시점보다 앞선 백업이어야 한다.
    //  뒤에 뜬 백업을 깔면 이미 사고가 포함돼 있다.
    std::vector<FBackupSet> sets = ListBackups(dbDir);
    if (sets.empty())
    {
        std::cout << "  백업이 없다. 먼저 백업을 뜰 것.\n";
        return false;
    }

    // "2026-08-04 14:51:53" -> 20260804145153
    std::string digits;
    for (char c : stopDateTime)
        if (c >= '0' && c <= '9') digits.push_back(c);

    if (digits.size() < 14)
    {
        std::cout << "  시각 형식이 잘못됐다. 예: 2026-08-04 14:51:53\n";
        return false;
    }
    long long stopKey = std::atoll(digits.substr(0, 14).c_str());

    const FBackupSet* chosen = nullptr;
    for (const auto& s : sets)          // 최신순이므로 처음 걸리는 게 가장 가까운 것
    {
        if (s.sortKey <= stopKey) { chosen = &s; break; }
    }

    if (!chosen)
    {
        std::cout << "  되돌릴 시점보다 앞선 백업이 없다.\n";
        std::cout << "  가장 오래된 백업: " << sets.back().stamp << "\n";
        return false;
    }

    if (chosen->binlogFile.empty())
    {
        std::cout << "  이 백업에 binlog 좌표가 없다.\n";
        std::cout << "  --source-data=2 로 뜬 백업인지 확인할 것.\n";
        return false;
    }

    std::cout << "  [백업]   " << chosen->stamp << "\n";
    std::cout << "  [시작점] " << chosen->binlogFile << " : " << chosen->binlogPos << "\n";

    SetMysqlPassword(rootPassword);

    // ---- binlog 위치 알아내기 ----
    //  파일이 실제로 어느 폴더에 있는지는 MySQL 에게 물어본다.
    fs::path replay = U8Path(dbDir) / "backups" / ("replay_" + chosen->stamp + ".sql");
    fs::path binDirFile = fs::temp_directory_path() / "mmo_binlog_dir.txt";

    {
        std::ostringstream q;
        q << "\"" << mysql << "\" -u root -N -B -e \"SELECT @@log_bin_basename\""
          << " > \"" << PathU8(binDirFile) << "\"";
        if (RunCommand(q.str()) != 0)
        {
            std::cout << "  binlog 경로를 물어보지 못했다.\n";
            ClearMysqlPassword();
            return false;
        }
    }

    std::string basename;
    {
        std::ifstream in(binDirFile);
        std::getline(in, basename);
    }
    std::error_code ec;
    fs::remove(binDirFile, ec);

    // 배치 모드 출력은 역슬래시를 두 개로 이스케이프해서 준다. 되돌려야 실제 경로가 된다.
    std::string fixed;
    for (size_t i = 0; i < basename.size(); ++i)
    {
        fixed.push_back(basename[i]);
        if (basename[i] == '\\' && i + 1 < basename.size() && basename[i + 1] == '\\')
            ++i;
    }
    while (!fixed.empty() && (fixed.back() == '\r' || fixed.back() == '\n')) fixed.pop_back();

    // MySQL 데이터 폴더 경로다. 기본 설치 위치는 전부 영문이라
    // UTF-8 로 읽어도 CP949 로 읽어도 바이트가 같다.
    fs::path binDir = U8Path(fixed).parent_path();
    fs::path binPath = binDir / chosen->binlogFile;

    if (!fs::exists(binPath, ec))
    {
        std::cout << "  binlog 파일을 읽을 수 없다: " << PathU8(binPath) << "\n";
        std::cout << "  관리자 권한으로 실행했는지 확인할 것.\n";
        ClearMysqlPassword();
        return false;
    }

    // ---- 재생할 SQL 뽑아내기 ----
    //
    //  ★ --database=mmorpg 가 여기서 가장 중요한 옵션이다.
    //
    //    binlog 에는 mmorpg 뿐 아니라 mmorpg_log 의 변경도 같이 들어 있다.
    //    필터 없이 재생하면 로그 INSERT 까지 다시 실행되는데,
    //    로그 DB 는 되돌리지 않았으므로 그 행들은 이미 있다.
    //    결과는 로그가 두 번씩 들어간 DB 이고, 보상 산정이 두 배로 어긋난다.
    //
    //  ★ --stop-datetime 은 그 시각 "직전" 까지만 재생한다.
    std::ostringstream gen;
    gen << "\"" << mysqlbinlog << "\""
        << " --database=mmorpg"
        << " --start-position=" << chosen->binlogPos
        << " --stop-datetime=\"" << stopDateTime << "\""
        << " --result-file=\"" << PathU8(replay) << "\""
        << " \"" << PathU8(binPath) << "\"";

    std::cout << "  [재생파일] " << PathU8(replay) << "\n";
    if (RunCommand(gen.str()) != 0)
    {
        std::cout << "  mysqlbinlog 실패.\n";
        ClearMysqlPassword();
        return false;
    }

    // ---- 무엇이 바뀌는지 요약 ----
    //  replay.sql 은 사람이 못 읽는다. binlog_format 이 ROW 라
    //  변경 내용이 base64 로 인코딩돼 있기 때문이다(BINLOG '...' 형태).
    //  실행에는 그 형태가 맞고, 보여주기 위해서는 풀어쓴 판을 따로 만든다.
    fs::path human = fs::temp_directory_path() / "mmo_replay_readable.txt";
    {
        std::ostringstream dec;
        dec << "\"" << mysqlbinlog << "\""
            << " --database=mmorpg"
            << " --start-position=" << chosen->binlogPos
            << " --stop-datetime=\"" << stopDateTime << "\""
            << " --base64-output=DECODE-ROWS"
            << " --verbose"
            << " --result-file=\"" << PathU8(human) << "\""
            << " \"" << PathU8(binPath) << "\"";
        RunCommand(dec.str());
    }

    int nInsert = 0, nUpdate = 0, nDelete = 0;
    {
        std::ifstream in(human);
        std::string line;
        while (std::getline(in, line))
        {
            if (line.rfind("### INSERT", 0) == 0) ++nInsert;
            else if (line.rfind("### UPDATE", 0) == 0) ++nUpdate;
            else if (line.rfind("### DELETE", 0) == 0) ++nDelete;
        }
    }
    fs::remove(human, ec);

    std::cout << "\n  재생 구간에 바뀌는 것:\n";
    if (nInsert + nUpdate + nDelete == 0)
        std::cout << "    (요약을 만들지 못했다. 위 파일을 직접 확인할 것)\n";
    else
    {
        std::cout << "    INSERT " << nInsert << " 행\n";
        std::cout << "    UPDATE " << nUpdate << " 행\n";
        std::cout << "    DELETE " << nDelete << " 행\n";
    }

    if (!bApply)
    {
        std::cout << "\n  미리보기까지만 했다. DB 는 그대로다.\n";
        ClearMysqlPassword();
        return true;
    }

    // ---- 적용 ----
    std::cout << "\n  [1/2] 백업 복구 중 (mmorpg 만)...\n";
    {
        // mysql 의 source 명령은 한글이 든 경로를 열지 못한다.
        // cmd 의 < 리다이렉션은 한글 경로도 정상이라 그쪽을 쓴다.
        std::ostringstream r;
        r << "\"" << mysql << "\" -u root --default-character-set=utf8mb4"
          << " < \"" << chosen->gamePath << "\"";
        if (RunCommand(r.str()) != 0)
        {
            std::cout << "  백업 복구 실패.\n";
            ClearMysqlPassword();
            return false;
        }
    }

    std::cout << "  [2/2] binlog 재생 중...\n";
    {
        std::ostringstream r;
        r << "\"" << mysql << "\" -u root --default-character-set=utf8mb4"
          << " < \"" << PathU8(replay) << "\"";
        if (RunCommand(r.str()) != 0)
        {
            std::cout << "  재생 실패. DB 가 백업 시점에 멈춰 있다.\n";
            ClearMysqlPassword();
            return false;
        }
    }

    ClearMysqlPassword();
    std::cout << "\n  복구 완료. 서버를 켜기 전에 값이 예상과 맞는지 확인할 것.\n";
    return true;
}
