# MMORPG_Project — 프로젝트 가이드

2D 아이소메트릭 MMORPG. **클라이언트(Direct2D, Win32)** + **게임서버(IOCP)** 두 개의 Visual Studio 솔루션으로 구성. C++17, PlatformToolset v143, x64, CharacterSet=Unicode.

> 이 문서는 매 세션 자동 로드되어 전체 코드를 다시 훑지 않고도 구조를 파악하기 위한 것. 기능 추가 시 먼저 여기서 관련 파일을 찾고, 변경 후 구조가 바뀌면 이 문서도 갱신할 것.

## 디렉터리

- `MMO_Client/MMO_Client/` — 클라이언트 소스. `MMO_Client.sln` 으로 빌드. 엔트리: `Game.cpp`(WinMain).
- `MMO_GameServer/MMO_GameServer/` — 서버 소스. `MMO_GameServer.sln`. 엔트리: `MainServer.cpp`(main). 포트 **7777**.
- `MMO_StressTest/MMO_StressTest/` — 부하 봇. `MMO_StressTest.sln`. 인자: `<서버IP> <봇수> <존> [hold|ramp]`.
- `MMO_AdminTool/MMO_AdminTool/` — **운영 도구(콘솔)**. `MMO_AdminTool.sln`. `db/*.sql`과 `db/*.ps1`이 하던 일(탐지/조사/환수/백업/PITR)을 메뉴 7개로 옮긴 것. 구성: `AdminTool.cpp`(메뉴+SQL 전문), `AdminDB.{h,cpp}`(접속·질의·표 출력), `AdminOps.{h,cpp}`(mysqldump/mysqlbinlog 실행), `nanodbc.{h,cpp}`(서버에서 복사). **조회는 `mmo_analyst`(읽기전용)로 붙고 환수/백업/PITR 때만 root 비밀번호를 물어본다.** 사용자 입력은 화이트리스트로 걸러 SQL 인젝션 차단. 7번(PITR)만 관리자 권한 필요.
  - ⚠️ **이 솔루션만 인코딩이 UTF-8 통일이다**(2026-08-04, 콘솔 한글이 전부 깨져서 고침). vcxproj에 **`/utf-8`** + `main()`의 **`SetConsoleOutputCP(CP_UTF8)`** + 커넥션 `CHARSET=utf8mb4`, **셋이 같아야 한다**. 처음엔 "한국어 윈도우 = CP949"로 맞췄다가 깨졌다 — Windows Terminal은 65001로 뜨고 프로그램이 도중에 949로 바꿔도 따라오지 않으며, 출력을 파일/다른 프로그램으로 넘길 때도 UTF-8이라야 안 깨진다. 덕분에 **SQL에 한글을 써도 되고**(별칭이 영문인 건 표 폭 때문), DB에서 온 문자열도 변환 없이 그대로 출력한다(`Utf8ToCp949` 삭제됨).
  - ⚠️ **대신 Windows ANSI API 경계에서만 wide로 바꾼다** — 리포 경로에 `문서`가 있어서 안 하면 파일을 못 찾는다: `std::system`→**`_wsystem`**, `_putenv_s`→`_wputenv_s`, `fs::path::string()`→`PathU8()`, `fs::path(문자열)`→`U8Path()`, `ifstream`은 wide 경로로. "안에서는 UTF-8, 밖으로 나갈 때만 UTF-16".
  - 표 정렬은 바이트가 아니라 **`Utf8DisplayWidth`**(한글 3바이트=2칸)로 계산하고 공백을 직접 채운다(`std::setw`도 바이트 기준이라 못 씀). 입력은 `ReadLineUtf8`(`ReadConsoleW`, 파이프면 `getline` 폴백), 비밀번호는 `_getwch`.
- `db/setup.sql` — **DB 설치 스크립트 (이 파일 하나가 전부)**. Workbench에서 **root로** 전체 실행(⚡). 신규 설치/기존 업그레이드를 모두 처리하고, `mmo_server` 계정 생성·권한·시드·로그 DB(`mmorpg_log`)까지 포함한다. 재실행 안전(초기화되는 건 `test1`/`test2`와 판매자 `경매장` 기본 매물뿐). 예전의 schema/procedures/seed/auction_seed/migration 2개/fix_login 7개 파일은 여기로 통합 후 삭제(git 이력에 남아 있음). **인코딩: UTF-8+BOM 필수** — BOM이 없으면 한국어 Windows의 Workbench가 CP949로 오독해 에러가 난다. 또한 **CP949에 없는 문자(`⚡` U+26A1, em dash `—` 등)를 넣지 말 것** — Workbench가 파일을 거부한다(실제로 겪음). mysql CLI는 BOM을 정상 처리하는 것을 확인했다.
- ⚠️ **`db/backup.ps1` / `db/pitr.ps1`은 2026-08-04에 삭제됨.** `MMO_AdminTool` 메뉴 6/7이 같은 `mysqldump`/`mysqlbinlog` 명령을 직접 실행하므로 중복이었다(**AdminTool은 `.ps1`을 부르지 않는다 — 애초에 부른 적 없음**). 두 벌을 두면 한쪽만 고쳤을 때 어느 게 맞는지 알 수 없어 지웠다. **git에 커밋된 적 없어 복구 불가**. 그 스크립트들이 하던 일의 설계 근거는 아래 두 항목과 `DB_TOOLS_EXPLAINED.md` 7-2/7-3절에 남아 있다.
  - **백업(메뉴 6)의 계약**: `mmorpg`와 `mmorpg_log`를 **별도 파일로** 덤프(복구 때 로그를 같이 되돌리면 보상 산정이 불가능해지므로). 비번은 `MYSQL_PWD` 환경변수로 넘겨 프로세스 목록 노출 방지. `--source-data=2`로 binlog 좌표 확보, **`--routines` 필수**(빼먹으면 복구본에 `sp_login`이 없어 로그인 불가). 결과물은 `db/backups/`(gitignore). 덤프 검증법은 **DB 이름만 바꿔 별도 DB로 복구 후 행 수 비교**(현재 DB 무손상) — 6테이블/9건 경매/한글 판매자명/`sp_login`까지 일치 확인함.
