#pragma once
#include <string>
#include <vector>

// ================================================================
//  CAdminDB - 운영 도구의 DB 담당
//
//  하는 일은 딱 두 가지다.
//    1) DB 에 질문(SQL)을 보낸다
//    2) 돌아온 표를 콘솔에 예쁘게 출력한다
//
//  서버(CDB_Manager)와 같은 방식(nanodbc + ODBC)으로 붙지만,
//  목적이 달라서 계정이 다르다.
//    - 서버        : mmo_server  (게임 데이터를 읽고 쓴다)
//    - 이 도구(조회): mmo_analyst (읽기만. 조사하다 실수로 고치는 걸 막는다)
//    - 이 도구(조치): root        (되돌리기/복구할 때만 비밀번호를 물어본다)
//
//  ★ 이 프로그램의 std::string 은 전부 UTF-8 이다
//    프로젝트에 /utf-8 컴파일 옵션이 걸려 있어서, 소스의 한글 리터럴이
//    UTF-8 바이트 그대로 실행 파일에 들어간다.
//    DB 연결(utf8mb4)도, 콘솔(SetConsoleOutputCP(CP_UTF8))도 UTF-8 이라
//    중간에 변환할 곳이 없다. 한 가지 인코딩으로 관통시키는 것이 요점이다.
//
//    (예전에는 리터럴이 CP949 로 구워져서 SQL 에 한글을 못 넣었고,
//     DB 에서 온 글자는 Utf8ToCp949 로 되돌려야 했다. 지금은 둘 다 필요 없다.)
// ================================================================

// DB 에서 읽어온 표 하나. 화면에 뿌리기 좋게 문자열로만 담는다.
struct FTable
{
    std::vector<std::string>              headers;   // 열 이름
    std::vector<std::vector<std::string>> rows;      // 값들

    bool empty() const { return rows.empty(); }
};

class CAdminDB
{
public:
    // 읽기 전용 계정으로 접속. 프로그램 시작할 때 한 번 부른다.
    bool ConnectAsAnalyst();

    // 조치용. root 비밀번호를 받아 접속 가능한지 확인한다.
    bool ConnectAsRoot(const std::string& password);

    bool HasRoot() const { return m_hasRoot; }

    // ---- 질의 ----
    //  bUseRoot=true 면 root 연결로 보낸다(되돌리기 등 쓰기 작업).
    //  실패하면 false 를 돌려주고 outError 에 이유를 담는다.
    bool Query(const std::string& sql, FTable& outTable, std::string& outError,
               bool bUseRoot = false);

    // CALL 처럼 결과 표가 여러 개 나오는 것을 위한 버전.
    bool QueryMulti(const std::string& sql, std::vector<FTable>& outTables,
                    std::string& outError, bool bUseRoot = false);

    // 표를 안 돌려주는 것(UPDATE/INSERT 등). 바뀐 행 수를 돌려준다.
    bool Execute(const std::string& sql, long& outAffected, std::string& outError,
                 bool bUseRoot = true);

private:
    std::string BuildConnStr(const std::string& db,
                             const std::string& user,
                             const std::string& password) const;

    std::string m_analystConn;   // mmo_analyst 용 연결 문자열
    std::string m_rootConn;      // root 용
    bool        m_hasRoot = false;
};

// ---- 출력 도우미 (AdminDB.cpp 에 구현) ----

// 표를 테두리와 함께 줄 맞춰 출력한다.
void PrintTable(const FTable& table);

// 제목 줄. 화면에서 구획을 나눈다.
void PrintTitle(const char* title);

// UTF-8 문자열이 콘솔에서 차지하는 칸 수.
//  바이트 길이로는 표를 못 맞춘다 - 한글은 3바이트인데 화면에서는 2칸이다.
//  표의 열 폭을 계산할 때 쓴다.
size_t Utf8DisplayWidth(const std::string& utf8);

// 화면에 안 보이게 비밀번호를 입력받는다.
std::string ReadPassword(const char* prompt);

// 콘솔에서 한 줄 입력받는다(UTF-8 로 돌려준다).
//  콘솔 입력 코드페이지가 65001 일 때 std::getline(std::cin) 은
//  첫 글자를 흘리거나 빈 줄을 주는 일이 있다. 그래서 ReadConsoleW 로
//  UTF-16 을 직접 읽어 UTF-8 로 바꾼다.
//  파이프로 입력이 들어오는 경우(콘솔이 아님)는 getline 으로 넘어간다.
std::string ReadLineUtf8();

// ---- 인코딩 변환 (Windows API 경계용) ----
//  Windows 의 ANSI API(std::system, fs::path::string 등)는 UTF-8 을 모른다.
//  그런 곳을 지날 때만 UTF-16 으로 바꿔서 넘긴다.
std::wstring Utf8ToWide(const std::string& utf8);
std::string  WideToUtf8(const std::wstring& wide);
