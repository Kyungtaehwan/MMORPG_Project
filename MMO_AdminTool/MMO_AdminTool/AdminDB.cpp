#include "AdminDB.h"
#include "nanodbc.h"

#include <windows.h>
#include <conio.h>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <iterator>

#pragma comment(lib, "odbc32.lib")   // ODBC 네이티브 API 링크 (nanodbc 백엔드)

// ================================================================
//  연결
// ================================================================

// DSN 없이 드라이버 이름을 직접 박는 "DSN-less" 방식.
// 서버(CDB_Manager::Init)와 같은 문자열이라, 드라이버 이름이 바뀌면 양쪽 다 고쳐야 한다.
std::string CAdminDB::BuildConnStr(const std::string& db,
                                   const std::string& user,
                                   const std::string& password) const
{
    return
        "Driver={MySQL ODBC 9.7 Unicode Driver};"
        "Server=127.0.0.1;"
        "Port=3306;"
        "Database=" + db + ";"
        "User=" + user + ";"
        "Password=" + password + ";"
        "CHARSET=utf8mb4;";
}

bool CAdminDB::ConnectAsAnalyst()
{
    // 조회는 로그 DB 를 주로 보므로 기본 데이터베이스를 mmorpg_log 로 잡는다.
    // 게임 DB 를 볼 때는 SQL 안에서 mmorpg.테이블 처럼 이름을 붙여 쓴다.
    m_analystConn = BuildConnStr("mmorpg_log", "mmo_analyst", "analyst1234");

    try
    {
        nanodbc::connection conn(m_analystConn);
        std::cout << "[DB] 접속 성공 - " << conn.dbms_name() << " " << conn.dbms_version()
                  << " (계정: mmo_analyst / 읽기 전용)\n";
        return true;
    }
    catch (const std::exception& e)
    {
        std::cout << "[DB] 접속 실패: " << e.what() << "\n";
        std::cout << "     db/setup.sql 을 root 로 실행해 mmo_analyst 계정을 만들었는지 확인할 것.\n";
        return false;
    }
}

bool CAdminDB::ConnectAsRoot(const std::string& password)
{
    m_rootConn = BuildConnStr("mmorpg_log", "root", password);

    try
    {
        nanodbc::connection conn(m_rootConn);
        m_hasRoot = true;
        std::cout << "[DB] root 접속 확인.\n";
        return true;
    }
    catch (const std::exception& e)
    {
        m_hasRoot = false;
        std::cout << "[DB] root 접속 실패: " << e.what() << "\n";
        return false;
    }
}

// ================================================================
//  질의
//
//  nanodbc::result 는 "커서" 다. next() 를 부를 때마다 한 행씩 넘어간다.
//  화면에 줄을 맞춰 출력하려면 각 열의 최대 길이를 알아야 하고,
//  그러려면 모든 행을 먼저 다 읽어야 한다. 그래서 FTable 로 통째로 옮겨 담는다.
// ================================================================

// result 한 덩어리를 FTable 로 옮긴다.
static void ResultToTable(nanodbc::result& res, FTable& out)
{
    const short nCols = res.columns();

    out.headers.clear();
    out.rows.clear();

    for (short i = 0; i < nCols; ++i)
        out.headers.push_back(res.column_name(i));

    while (res.next())
    {
        std::vector<std::string> row;
        row.reserve(nCols);

        for (short i = 0; i < nCols; ++i)
        {
            // NULL 은 빈 칸이 아니라 '-' 로 표시한다.
            // "값이 없음" 과 "빈 문자열" 은 다른 것이라 구분해서 보여줘야 한다.
            if (res.is_null(i))
                row.push_back("-");
            else
                row.push_back(res.get<std::string>(i, ""));   // DB 도 콘솔도 UTF-8 이라 그대로 쓴다
        }
        out.rows.push_back(std::move(row));
    }
}

bool CAdminDB::Query(const std::string& sql, FTable& outTable, std::string& outError,
                     bool bUseRoot)
{
    std::vector<FTable> tables;
    if (!QueryMulti(sql, tables, outError, bUseRoot))
        return false;

    outTable = tables.empty() ? FTable{} : tables.front();
    return true;
}

bool CAdminDB::QueryMulti(const std::string& sql, std::vector<FTable>& outTables,
                          std::string& outError, bool bUseRoot)
{
    outTables.clear();
    outError.clear();

    const std::string& connStr = bUseRoot ? m_rootConn : m_analystConn;
    if (connStr.empty())
    {
        outError = "연결이 준비되지 않았다.";
        return false;
    }

    try
    {
        nanodbc::connection conn(connStr);
        nanodbc::result res = nanodbc::execute(conn, sql);

        // CALL 로 프로시저를 부르면 표가 여러 개 나온다.
        // next_result() 가 false 를 줄 때까지 계속 꺼낸다.
        do
        {
            if (res.columns() > 0)
            {
                FTable t;
                ResultToTable(res, t);
                outTables.push_back(std::move(t));
            }
        } while (res.next_result());

        return true;
    }
    catch (const std::exception& e)
    {
        outError = e.what();
        return false;
    }
}