- `db/gen_test_data.sql` — **탐지 쿼리 검증용 합성 데이터 픽스처**(root로 실행, 재실행 안전, `user%` 계정과 그 로그만 건드림). 정상 유저 30명 + 악용자 `user17`(2026-08-03 20:00~23:00 골드 드롭 180건). **봇(`MMO_StressTest`)은 로그인/이동/공격만 하고 줍기·상점·경매 패킷을 아예 안 보내므로 거래 로그가 0건** — 그래서 SQL로 만든다. 난수 대신 `CRC32` 해시라 **재실행해도 같은 데이터**. `gold_balance`는 지어내지 않고 **시간순 누적합으로 계산**(안 그러면 정상 유저 전원이 연속성 검사에 걸린다). `character.gold`/`inventory`도 로그와 일치하게 맞춘다.
- `db/detect.sql` — **탐지/범위/산정 쿼리 묶음**(읽기 전용, `mmo_analyst`로 실행). [1]집계 이상치(**평균이 아니라 중앙값 대비** — 악용자가 평균을 끌어올려 자기를 가림) → [2]시간대별로 구간 특정 → [3]연속성 검사(**0행인 게 결론** = 로그를 믿을 수 있다 = 선별 환수 가능) → [4]`target`으로 전파 범위(0건이면 백섭 불필요) → [5]부정 이득 산정 → [6]**실행할 `CALL sp_rollback_account(...)` 문장을 만들어 뱉는다**. 맨 위 `SET @suspect` 한 줄만 바꿔 쓴다. ⚠️ 첫머리에 `SET NAMES utf8mb4 COLLATE utf8mb4_general_ci` 필수(DB는 general_ci인데 접속 기본값은 0900_ai_ci라 컬럼과 변수 비교에서 Error 1267).
- `db/gen_dupe_incident.sql` / `db/detect_dupe.sql` — **시나리오 2(복제 버그 확산) 픽스처 + 조사 쿼리**. 계정 120개(`dup001`~`dup120`), `dup001`의 골드가 **로그 없이 500,000 증가** → 연속성 검사에 정확히 1건 걸림 → 경매 대금으로 **100명에게 전파**. 조사는 [1]설명되지 않는 골드 → [2]백섭 시점 → [3]전파 범위 → [4]**선별 환수 시 함께 날아가는 정상 거래 규모**(101계정/1052로그 중 852건이 사건 무관 = 선별이 더 정밀하다는 직관이 뒤집히는 지점) → [5]보상 산정. 보상은 **계정 단위 제외가 아니라 금액 단위로 오염분만 차감**(대금 받은 사람도 그 시간에 자기 사냥은 했으므로). ⚠️ **사건 이후 정상 플레이 구간이 데이터에 반드시 있어야** 보상 산정이 성립한다(백섭이 날리는 게 그 구간).
- **PITR(메뉴 7)의 계약**: 백업 세트를 시각으로 자동 선택(되돌릴 시점보다 **앞선** 것만) → 덤프에서 binlog 좌표 추출 → 재생 파일 목록 구성 → `mysqlbinlog`로 `replay.sql` 생성 → 확인 후 적용. ⚠️ **`--database=mmorpg` 필터가 핵심**: binlog에는 `mmorpg_log` 변경도 들어 있는데 로그 DB는 되돌리지 않았으므로, 필터 없이 재생하면 **로그가 두 번 들어가 보상 산정이 두 배로 어긋난다**. ⚠️ **관리자 권한 필수**(binlog 폴더가 MySQL 서비스 계정 소유) — 실행 전 **관리자 권한/게임서버 종료/백업 존재** 3가지를 검사하고 하나라도 안 되면 멈춘다(`AdminOps.cpp::DoPitr`). ⚠️ **픽스처 시각으로는 PITR 실습이 안 된다** — binlog는 로그의 `created_at`이 아니라 **SQL이 실제 실행된 시각**으로 기록되므로, 실습하려면 백업 → 실시간 사고 → 복구 순으로 해야 한다.
- `ProjectPlane.md` — 주차별 개발 계획/진행 로그.
- 빌드 산출물(`x64/`, `.vs/`)은 무시.

## ⚠️ 반드시 지킬 규칙 (gotchas)

1. **인코딩 함정(매우 중요)**: 대부분의 소스는 **CP949(BOM 없음)**로 저장돼 있다 (그래서 Read 도구로 보면 한글 주석이 mojibake로 보임). 이런 CP949 파일을 **Edit/Write 도구로 수정하면 UTF-8로 재저장되어 기존 한글이 전부 깨진다**(주석·`L"..."` 문자열 포함). 따라서 한글이 든 파일을 편집하기 전, 먼저 **UTF-8+BOM으로 변환**할 것:
   - 손상됐으면 `git checkout HEAD -- <file>`로 원본(CP949) 복원 → PowerShell로 `GetEncoding(949)`로 읽어 `UTF8Encoding($true)`(BOM)로 다시 써서 변환 → 그 다음 Edit 사용.
   - 편집 후 BOM이 유지됐는지 확인(Edit가 BOM을 떼면 다시 CP949로 오인됨). UTF-8+BOM이면 MSVC가 로케일 무관하게 UTF-8로 읽으므로 안전.

   - **새로 만드는 파일도 반드시 UTF-8+BOM으로 저장할 것.** BOM 없이 한글 주석이 들어가면 MSVC가 CP949로 오독해, 주석의 바이트가 다음 줄 선언을 삼켜 **"선언되지 않은 식별자"** 같은 엉뚱한 컴파일 에러가 난다(실제로 `RaidMap.h`에서 겪음).
   - **이미 UTF-8+BOM으로 변환 완료된 파일**(자유롭게 편집 가능): 양쪽 `Protocol.h`/`RaidMap.h`, 서버 `Zone.cpp`/`Zone_Manager.{h,cpp}`/`Packet_Handler.{h,cpp}`/`Player.h`/`Monster.h`/`SaveData.h`/`GameLog.h`(신규, BOM으로 생성)/`AccountDB.h`/`DB_Manager.cpp`/`Session_Manager.h`(2026-08-02 변환)/`ServerConfig.h`/`RWLock.h`(신규, BOM으로 생성), 클라 `Network_Manager.{h,cpp}`/`Map_Manager.cpp`/`Portal.cpp`/`Zone_Field_N.{h,cpp}`/`Zone_Town.cpp`/`Zone_Raid.{h,cpp}`/`UI_ZoneSelect.{h,cpp}`/`GameObject.h`/`Player.cpp`/`UI_HUD.{h,cpp}`/`UI_QuickSlot.{h,cpp}`/`define.h`/`Object_Manager.h`/`UI_Manager.{h,cpp}`/`NPC_Angel.{h,cpp}`. 나머지(예: 클라 `Zone.h`)는 아직 CP949이므로 편집 전 변환 필요.
2. **`Protocol.h`는 클라/서버 양쪽에 복제본이 있다.** (`MMO_Client/.../Protocol.h`, `MMO_GameServer/.../Protocol.h`) 패킷 구조를 바꾸면 **양쪽 파일을 동시에 똑같이** 수정해야 한다. 한쪽만 고치면 직렬화가 깨진다. `#pragma pack(push,1)` 유지 필수. **`RaidMap.h`도 같은 복제본 규칙**(한쪽만 고치면 대형 맵이 달라져 이동 검증이 깨짐).
3. 클라 `define.h`에 `#define NO_SERVER`가 있음 — 이름과 정반대다. 정의돼 있어도 서버 접속은 항상 하며(온라인 모드), 실제 효과는 `Monster::Set_ServerPos`의 위치 스냅뿐. **끄면 `Monster.h`의 `#else` 분기가 미선언 멤버를 참조해 컴파일이 깨진다**(한 번도 빌드된 적 없는 죽은 코드). 온라인 테스트한다고 끄지 말 것.
4. enum 값은 클라/서버가 **숫자까지 일치**해야 함 (예: `ZONE_ID`, `PLAYER_STATE`, `MONSTER_STATE`, `DIRECTION/MONSTER_DIR`, `TILE_TYPE`). 단, `TILE_TYPE`은 클라(`TILE_BLOCK=10`)와 서버(`TILE_BLOCK=1`)의 숫자가 다르니 주의 — 타일 동기화 시 매핑 확인.
5. **입력 함정 — `CInput_Manager::Key_Down()`은 클릭을 "소비"한다.** 호출하는 순간 `m_bKeyState`를 true로 바꿔서, **한 프레임에 처음 부른 쪽만 true를 받고 나머지는 전부 false**를 받는다(`Mouse_Down()`은 프레임 시작에 한 번 계산된 값이라 여러 번 읽어도 안전). 프레임 순서가 `Object_Manager::Update` → `UI_Manager::Update`이고 `CPlayer::Update`가 `Key_Down(VK_LBUTTON)`을 먼저 부르므로, **새 UI를 만들 때 `Open()`에서 `Set_InputMode(INPUT_MODE_UI)`를, `Close()`에서 `INPUT_MODE_GAME`을 반드시 호출해야 한다**(`CPlayer::Update`는 `!Is_GameMode()`면 즉시 리턴 → 클릭을 양보). 안 하면 그 UI의 버튼이 **영원히 안 눌린다**. `CUI_Shop`/`CUI_Auction`/`CUI_ZoneSelect`가 모두 이 규약을 따른다.

