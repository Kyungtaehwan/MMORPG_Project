#include "pch.h"
#include "IOCP_Server.h"
#include "Session_Manager.h"
#include "Zone_Manager.h"
#include "DB_Manager.h"
#include <iostream>

int main()
{
    std::cout << "=== MMO GameServer ===" << std::endl;

    // 어떤 최적화가 켜진 빌드인지 먼저 출력
    PrintServerConfig();

    // DB 연결 테스트 (저장 프로시저 호출용 ODBC 연결).
    if (!CDB_Manager::Get_Instance()->Init())
        std::cout << "[경고] DB 연결 실패 - 로그인이 동작하지 않습니다." << std::endl;

    // 저장 전용 스레드(주기/접속종료 저장을 IOCP 워커에서 떼어내 비동기 처리).
    CDB_Manager::Get_Instance()->StartSaveThread();

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
    CDB_Manager::Get_Instance()->StopSaveThread();   // 남은 저장 큐 비우고 조인
    CSession_Manager::Destroy_Instance();
    return 0;
}