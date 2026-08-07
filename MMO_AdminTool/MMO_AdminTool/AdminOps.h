#pragma once
#include <string>
#include <vector>

// ================================================================
//  AdminOps - 백업 / 시점 복구
//
//    AdminDB 는 DB 에 SQL 을 보냄
// 
//    반면 백업과 시점 복구는 MySQL 이 제공하는 별도 프로그램이 해야 한다.
//      - mysqldump    : DB 를 통째로 SQL 파일로 뽑아낸다
//      - mysqlbinlog  : binlog(변경 일지)를 읽어 SQL 로 바꾼다
//      - mysql        : SQL 파일을 DB 에 밀어넣는다
// ================================================================

// 백업 한 세트의 정보
struct FBackupSet
{
    std::string stamp;        // 20260804_145145
    std::string gamePath;     // 게임 DB 덤프 파일 경로
    std::string logPath;      // 로그 DB 덤프 파일 경로
    std::string binlogFile;   // 이 백업을 뜬 시점의 binlog 파일명
    std::string binlogPos;    // 그 안에서의 위치
    long long   sortKey = 0;  // 시각 비교용 (20260804145145)
};

// db 폴더를 찾는다. 실행 파일 위치에서 위로 올라가며 db/setup.sql 을 찾는다.
// 못 찾으면 빈 문자열.
std::string FindDbDir();

// 관리자 권한으로 실행 중인가. PITR 에 필요하다.
bool IsRunningAsAdmin();

// 게임 서버가 켜져 있는가.
//  켜진 채로 DB 를 고치면 5초 주기 자동 저장이 메모리 상태로 덮어써서
//  복구가 조용히 원복된다. 이 프로젝트에서 가장 놓치기 쉬운 함정이다.
bool IsGameServerRunning();

// MySQL 도구 경로를 찾는다 (mysqldump / mysqlbinlog / mysql).
std::string FindMySqlTool(const std::string& name);

// db/backups 폴더의 백업 목록을 최신순으로 읽는다.
std::vector<FBackupSet> ListBackups(const std::string& dbDir);

// 백업을 뜬다. 성공하면 true.
bool DoBackup(const std::string& dbDir, const std::string& rootPassword);

// 시점 복구.
//  bApply=false 면 재생할 SQL 만 만들고 DB 는 건드리지 않는다.
bool DoPitr(const std::string& dbDir, const std::string& rootPassword,
            const std::string& stopDateTime, bool bApply);