6. **SQL 파일 작성 규칙 (`db/*.sql`) — 셋 다 실제로 겪은 것.** 실행 환경이 **MySQL Workbench(한국어 Windows)** 라서 CLI와 동작이 다르다. CLI로만 테스트하면 Workbench에서 터진다.
   - **BOM을 넣지 말 것 (UTF-8, BOM 없음).** Workbench는 파일을 에디터로 읽어 **텍스트를 그대로 서버에 전송**하므로 BOM이 첫 구문에 섞여 `Error 1064`가 난다(`near '﻿-- ===...'`). 반면 `mysql` CLI의 `source`는 BOM을 걸러내므로 **CLI 테스트로는 이 문제가 안 잡힌다**. C++ 소스는 정반대로 BOM이 필수(1번 규칙)이니 혼동 주의.
   - **CP949에 없는 문자를 쓰지 말 것.** `⚡`(U+26A1), em dash `—`(U+2014) 등이 들어가면 Workbench가 파일을 거부한다. `★`, `→`는 CP949에 있어 안전. 확인법: PowerShell로 `GetEncoding(949)` 왕복 변환해 원문과 다른 문자를 찾는다.
   - **키 컬럼 없는 `WHERE`로 `DELETE`/`UPDATE` 하려면 safe update mode를 꺼야 한다.** Workbench는 이게 기본 ON이라 `Error 1175`가 난다(예: `DELETE FROM auction WHERE seller_name='경매장'` — `seller_name`은 인덱스 없음). 사용자 설정을 바꾸라고 하지 말고 **스크립트가 세션 단위로 껐다 되돌릴 것**: 앞에서 `SET @old_safe_updates := @@SQL_SAFE_UPDATES; SET SQL_SAFE_UPDATES = 0;`, 끝에서 `SET SQL_SAFE_UPDATES = @old_safe_updates;`. `setup.sql` 0장/8장이 이 방식이다.
   - 검증 요령: `--force`는 `source`에서 동작하지 않아 첫 오류에서 멈춘다. 권한이 필요한 스크립트의 문법을 확인하려면 **권한 없이 실행 가능한 구간만 줄 범위로 잘라내 실제로 파싱**시킬 것(정규식으로 줄을 고르면 여러 줄 구문이 잘려 엉뚱한 1064가 난다).


## 최적화 토글 (`ServerConfig.h`) — 부하 테스트 before/after 비교용

서버의 모든 최적화는 **`MMO_GameServer/MMO_GameServer/ServerConfig.h`의 `USE_*` 매크로 하나로 켜고 끈다.** `pch.h`가 이 헤더를 include하므로 **모든 TU가 같은 값을 본다**(플래그가 클래스 멤버의 *타입*을 바꾸므로 이 일관성이 필수 — 일부만 재컴파일되면 ODR 위반으로 메모리가 조용히 깨진다). **값을 바꾸면 전체 리빌드**할 것.

- 현재 플래그(전부 **기본값 = BASE**): `USE_RW_LOCK`, `USE_MEMORY_POOL`, `USE_SECTOR_AOI`. **구현이 없는 플래그는 미리 만들지 말 것**(한 번 부풀렸다가 걷어냈다).
- **`USE_RW_LOCK`은 0/1이 아니라 3자 선택**: `RW_LOCK_MUTEX`(기준선) / `RW_LOCK_SPIN`(직접 구현 `FRWSpinLock`, `RWLock.h`) / `RW_LOCK_SHARED`(`std::shared_mutex`). 세 방식을 같은 코드로 갈아끼워 비교하는 게 목적.
- **락은 `#if`를 코드에 뿌리지 말고 추상화된 별칭/매크로를 쓸 것**: 멤버는 `FRWLock`, 획득은 `READ_LOCK(m)` / `WRITE_LOCK(m)`. 세 모드 모두 같은 호출부로 동작한다. 매크로는 `__LINE__`으로 변수명을 만들어 한 스코프에서 여러 번 잠글 수 있다.
- **`FRWSpinLock`(`RWLock.h`) 구조**: `atomic<uint32>` 하나에 **상위 16비트=쓰기 스레드 번호, 하위 16비트=읽기 개수**를 담아 한 번의 CAS로 상태 전이. 쓰기 재귀 허용(`m_writeCount`), `MAX_SPIN_COUNT`(4000) 후 `yield`, 스핀 중 `_mm_pause()`. 알고리즘은 루키스 강의 `Lock`과 동일하고, 아래 두 가지만 이 프로젝트에 맞게 바꿨다.
  - **스레드 번호는 `GetLockThreadId()`가 함수 지역 `thread_local`로 첫 사용 시 자동 발급**한다. 루키스 원본은 전역 `LThreadId`를 `ThreadManager::InitTLS()`가 채우는 방식인데, **이 프로젝트엔 ThreadManager가 없고 `std::thread`로 직접 스레드를 만들므로** 그대로 쓰면 모든 스레드가 `LThreadId==0`이 되고 `desired == EMPTY_FLAG`라 **쓰기 락이 조용히 무력화된다**(여러 스레드가 동시에 쓰기 진입 + 서로를 자기 자신으로 오인해 재귀 통과). 옮겨올 때 반드시 주의할 함정.
  - **`RWLOCK_CRASH`로 실패를 즉시 죽인다**(루키스 방식 채택): 10초(`ACQUIRE_TIMEOUT_MS`) 안에 못 잡으면 데드락으로 보고 크래시, `WriteUnlock` 전에 읽기가 남아 있어도 크래시, `ReadUnlock` 언더플로는 `fetch_sub` **반환값**으로 검사해 크래시. `assert`가 아니라 매크로인 이유는 **Release에서도 검사가 살아 있어야** 하기 때문. 콘솔에 원인을 찍고 죽으므로 행보다 원인 파악이 쉽다.
  - 도입 안 한 것: 루키스의 `DeadLockProfiler`(락마다 전역 뮤텍스+맵 조회라 Debug 측정을 오염시킴), `USE_LOCK`/`_locks[]` 무인자 매크로(이 프로젝트는 `CPlayer`처럼 **클래스당 락이 여러 개**라 `READ_LOCK_IDX(1)`이 되면 이름이 사라짐).
