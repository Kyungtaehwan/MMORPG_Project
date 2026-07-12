#include "pch.h"
#include "IOCP_Server.h"
#include "Session_Manager.h"
#include "Zone_Manager.h"
#include "DB_Manager.h"
#include <iostream>

int main()
{
    std::cout << "=== MMO GameServer ===" << std::endl;

    // DB 연결 테스트 (sp_login 등 저장 프로시저 호출용 ODBC 연결).
    // 실패해도 서버는 뜨지만, 로그인은 전부 실패하게 된다 - 즉시 경고.
    if (!CDB_Manager::Get_Instance()->Init())
        std::cout << "[경고] DB 연결 실패 - 로그인이 동작하지 않습니다." << std::endl;

    CZone_Manager::Get_Instance();  // 생성자에서 맵 생성
    std::cout << "맵 초기화 완료" << std::endl;
    // 워커 스레드 뜨기 전에 미리 생성
    // 멀티스레드 경합 없이 안전하게 초기화
    CSession_Manager::Get_Instance();
    //시작
    CIOCP_Server server;
    if (!server.Start(7777))
    {
        std::cout << "서버 시작 실패" << std::endl;
        CSession_Manager::Destroy_Instance();
        return -1;
    }

    server.Run();
    // 서버 종료 시 명시적 해제
    CSession_Manager::Destroy_Instance();
    return 0;
}