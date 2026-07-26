# MMO_StressTest — MMORPG_Project 부하 봇

IOCP 기반 부하 봇. 교수님 `stressTest`(NetworkModule)를 우리 서버 프로토콜에
맞춰 이식한 것. 시나리오 B(한 존 밀집 = 월드 로직/AOI 한계) 측정용.

## 구성
- `MMO_StressTest/StressBot.cpp` — 봇 본체(접속·이동·통계).
- `MMO_StressTest/Protocol.h` — **서버 `Protocol.h`의 복제본**. 서버 패킷 구조를
  바꾸면 이 파일도 같이 갱신할 것(클라/서버 복제본 규칙과 동일).

## 서버 준비 (중요)
봇은 `id="bot_<n>"` 로 로그인한다. 서버는 `STRESS_TEST` 매크로가 정의된
빌드에서만 이 접두어를 **DB(sp_login) 없이** 기본 스탯으로 마을(ZONE_TOWN)에
입장시킨다. 마을은 몬스터가 없어 전투 노이즈 없이 **순수 AOI 부하**만 측정된다.

- `STRESS_TEST` 는 서버 vcxproj의 **Debug|x64 / Release|x64** 전처리기 정의에 있음.
- 측정은 반드시 **Release** 서버로(디버그는 성능 수치가 무의미).
- 봇은 자동저장(DB)에서 제외되므로 DB를 오염시키지 않는다.
- 다른 시나리오(필드/레이드 = 몬스터·전투·길찾기 부하)를 재려면
  `Packet_Handler.cpp` 의 `STRESS_BOT_ZONE` 상수만 바꾸면 됨.

> 📖 **자세한 설명·코드해설·로드맵은 상위 폴더 `부하테스트_정리_및_로드맵.md` 참고** (이게 정본).

## 실행
```
# 1) 서버 먼저 (데스크탑, Release). 몬스터 마리수는 실행 전 환경변수로:
set STRESS_MON_COUNT=120
MMO_GameServer\x64\Release\MMO_GameServer.exe

# 2) 봇 (노트북).  인자: <서버IP> <봇수> <존> [hold|ramp]
#    존: 6=평지(Wing,A*X)  5=장애물(Orc,A*)  1=마을
MMO_StressTest.exe 192.168.0.12 500 6 hold    # 평지 500명 고정
MMO_StressTest.exe 192.168.0.12 1000 5 ramp   # 장애물, 자동증설로 한계 탐색
```

봇 콘솔(1초 주기):
```
접속 300/300 | 송신 1200 pps  수신 72000 pps  2.2 MB/s | 종단지연 avg 17 p50 16 p99 62 max 78 ms
```
- **수신 pps ≫ 송신 pps** 가 핵심: 이동 1건이 시야 내 N명에게 팬아웃 → AOI 비용.
- 종단지연 = 누군가 이동 → 서버 처리 → 내 시야로 브로드캐스트 → 수신까지 전체 파이프라인.

서버 콘솔의 `[부하 계측]` 섹션(권위 지표):
```
패킷 처리량 : 1272 pkt/s
처리 지연   : avg 4.2ms  p50 2.0ms  p99 16.4ms  max 30ms   # 핸들러 처리시간(락·GetNearPlayers 포함)
워커별 처리 : 40 38 40 47 ...                                # 워커 균형
```

## 측정 절차 (before/after 비교)
1. Release 서버 + 봇을 100 → 300 → 500 → … 로 올리며 서버 `p99` / 봇 `수신 pps` 기록.
2. `p99` 가 급격히 꺾이는 지점 = 현재 한계.
3. 개선(섹터 AOI / 전용 DB 스레드 / A* 조기반환) 후 **같은 봇 수**로 재측정.

## 튜닝 포인트 (StressBot.cpp 상단 상수)
- `g_targetBots` 기본값, `CONNECT_BATCH`/`RAMP_INTERVAL_MS` (램프 속도)
- `POS_INTERVAL_MS` (이동 중 위치보고 빈도 = 타일변경/AOI 트리거 밀도)
- `BOUND_MIN/MAX` (봇 이동 사각형 = 밀집도)
- 속도는 서버와 동일한 `MOVE_SPEED=1.0`(타일/초) 유지 — 넘으면 서버가 되돌림.