- **⚠️ 스핀락 적용 금지 구간**: 락을 쥔 채 `Send()`/DB 호출/O(N) 순회를 하는 곳(특히 `CZone`의 `Broadcast_*`, `GetNearPlayers`). 대기 스레드가 CPU를 태우므로 손해다. 그런 곳은 `RW_LOCK_SHARED`로 비교할 것.
- **적용 완료(읽기 12 / 쓰기 7)**:
  - `CPlayer_Manager::m_lock` — `Get_Player`=READ / `Create`·`Remove`·`AutoSaveNext`=WRITE (**`AutoSaveNext`는 읽기처럼 보이지만 `m_saveCursor`를 갱신하므로 쓰기**)
  - `CSession_Manager::m_lock` — `Get_Session`=READ / `Assign`·`Release`=WRITE
  - `CZone::m_zoneLock` — 순회 10곳 전부 READ / `EnterZone`·`LeaveZone`만 WRITE
- **아직 mutex인 곳(의도적)**: `CSession::m_sendLock`, `g_timerLock` — 큐 넣고 빼기가 **전부 변경**이라 "동시 읽기"라는 개념이 없어 RW 락으로 바꿔도 이득이 0이다. `CPlayer::m_viewLock`/`m_saveLock`은 읽기·쓰기 비율이 비슷해 후순위.
- 타입이 안 바뀌고 함수 본문만 갈라지는 곳은 `#if` 대신 **`if constexpr (kUseSectorAoi)`** 를 쓰는 편이 낫다(꺼진 쪽도 문법 검사를 받아 코드가 썩지 않음). `kUse*` constexpr 상수가 플래그마다 준비돼 있다.
- 튜닝값: `SECTOR_SIZE`(**6**), `MEMORY_POOL_CHUNK`(1024). **섹터 스캔은 3×3 고정이 아니라 시야 사각형이 걸치는 범위만** 본다 — `CZone::CollectPlayersNear`(`Zone.cpp`)가 `(타일 ± nRange) / SECTOR_SIZE`로 범위를 구하고, `GetNearPlayers`는 `nRange=VIEW_RANGE`로 그걸 호출한다(범위 계산이 `GetNearPlayers` 안에 없으니 찾을 때 주의). `SECTOR_SIZE=6/VIEW_RANGE=5` 기준 평균 2.67열 × 2.67행 ≈ 7.1칸으로 3×3(9칸) 대비 21% 절감. 몬스터 어그로(8타일)/해제(13타일)/브로드캐스트(`VIEW_RANGE+3`)도 같은 함수를 `nRange`만 바꿔 부르므로 자동으로 덮인다 — **3×3 고정이었다면 `SECTOR_SIZE=6`으로는 6타일까지밖에 못 덮어 몬스터가 타겟을 놓쳤다**(이게 사각형 기준으로 바꾼 이유). `Zone.h`의 `static_assert(SECTOR_SIZE >= VIEW_RANGE)`는 이제 정확성 조건이 아니라 헛스캔 방지용 튜닝 가드다.
- `main()`이 `PrintServerConfig()`로 활성 플래그를 출력하고, `GetServerConfigTag()`가 `"BASE"`/`"RW+SECTOR"` 같은 태그를 준다. **측정 로그에 이 태그를 남겨야 나중에 비교가 된다.**
- **측정 원칙: 한 번에 하나씩만 켜고 BASE와 비교**한다(둘을 동시에 켜면 원인 분리가 안 됨).
- 프로젝트는 **C++17**(`LanguageStandard=stdcpp17`, 4개 구성 전부). 이전엔 미지정이라 기본 C++14로 빌드되고 있었고, `std::shared_mutex`/`if constexpr`/inline 변수가 모두 C++17이라 2026-08-02에 명시적으로 올렸다.
- x64 구성에는 기존 `STRESS_TEST` 전처리기 정의도 있다(계측 코드용, `ServerConfig.h`와는 별개).

## 네트워크 프로토콜 (핵심 계약)

`Protocol.h` — `PacketHeader{ uint16 size; uint16 id; }` 뒤에 페이로드. ID 규칙:
- `CS_*` = 1000번대 (Client→Server): LOGIN, MOVE_DEST(목적지 클릭), MOVE_POS(현재위치 갱신), ATTACK_MONSTER, RESPAWN, PORTAL(존 이동 요청).
- `SC_*` = 2000번대 플레이어, 2100번대 몬스터: LOGIN_OK/FAIL, ENTER_GAME, ADD/REMOVE/MOVE_PLAYER, PLAYER_STATE/HIT, RESPAWN, CHANGE_ZONE(존 전환 확정) / ADD/REMOVE/MOVE_MONSTER, MONSTER_STATE/HIT.

**월드 구성(존)**: 허브인 **마을(ZONE_TOWN)** 중심으로 동서남북 4개 몬스터 필드 — 북=`ZONE_FIELD_N`(구 `ZONE_TEST`, enum 값 0 유지 → DB `zone_id` 마이그레이션 불필요), 동=`ZONE_FIELD_E`, 남=`ZONE_FIELD_S`, 서=`ZONE_FIELD_W`. 모든 필드 20×30, 테두리 동일, 장애물 배치만 다름. 마을 4모서리(N(6,6)/E(19,6)/S(19,29)/W(6,29))에 포탈, 각 필드의 복귀 포탈은 마을 반대편에 위치. 몬스터는 서버가 각 필드에 랜덤 배치(`Zone_Manager.cpp::SpawnRandomMonsters`). 클라 필드 존 클래스: `CZone_Field_N/E/S/W`(개별 .h/.cpp + vcxproj 등록됨). **각 필드 블록맵은 클라(`CZone_*::Build`)와 서버(`BLOCK_MAP_FIELD_*`)가 반드시 동일해야 함**(이동 검증). 좌표: 내부 grass X 3..22, Z 3..32. 로그인 시작 존은 ZONE_FIELD_N(북). 잔디 타일 이미지 키는 모든 필드/마을이 공유(`GRASS_*`).

**대형 맵 2개 (`ZONE_RAID`=5 장애물 / `ZONE_RAID_FLAT`=6 평지, 각 150×150)**: 레이드 필드 겸 **부하·길찾기 측정용**. 두 맵은 **장애물 유무만 다르고** 크기/스폰/몬스터 구성이 동일해야 한다(그래야 성능 차이가 "장애물"이라는 단독 변수에서만 나옴). 블록맵은 손으로 못 찍으므로 **`RaidMap.h`가 고정 시드 LCG로 절차 생성** — `Protocol.h`처럼 **클라/서버 복제본 2벌**이라 한쪽만 고치면 두 맵이 달라져 이동 검증이 깨진다. 생성 결과: 장애물 13.4%, 이동 가능 칸 전부 스폰에서 도달 가능(고립 구역 없음). 스폰 `RAID_SPAWN_WORLD_X/Z`(78.5, 78.5), 주변 반경 8칸은 장애물 없음. 몬스터 수는 **`Zone_Manager.cpp`의 `CFG_MON_COUNT`(기본 0 = 전투 없는 순수 AOI 측정)와 환경변수 `STRESS_MON_COUNT`(0~230, 리빌드 없이 우선 적용)** 가 정한다 — 평지(6)=Wing / 장애물(5)=Orc, 고정 시드로 배치 재현. (`RaidMap.h`에 있던 `RAID_MONSTER_COUNT`는 이 경로에서 한 번도 안 쓰여 오해를 유발해 2026-08-03 삭제했다.) 어그로 `RAID_AGGRO_RANGE`(8타일 — 기본 3이면 A* 경로가 4칸 이하라 길찾기 부하가 측정되지 않음). **진입**: 마을 천사 NPC(25,22) 클릭 → `CUI_ZoneSelect` 선택창 → `CS_PORTAL`(포탈과 동일 경로). 클라 존 클래스 `CZone_Raid`/`CZone_Raid_Flat`(`Zone_Raid.{h,cpp}`).