bool CAdminDB::Execute(const std::string& sql, long& outAffected, std::string& outError,
                       bool bUseRoot)
{
    outAffected = 0;
    outError.clear();

    const std::string& connStr = bUseRoot ? m_rootConn : m_analystConn;
    if (connStr.empty())
    {
        outError = "연결이 준비되지 않았다.";
        return false;
    }

    try
    {
        nanodbc::connection conn(connStr);
        nanodbc::result res = nanodbc::execute(conn, sql);
        outAffected = res.affected_rows();
        return true;
    }
    catch (const std::exception& e)
    {
        outError = e.what();
        return false;
    }
}

// ================================================================
//  출력
// ================================================================

// ---- 인코딩 변환 ----
//  프로그램 안에서는 전부 UTF-8 을 쓰고,
//  Windows 의 ANSI API 를 지나야 할 때만 UTF-16 으로 바꾼다.

std::wstring Utf8ToWide(const std::string& utf8)
{
    if (utf8.empty()) return std::wstring();

    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                   static_cast<int>(utf8.size()), nullptr, 0);
    if (wlen <= 0) return std::wstring();

    std::wstring out(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                        static_cast<int>(utf8.size()), &out[0], wlen);
    return out;
}

std::string WideToUtf8(const std::wstring& wide)
{
    if (wide.empty()) return std::string();

    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                  static_cast<int>(wide.size()),
                                  nullptr, 0, nullptr, nullptr);
    if (len <= 0) return std::string();

    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                        static_cast<int>(wide.size()),
                        &out[0], len, nullptr, nullptr);
    return out;
}