**아이템 드롭/인벤토리(서버 권위적, Phase 1 완료)**: 몬스터 사망 → 서버 `RollDrop`(`ServerItem.h`) → `CZone::SpawnDrop`(존별 고정 풀 `FDrop m_drops[]`, **new 없음**) → `SC_ADD_DROP` 브로드캐스트. 클라 `CDropItem`(OBJ_DROP)로 월드에 아이콘 렌더. 플레이어가 **드롭에 접촉한 채 좌클릭** → `Object_Manager::Find_DropInContact` → `CS_PICKUP` → 서버 `OnPlayerPickup`(거리검증) → `CPlayer::AddItem/AddGold`(서버 값배열 인벤) → `SC_REMOVE_DROP` + `SC_INVEN_UPDATE`(전체 스냅샷) → 클라 `CInventory::Set_From_Snapshot`로 표시. **아이템 고유코드 = category×1000+subType** (1xxx 포션,2xxx 스크롤,3xxx 장비,4xxx 기타,9000 골드). 클라 팩토리 `CInventory::Create_ItemFromCode`. 존 전환 시 클라가 `DeleteID(OBJ_DROP)`로 옛 드롭 정리(새 존은 `EnterZone`의 `Send_AllDrops`로 재수신). **장비 장착 공/방·포션 사용 회복은 Phase 2 예정**(현재 장비/스탯은 아직 클라 보유).

**장비/스탯/포션(서버 권위, Phase 2 완료)**: 우클릭 장착/사용·퀵슬롯은 클라가 `CS_EQUIP`/`CS_UNEQUIP`/`CS_USE_ITEM`만 전송 → 서버가 인벤/장비/HP·MP/버프 변경 후 `SC_INVEN_UPDATE`(장비 포함)·`SC_PLAYER_HP`로 스냅샷 반환 → 클라 `CInventory::Set_From_Snapshot` + `CEquipment::Set_From_Snapshot`로 표시. 서버 `CPlayer::Get_Atk()=기본10+장비+ATK버프`, `Get_Def()=기본5+장비`. 전투: `OnPlayerAttackMonster` 데미지=`Get_Atk()`, `OnMonsterAttackHit` 피해=`max(1, 몬공−Get_Def())`(무적 시 0). 장비 스탯표는 서버 `ServerItem.h g_EquipTable`(클라 s_EquipTable 39종과 동일해야 함). 포션 효과표 `g_PotionTable`은 클라 s_PotionTable **배열 순서**와 일치(주의: POTION_TYPE enum의 [4]ATK/[5]무적이 클라에서 뒤바뀜 → 배열 인덱스 기준으로 맞춤). 버프 지속시간은 서버 상수(ATK 15s/무적 8s). **버프 쿨타임 UI**: 서버가 `SC_BUFF`(buffType/durationMs)로 사용 확정을 알리면 클라 `CPlayer::Add_Buff`로 자체 타이머, `CUI_HUD::Render_Buffs`가 HP UI 위에 아이콘+검은 오버레이(경과 비율)로 표시. **퀵슬롯은 코드 기반**(`UI_QuickSlot::m_aSlotCode` + `CItemData::Get_ItemCode` + `CPlayer::Use_QuickSlot_ByCode`)이라 인벤 스냅샷 재구성에도 등록 유지(아이템 소진 시만 해제). 몬스터 사망 시 클라가 `MON_DEAD`에서 HP=0 표시.

**레벨/경험치(서버 권위, DB 영속)**: 몬스터 처치 → `CZone::OnPlayerAttackMonster` 사망 분기에서 막타 친 플레이어에게 `CPlayer::AddExp(pMonster->m_nExpReward)` → `SC_PLAYER_EXP`(level/exp/maxExp/levelUp) 전송, 레벨업 시 `SC_PLAYER_HP`도 함께(최대치 변동+풀회복). 클라는 `Handle_SC_PLAYER_EXP`로 받아 표시만 하고(`CGameObject::Set_Exp/Set_Level`), `CUI_HUD`가 EXP 바 + `Lv.N` 렌더. **필요 경험치 = `100 × 현재레벨`** (`CPlayer::ExpToNext`), 만렙 `MAX_LEVEL=50`(도달 시 exp=0 고정, maxExp=0으로 보내 클라는 바를 꽉 채움). 몬스터 보상은 `CMonster::m_nExpReward`(오크 20 / 윙 25, `Monster_Manager::Create`에서 타입별 세팅). **레벨 파생 스탯은 증분이 아니라 `CPlayer::ApplyLevelStats()` 한 곳에서 계산**(MaxHp=100+20n, MaxMp=100+10n, baseAtk=10+2n, baseDef=5+n; n=레벨-1) — 레벨업 때도 DB 로드 때도 같은 식을 쓰므로 값이 어긋나지 않는다. DB는 `character.level/exp` 컬럼(`sp_login`이 반환, 주기 저장 `FSaveSnapshot`에 포함). **DB는 `db/setup.sql` 하나만 실행하면 된다**(신규 설치와 기존 DB 업그레이드를 모두 처리 — 없는 컬럼만 조건부 ALTER).

**퀵슬롯(DB 영속)**: 퀵슬롯은 "표시 전용"이라 서버 권위가 아니다(실제 사용은 `CS_USE_ITEM`이 따로 검증) — 서버는 **등록 내용만 보관했다가 DB에 저장하고 다음 로그인에 돌려준다**. 클라 `CUI_QuickSlot`에서 슬롯이 바뀌는 세 지점(드래그 등록 / 우클릭 해제 / `Update_SlotValidity`의 아이템 소진 자동 해제)은 모두 **`Set_Slot()` 한 곳을 거치며**, 값이 실제로 바뀔 때만 `CS_QUICKSLOT_SET`(slot, itemCode; 0=해제)을 보낸다(매 프레임 도는 유효성 검사의 중복 전송 방지). 서버 `CPlayer::SetQuickSlot`이 슬롯 범위(0~7)와 코드(0 또는 포션1xxx/스크롤2xxx만)를 검증 — 장비를 퀵슬롯에 넣는 잘못된 클라는 거부된다. 로그인 시 `SC_QUICKSLOT_UPDATE`(8칸 스냅샷)를 **반드시 `Send_InvenUpdate` 뒤에** 보낼 것(클라가 등록 코드를 인벤에서 찾아 아이콘을 그리므로 인벤이 먼저 채워져야 함). 클라는 경매장과 같은 **캐시+버전 풀 방식**(`CNetwork_Manager::GetQuickCodes/GetQuickVersion` → `CUI_QuickSlot::Sync_FromServer`)으로 받아 UI를 네트워크 스레드에서 직접 건드리지 않는다. DB는 `quickslot` 테이블(inventory/equipment와 동일 구조, `sp_login` 4번째 결과셋). **DB는 `db/setup.sql` 하나만 실행하면 된다**(퀵슬롯 테이블 + `sp_login` 4결과셋 포함).

**존 전환(포탈)**: 서버 권위적. 클릭→`CS_PORTAL`(targetZone, spawnX/Z) → 서버 `Handle_CS_PORTAL`: `LeaveZone`(옛 존) → `SC_CHANGE_ZONE`(클라가 맵 로드+플레이어 이동) → `EnterZone`(새 존, 새 플레이어/몬스터 add + 옛 몬스터 remove 전송). **adds보다 SC_CHANGE_ZONE을 먼저 보내야** 클라가 새 객체를 지우지 않음. 클라 `Handle_SC_CHANGE_ZONE`은 `Change_Zone_Async` + `Set_WorldPos`만 수행(객체 정리는 서버 remove 패킷에 의존). 비연결 시 포탈은 로컬 전환 폴백.

이동 모델: 서버는 "출발위치 + 방향*속도*경과시간"으로 정확한 위치를 계산(`CPlayer::GetCurrentPos`). 클릭→`CS_MOVE_DEST`, 타일 바뀔 때→`CS_MOVE_POS`(시야 갱신용).

## 서버 아키텍처 (IOCP)

- `CIOCP_Server` — 리슨소켓 + AcceptEx 풀, 워커스레드 풀, 타이머 스레드, 디버그 콘솔 스레드.
  - **디버그 콘솔(2026-08-02 재작성)**: 스레드 시작 시 `ClearConsoleAll()`로 기동 로그를 한 번 지우고 대시보드로 전환. 매 프레임 `ostringstream`에 **프레임 전체를 만들어 한 번에 출력**하며, 줄마다 콘솔 폭까지 공백을 채우므로 **줄 끝에 공백을 손으로 붙일 필요가 없다**(잔상 방지). 줄 수는 항상 고정. 제목에 `GetServerConfigTag()`를 찍어 **어느 빌드의 측정치인지** 남긴다.
  - **⚠️ 접속 수는 반드시 `CSession_Manager::GetConnectedCount()`(O(1) 원자 카운터)를 쓸 것.** 예전엔 `m_sessions` 20000칸을 매 0.5초 **두 번** 순회하며 `Get_Session`을 4만 번 호출했는데, 그게 곧 **측정 대상인 `m_lock`을 초당 8만 번 잡는 것**이라 부하 측정을 오염시켰다. 카운터 증감 지점은 `ProcessAccept`(`SetConnected(true)` 직후)와 `CSession::Disconnect`(`exchange` 통과 직후) 두 곳뿐. 대기 수 = `GetCount() - GetConnectedCount()`.
  - `m_acceptSessions` 벡터는 **제거됨**. 세션 소유자는 `CSession_Manager::m_sessions` 하나뿐이며, 이 벡터는 초기 64개 세션을 영구 참조해 소멸을 막던 누수였다. (대기 세션은 `m_connected==false`라 `Disconnect`가 즉시 리턴 → `Release`까지 안 가므로, **AcceptEx가 걸린 동안 세션이 파괴되는 일은 구조적으로 불가능**하다.)
- `CSession` (`SessionRef=shared_ptr`) — 소켓당 세션. `CIOEvent`(WSAOVERLAPPED 확장, `IOType`으로 Accept/Recv/Send/MonsterAI/MonsterRespawn/MonsterAttackHit 구분). recv 링버퍼 + send 락.
- `CPacket_Handler::Handle` — 패킷 ID로 디스패치(static). `Handle_CS_*` / `Send_SC_*`.
- 매니저 싱글턴: `CSession_Manager`(세션ID 발급/관리), `CZone_Manager`(zoneID→`CZone*`), `CPlayer_Manager`, `CMonster_Manager`.
- `CZone` — 맵 한 칸. 타일맵 보유, 플레이어/몬스터 ID 집합, **시야처리**(VIEW_RANGE=5, `GetNearPlayers`/`CanSee`/`UpdateViewAndBroadcast`), 이동 브로드캐스트, **몬스터 AI**(Chase/Attack/Patrol 상태머신, A*/Theta* 길찾기 `PathFinder`), 전투(`OnPlayerAttackMonster`, 타이머 기반 공격모션→Hit 분기). **몬스터 타입별 이동**: `MONSTER_ORC`은 `PathFinder` 길찾기, `MONSTER_WING`(부유 몬스터)은 `Monster_Chase`에서 `m_eType==MONSTER_WING` 분기로 길찾기 없이 플레이어를 향해 **직선 이동(장애물 무시)**. 스폰은 `Zone_Manager.cpp::SpawnRandomMonsters(zone,id,cnt,type)`로 각 필드에 오크+윙 혼합.
- `CPlayer`(서버) — 위치/이동/HP/시야리스트(`m_viewList`, `m_monsterViewList` + 각 mutex)/공격쿨다운/사망플래그.
- `CMonster` — 상태(IDLE/WALK/ATTACK_0/1/HIT/DEAD), 어그로/공격 사거리, 공격쿨, hit delay, 타겟ID.
- `CTimer` — IOCP에 타이머 이벤트를 던져 워커가 처리(공격 모션 후 실제 Hit, 리스폰, AI 틱).

## 클라이언트 아키텍처 (Direct2D)

- 게임루프: `Game.cpp` WinMain → 144FPS 고정, `CTimer_Manager`로 dt 계산 → `CMainApp::Update/Late_Update/Render`. D2D는 **싱글스레드**.
- 매니저 싱글턴 패턴(`Get_Instance`/`Destroy_Instance`): `CLevel_Manager`, `CObject_Manager`, `CUI_Manager`, `CImg_Manager`, `CInput_Manager`, `CMap_Manager`, `CCollision_Manager`, `CTimer_Manager`, `CNetwork_Manager`.
- **레벨**: `CLevel` 기반 — `LEVEL_MENU/LOGIN/CHOICE/TEST`. `CLevel_Manager::Level_Change`.
- **오브젝트**: `CGameObject`(추상, 순수가상 Initialize/Update/Late_Update/Render/Release) 기반. `OBJ_ID` = PORTAL/PLAYER/OTHER_PLAYER/NPC/MONSTER. `CObject_Manager`가 `m_ObjectList[OBJ_END]` 리스트로 관리, `Find_OtherPlayer(id)`/`Find_Monster(id)`/`Pick_Monster(mouse)`.
- 주요 오브젝트: `Player`, `Other_Player`, `Monster`/`Monster_Orc`/`Monster_Wing`, `NPC`/`NPC_Shop`/`NPC_Market`/`NPC_OldMan`/`NPC_Knight`/`NPC_Angel`, `Portal`. 몬스터 타입은 `MONSTER_TYPE`(클라 `define.h` / 서버 `Monster.h`, 숫자 일치: ORC=0,WING=1)로 구분하고 `SC_ADD_MONSTER_PACKET.monsterType`으로 전달 → 클라 `Handle_SC_ADD_MONSTER`가 타입별 서브클래스 생성. 스프라이트는 `Resource/Monster/<이름>/action(WxHxframesxdirs).png` 규칙(방향행 순서 D,LB,L,LT,T,RT,R,RB), 마젠타 배경은 투명 처리. `Monster_Wing`은 오크와 동일 모션 세트에 부유 높이(HoverHeight)만 다름. 아이소 좌표는 `ISO_INFO{fWorldX,fWorldZ,fHeight}`, 충돌 `COLLIDER`, 마우스픽 `MOUSE_COLLIDER`.
- **네트워크**: `CNetwork_Manager` — recv 전용 스레드가 패킷을 파싱해 `std::function` task로 큐에 push, 메인스레드가 `Dispatch()`로 pop 실행(D2D 싱글스레드 안전성 확보). `Handle_SC_*` 핸들러들.
- **NPC**: `CNPC` 상속. 이동 없음, 플레이어 근접(`On_Collision`) 시 좌클릭 `On_Click`. 스프라이트는 **가로 1줄 스트립**(투명 배경, `Render_Sprite`가 `fCX`폭으로 슬라이스), `Motion_Change`에서 `Set_Frame(끝idx,ms)`+`m_bLoopAnim`. 마을 전용(`Zone_Town::Spawn_Objects`에서 `Insert_Png`+생성). 장식/대화 NPC는 타입별 개별 클래스(추후 역할 확장): `NPC_OldMan`(Idle만), `NPC_Knight`(Idle+Talk=Special, 클릭 시 Talk 1회 후 Idle 복귀), `NPC_Angel`(Idle+Effect, 클릭 시 NPC 뒤에 이펙트를 독립 프레임으로 표시). 리소스는 `Resource/NPC/<이름>/<이름>_Idle|Talk|Effect.png`(배경 원본: OldMan 흰색/Knight 마젠타/Angel 마젠타+이펙트 검정 → 투명 처리). 클릭 시 특별 창 없이 모션/말풍선만.
- **UI**: `CUI`(`UI_BUTTON/INVENTORY/QUICKSLOT/HUD/BOX`), `CUI_Manager`. HUD/퀵슬롯/인벤토리/로그인박스 완성.
- **아이템**: `CItemData` 기반 상속(Equipment/Potion/Scroll/Etc/UseItem), `Inventory`, `Equipment`.
- 이미지: `MyBmp`/`MyPng`, `CImg_Manager`.