// UTF-8 문자열이 화면에서 차지하는 칸 수.
//
//  바이트 수를 폭으로 쓰면 표가 어긋난다.
//    영문 'a' : 1바이트 = 1칸   -> 맞는다
//    한글 '가' : 3바이트 = 2칸  -> 1칸씩 밀린다
//
//  UTF-8 은 첫 바이트만 보면 그 글자가 몇 바이트인지 알 수 있다.
//    0xxxxxxx = 1바이트, 110xxxxx = 2바이트, 1110xxxx = 3바이트, 11110xxx = 4바이트
//  이어지는 바이트는 전부 10xxxxxx 이라 세지 않고 건너뛴다.
//
//  여기서 필요한 건 한글뿐이라
//  대표적인 넓은 구간만 2칸으로 세고 나머지는 1칸으로 본다.
size_t Utf8DisplayWidth(const std::string& utf8)
{
    size_t width = 0;

    for (size_t i = 0; i < utf8.size(); )
    {
        unsigned char c = static_cast<unsigned char>(utf8[i]);

        int    bytes = 1;
        unsigned cp  = c;

        if      ((c & 0x80) == 0x00) { bytes = 1; cp = c; }
        else if ((c & 0xE0) == 0xC0) { bytes = 2; cp = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { bytes = 3; cp = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { bytes = 4; cp = c & 0x07; }
        else                         { ++i; ++width; continue; }   // 깨진 바이트는 1칸으로

        if (i + bytes > utf8.size()) { ++i; ++width; continue; }

        for (int k = 1; k < bytes; ++k)
            cp = (cp << 6) | (static_cast<unsigned char>(utf8[i + k]) & 0x3F);

        // 두 칸을 차지하는 구간 (한글 완성형/자모, CJK 한자, 전각 기호)
        const bool bWide =
            (cp >= 0x1100  && cp <= 0x115F)  ||   // 한글 자모
            (cp >= 0x2E80  && cp <= 0xA4CF)  ||   // CJK 부수 ~ 한자
            (cp >= 0xAC00  && cp <= 0xD7A3)  ||   // 한글 완성형 (가 ~ 힣)
            (cp >= 0xF900  && cp <= 0xFAFF)  ||   // CJK 호환 한자
            (cp >= 0xFF00  && cp <= 0xFF60)  ||   // 전각 영숫자
            (cp >= 0xFFE0  && cp <= 0xFFE6);

        width += bWide ? 2 : 1;
        i     += bytes;
    }

    return width;
}

void PrintTitle(const char* title)
{
    std::cout << "\n";
    std::cout << "==================================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "==================================================================\n";
}

void PrintTable(const FTable& table)
{
    if (table.headers.empty())
    {
        std::cout << "  (결과 없음)\n";
        return;
    }

    const size_t nCols = table.headers.size();

    // 열마다 가장 긴 값의 "화면 폭" 을 구한다. 그게 그 열의 폭이 된다.
    //  UTF-8 이라 바이트 수와 칸 수가 다르므로 Utf8DisplayWidth 로 센다.
    //  (std::setw 도 바이트로 세기 때문에 못 쓴다. 아래에서 직접 공백을 채운다.)
    std::vector<size_t> width(nCols);
    for (size_t c = 0; c < nCols; ++c)
        width[c] = Utf8DisplayWidth(table.headers[c]);

    for (const auto& row : table.rows)
        for (size_t c = 0; c < nCols && c < row.size(); ++c)
            width[c] = (std::max)(width[c], Utf8DisplayWidth(row[c]));

    // 값 뒤에 모자란 칸만큼 공백을 붙여 왼쪽 정렬한다.
    auto cell = [&](const std::string& v, size_t w)
    {
        const size_t used = Utf8DisplayWidth(v);
        std::cout << " " << v;
        if (used < w) std::cout << std::string(w - used, ' ');
        std::cout << " |";
    };

    // 가로줄 그리기
    auto line = [&]()
    {
        std::cout << "  +";
        for (size_t c = 0; c < nCols; ++c)
        {
            std::cout << std::string(width[c] + 2, '-') << "+";
        }
        std::cout << "\n";
    };

    line();

    std::cout << "  |";
    for (size_t c = 0; c < nCols; ++c)
        cell(table.headers[c], width[c]);
    std::cout << "\n";

    line();

    if (table.rows.empty())
    {
        std::cout << "  (해당 없음 - 0 행)\n";
        line();
        return;
    }

    for (const auto& row : table.rows)
    {
        std::cout << "  |";
        for (size_t c = 0; c < nCols; ++c)
            cell((c < row.size()) ? row[c] : std::string(), width[c]);
        std::cout << "\n";
    }

    line();
    std::cout << "  " << table.rows.size() << " 행\n";
}

// 비밀번호를 화면에 안 보이게 받는다.
// 받은 UTF-16 을 마지막에 UTF-8 로 한 번에 바꾼다.
std::string ReadPassword(const char* prompt)
{
    std::cout << prompt << std::flush;

    std::wstring pw;
    for (;;)
    {
        wint_t ch = _getwch();

        if (ch == L'\r' || ch == L'\n')     // 엔터 = 입력 끝
            break;

        if (ch == L'\b')                    // 백스페이스
        {
            if (!pw.empty())
            {
                pw.pop_back();
                std::cout << "\b \b" << std::flush;   // 화면에서 별표 하나 지우기
            }
            continue;
        }

        if (ch == 0 || ch == 0xE0)          // 방향키 등 특수키는 2번에 나눠 오므로 버린다
        {
            _getwch();
            continue;
        }

        pw.push_back(static_cast<wchar_t>(ch));
        std::cout << "*" << std::flush;
    }
    std::cout << "\n";
    return WideToUtf8(pw);
}

// 한 줄 입력을 UTF-8 로 받는다.
//
//  ★ 왜 std::getline(std::cin) 을 그냥 안 쓰는가
//    콘솔 입력 코드페이지를 65001(UTF-8)로 맞춰 두면, 콘솔에서 바이트를
//    읽어오는 경로에 알려진 버그가 있어 첫 입력이 통째로 비어 나오거나
//    글자가 잘리는 일이 있다. UTF-16 을 직접 읽는 ReadConsoleW 에는 그 문제가 없다.
//
//  입력이 파이프/파일로 들어오면(테스트 자동화 등) 콘솔 핸들이 아니므로
//  ReadConsoleW 가 실패한다. 그때는 원래대로 getline 을 쓴다.
std::string ReadLineUtf8()
{
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);

    DWORD mode = 0;
    if (hIn != INVALID_HANDLE_VALUE && GetConsoleMode(hIn, &mode))
    {
        std::wstring line;
        wchar_t      buf[512];

        for (;;)
        {
            DWORD read = 0;
            if (!ReadConsoleW(hIn, buf, static_cast<DWORD>(std::size(buf)), &read, nullptr))
                break;
            if (read == 0)
                break;

            line.append(buf, read);

            // 엔터를 만나면 한 줄이 끝난 것. 줄바꿈 문자는 떼어낸다.
            if (line.find(L'\n') != std::wstring::npos)
                break;
        }

        while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n'))
            line.pop_back();

        return WideToUtf8(line);
    }

    std::string line;
    std::getline(std::cin, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        line.pop_back();
    return line;
}