## 좌표/타일 상수 (클라 `define.h`)

- 창 1280×720. 타일 `TILE_WIDTH=160`, `TILE_HEIGHT=80`(아이소 2:1). 맵 `MAP_TILE_X/Z=20`.
- 방향 8방위 `DIRECTION{DIR_B,LB,L,LT,T,RT,R,RB}` — 서버 `MONSTER_DIR`와 순서 일치.

## 현재 진행 상황 (git Develop 브랜치 기준)

완료: 클라 프레임워크, 플레이어/오브젝트/NPC/몬스터, UI/인벤토리/HUD/퀵슬롯, 멀티스레드 로딩, 로그인/이동 서버연동(보간+해킹방지 검증), A*/Theta* 길찾기, 몬스터 AI+애니메이션 연동, 시야(AOI), **전투(사망/리스폰)**, **다중 존 + 포탈**, **아이템 드롭/인벤/장비/버프**, **상점**, **경매장(DB 정본, 원자적 조건부 UPDATE로 동시구매 방지)**, **DB 영속화(sp_login 결과셋 4개 + 트랜잭션 저장 + 5초 주기 자동저장)**, **레벨/경험치**, **퀵슬롯 DB 연동**, **대형 맵 2종(150×150, 장애물/평지)**.

**거래 로그 / 백섭 (진행 중)**: 설계·시나리오 전문은 `DB_AND_LOG_GUIDE.md`. 요지 — 로그는 게임 DB(`mmorpg`)가 아니라 **별도 DB `mmorpg_log`** 에 쌓는다(백섭이 `mmorpg`만 되돌리므로 로그가 살아남아 보상 산정이 가능). 시나리오는 두 개이고 **탐지 방법이 서로 다르다**: ①단독 악용자 → 집계 이상치로 탐지 → **선별 환수(백섭 안 함)**, ②복제 버그 확산 → `gold_balance` 연속성 위반으로 탐지 → **전면 백섭(PITR)+보상**. ⚠️ **어느 쪽이든 0단계는 서버 셧다운** — `AUTOSAVE_INTERVAL_MS=5000` 주기 저장이 메모리 상태로 DB를 5초마다 덮으므로, 서버가 켜진 채 DB를 고치면 환수든 백섭이든 조용히 원복된다.
- **B단계 완료(로그 통로)**: `GameLog.h`(`FGameLog`/`LogType::`/`MakeGameLog()`, UTF-8 BOM) + `CDB_Manager::EnqueueLog()`/`LogThreadFunc()`. 저장 스레드와 같은 큐+전용스레드 구조지만 **100건(`LOG_BATCH_MAX`) 또는 1초(`LOG_FLUSH_MS`) 단위 다중행 INSERT**로 왕복을 줄인다. 값은 전부 `?` 바인딩(800개), 행 수 바뀔 때만 재prepare. 큐 상한 `LOG_QUEUE_MAX`+유실 카운터, `affected_rows` 검증, **로그 실패 메시지는 `SLOG`가 아니라 `std::cout`**(STRESS_TEST 빌드에서도 묻히면 안 됨). `StopLogThread()`는 `StopSaveThread()` **뒤에** 호출. `detail`에 한글 금지(리터럴이 CP949로 컴파일되는데 로그 커넥션은 utf8mb4).
- **C단계 완료(로그 심기, 8지점)**: 경매 4핸들러(등록/구매/수령/취소, 구매는 구매자 `AUCTION_BUY`+판매자 `AUCTION_SOLD` 2건) + `Handle_CS_BUY`/`Handle_CS_SELL` + `CZone::OnPlayerPickup`(`DROP_GAIN`) + `Handle_CS_USE_ITEM`(`ITEM_USE`). **로그는 DB 변경/지급이 성공한 뒤에만** 남긴다.
  - **잔액 정확성**: `CPlayer::AddGold/SpendGold`에 `int32_t* pOutBalance` 추가(기본 nullptr) + `GetGold()` 신설. 바꾸고 나서 `m_gold`를 따로 읽으면 그 사이 다른 스레드 변경으로 로그 잔액이 어긋나고, 그게 연속성 검사에서 **복제 버그로 오탐**된다.
  - `Auction_PeekBuy`에 `std::string* outSeller` 추가(`AUCTION_SOLD`의 actor). ⚠️ 이 판매자명은 **DB 그대로의 UTF-8**을 써야 한다 — 클라 전송용 `Utf8ToCp949`를 로그 경로에 쓰면 로그 DB(utf8mb4)에서 깨진다.
  - `AUCTION_SOLD`만 수량/골드/잔액이 전부 0(판매자가 오프라인일 수 있고 대금은 pending). 목적은 수치가 아니라 `target`(거래 상대) — **연속성 검사 쿼리는 반드시 `gold <> 0` 필터를 걸 것.**
  - `ITEM_USE`는 원래 계획에 없었는데 전수 확인 중 `CPlayer::UseItem`이 `RemoveItemSlot`을 안 거치고 배열을 직접 줄이는 걸 발견해 추가(아이템이 로그 없이 사라지는 유일한 경로였음). `FUseResult`에 `itemCode` 필드 추가.
  - 로그인 시 DB→메모리 복원은 **의도적으로 로그 없음**(재화 이동이 아니라 상태 복원).
  - **드롭 출처 몬스터 기록(2026-08-04)**: `FDrop`에 `srcMon`(MONSTER_TYPE, -1=몬스터 아님) 추가, `SpawnDrop(..., nSrcMon=-1)`으로 몬스터 사망 시 타입 전달 → `OnPlayerPickup`이 `DROP_GAIN` 로그의 `detail`에 `mon=orc`/`mon=wing`으로 남긴다. **줍는 시점엔 몬스터가 이미 사라진 뒤라 드롭이 출처를 들고 있어야 한다.** 이름 변환은 `Monster.h::MonsterTypeName()`(ASCII만 — detail 규칙). 이게 없으면 이상치 탐지가 "이 유저가 이상하다"에서 멈추고 원인 몬스터를 못 짚는다.
  - **드롭 획득 인벤 가득 버그 수정(2026-08-04)**: `OnPlayerPickup`이 `d.active=false`로 **선점한 뒤** `AddItem`을 불러서, 인벤이 가득 차면 아이템이 조용히 증발했다. 되살리기는 그 사이 슬롯이 새 드롭에 재사용될 수 있어 위험 → **선점 전에 `CanAddItem`으로 막는 방식**으로 고침(골드는 인벤을 안 쓰므로 통과). 드롭은 바닥에 그대로 남는다. 락 순서는 `m_dropLock` → `m_saveLock`(역순 경로 없음). 클라는 드롭 호버 시 `CDropItem::Render_HoverName`이 이름 위에 빨간 "인벤 가득참" 한 줄 추가 — 판정은 신설 `CInventory::Can_Add_Code(code)`가 하며 **서버 `CPlayer::CanAddItem`과 규칙이 같아야 한다**(스택 가능=1xxx/2xxx/4xxx, 코드로 비교 — 클라 기존 `Find_SameItem`은 Type+이름 비교라 기준이 달라서 안 씀). 클라는 표시만 하고 요청은 그대로 보낸다(거부는 서버 권위).
  - **실게임 검증 완료(2026-08-04)**: 직접 플레이(드롭 획득/포션 사용/상점 구매/경매 등록)로 남은 로그 8건이 DB 최종 상태와 정확히 일치 — 골드 `1000 +11 +17 -60 = 968` = `character.gold`, 인벤 4종도 시드+로그 재구성값과 동일, `gold_balance` 연속성 위반 0건. 자가진단으로 넣었던 로그 764건은 삭제함.
- **계정 3개**: `mmo_server`(게임 DB 읽기/쓰기 + 로그 DB **INSERT만**) / **`mmo_analyst`(둘 다 SELECT만 - 탐지·보상 산정용)** / `root`(환수 UPDATE, PITR, `setup.sql` 실행). 조사할 때 root를 쓰면 실수로 UPDATE가 나갈 수 있어 분석은 `mmo_analyst`로 한다.
- **D단계 진행 중**: 백업 **완료**(처음엔 `db/backup.ps1`, 지금은 `MMO_AdminTool` 메뉴 6 — 위 디렉터리 절 참고).
  - **`sp_rollback_account(계정, 시점, 적용여부)` 완료**(`setup.sql` 5장). 시나리오 1(선별 환수)의 실행 도구. **백업이 필요 없다** — 백업본을 기준점으로 불러오는 대신 **현재 상태에서 그 시점 이후 로그를 거꾸로 되감는다**(`되돌릴 값 = 지금 값 - 이후 로그 증감 합계`). 골드는 `gold_balance`를 읽는 방법과 증감을 빼는 방법 **두 가지로 계산해 대조**하고, 다르면 `MISMATCH - unlogged gold path exists`로 알려준다(도구가 곧 무결성 검사). `p_apply=0`이면 미리보기만. 아이템 개수 음수/인벤 40칸 초과면 `SIGNAL`로 중단. 해당 시점 이후 등록된 경매 매물은 함께 내린다(아이템은 로그 델타로 이미 인벤에 복원됨).
  - ⚠️ **환수 자체도 `ADMIN_ROLLBACK`으로 로그를 남긴다.** 안 남기면 다음 거래 때 `직전잔액+증감 != 기록된잔액`이 되어 연속성 검사가 **환수를 복제 버그로 오탐**한다(실제로 대조 확인함). 장비창/레벨/위치는 대상 아님(장착은 재화 이동이 아니라 로그가 없음).
  - **실데이터 시연 완료(2026-08-04)**: test1을 13:47:30으로 되돌림 → 골드 968→1028(두 방법 일치), `1000` 7→5, 경매 등록했던 `3001`이 인벤으로 복귀 + 매물 #25 내려감, 7월에 올린 매물 #10은 그대로. 이후 `backup.ps1` 백업본으로 원상복구까지 확인(로그 9건은 살아남음 = `mmorpg`만 복구하는 설계가 실제로 동작).
  - ⚠️ **복구 명령 함정**: `mysql`의 `source`는 **한글 경로 파일을 못 연다**(`Failed to open file, error: 2`) — 이 프로젝트 경로에 `문서`가 들어 있어 그대로 걸린다. `cmd`의 `< 리다이렉션`은 한글 경로도 정상(PowerShell은 `<` 자체를 지원 안 함 → `cmd /c` 경유). 생성되는 `*.info.txt`에 이 내용이 다 적힌다.
  - 남은 것 = 탐지 쿼리(집계 이상치) + 보상 산정 쿼리 + PITR 실전 리허설 + 시연용 사고 주입 토글. ⚠️ 환수/binlog 작업은 **root 필요**(조회는 `mmo_analyst`로 가능). binlog 폴더(`C:\ProgramData\MySQL\MySQL Server 8.4\Data`)는 MySQL 서비스 계정 소유라 **관리자 권한 PowerShell**이어야 읽힌다.

**포트폴리오 문서**: `PROJECT_HISTORY.md` — **"어떤 문제를 발견해 어떻게 고쳐왔나"를 시간순 에피소드 20개로 정리한 것**(2026-08-04 신설). 기능 목록이 아니라 문제/진단/해결/교훈 형식이라 면접 답변의 뼈대로 쓸 것. 7장에 다음 작업 로드맵(송신 경로 최적화 → 측정 → 표 채우기)도 들어 있다. (예전에 여기 적혀 있던 `PORTFOLIO.md`/`PORTFOLIO_Server.md`는 **실제로 존재한 적이 없다** — git 이력에도 없음. 잘못된 기록이었음.)

**다음 목표 — 부하 테스트와 최적화** (기능 추가보다 이쪽이 기술적으로 남는 게 큼):
1. **부하 봇 + 서버 계측** ← 여기부터. 봇은 C++로(파이썬은 부하가 부족), 서버에 패킷 처리 지연(평균/p99)·락 대기·워커별 처리 건수를 심어 디버그 콘솔에 출력.
2. 시나리오 A(존별 분산 = 순수 I/O 한계) / B(**한 존 밀집** = 월드 로직 한계, 이쪽이 핵심) / C(전투+DB 부하).
3. 예상 병목 두 개: **`CZone::GetNearPlayers`가 존 전체를 O(N) 순회**(타일 바뀔 때마다 → 초당 O(N²)), **DB 저장이 IOCP 워커를 블로킹**(connection-per-call).
4. 개선 → 재측정: **섹터(공간 분할) AOI**(**구현 완료** — `USE_SECTOR_AOI=1`, 시야 사각형 기준 범위 스캔. 위 "최적화 토글" 절 참고), **전용 DB 스레드 + 커넥션 풀**(**구현 완료**), **A* 개선**(직선 시야 조기반환 — 지금은 평지에서도 A*를 그대로 돌린다 / 매 호출 힙 할당 제거).
   - 대형 맵 2종(장애물/평지)이 A* 개선의 before/after 비교군이다.
   - `MAX_SESSION`(`Session_Manager.h`)/`MAX_PLAYER`(`Player_Manager.h`) = **20000** (둘 다 고정 크기 `std::array`).

다음 예정(ProjectPlane.md): 일일/주간 퀘스트, 경매장, 채널 구조, DB 연동(캐릭터/인벤/스탯/로그인).
