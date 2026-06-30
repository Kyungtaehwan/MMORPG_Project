# MMORPG_Project — 프로젝트 가이드

2D 아이소메트릭 MMORPG. **클라이언트(Direct2D, Win32)** + **게임서버(IOCP)** 두 개의 Visual Studio 솔루션으로 구성. C++17, PlatformToolset v143, x64, CharacterSet=Unicode.

> 이 문서는 매 세션 자동 로드되어 전체 코드를 다시 훑지 않고도 구조를 파악하기 위한 것. 기능 추가 시 먼저 여기서 관련 파일을 찾고, 변경 후 구조가 바뀌면 이 문서도 갱신할 것.

## 디렉터리

- `MMO_Client/MMO_Client/` — 클라이언트 소스. `MMO_Client.sln` 으로 빌드. 엔트리: `Game.cpp`(WinMain).
- `MMO_GameServer/MMO_GameServer/` — 서버 소스. `MMO_GameServer.sln`. 엔트리: `MainServer.cpp`(main). 포트 **7777**.
- `ProjectPlane.md` — 주차별 개발 계획/진행 로그.
- 빌드 산출물(`x64/`, `.vs/`)은 무시.

## ⚠️ 반드시 지킬 규칙 (gotchas)

1. **인코딩 함정(매우 중요)**: 대부분의 소스는 **CP949(BOM 없음)**로 저장돼 있다 (그래서 Read 도구로 보면 한글 주석이 mojibake로 보임). 이런 CP949 파일을 **Edit/Write 도구로 수정하면 UTF-8로 재저장되어 기존 한글이 전부 깨진다**(주석·`L"..."` 문자열 포함). 따라서 한글이 든 파일을 편집하기 전, 먼저 **UTF-8+BOM으로 변환**할 것:
   - 손상됐으면 `git checkout HEAD -- <file>`로 원본(CP949) 복원 → PowerShell로 `GetEncoding(949)`로 읽어 `UTF8Encoding($true)`(BOM)로 다시 써서 변환 → 그 다음 Edit 사용.
   - 편집 후 BOM이 유지됐는지 확인(Edit가 BOM을 떼면 다시 CP949로 오인됨). UTF-8+BOM이면 MSVC가 로케일 무관하게 UTF-8로 읽으므로 안전.
   - **이미 UTF-8+BOM으로 변환 완료된 파일**(자유롭게 편집 가능): 양쪽 `Protocol.h`, 서버 `Zone.cpp`/`Zone_Manager.cpp`/`Packet_Handler.{h,cpp}`, 클라 `Network_Manager.{h,cpp}`/`Map_Manager.cpp`/`Portal.cpp`/`Zone_Test.cpp`/`Zone_Town.cpp`. 나머지 파일은 아직 CP949이므로 편집 전 변환 필요.
2. **`Protocol.h`는 클라/서버 양쪽에 복제본이 있다.** (`MMO_Client/.../Protocol.h`, `MMO_GameServer/.../Protocol.h`) 패킷 구조를 바꾸면 **양쪽 파일을 동시에 똑같이** 수정해야 한다. 한쪽만 고치면 직렬화가 깨진다. `#pragma pack(push,1)` 유지 필수.
3. 클라 `define.h`에 `#define NO_SERVER`가 있음 — 오프라인 단독 테스트 모드 토글. 서버 연동 동작을 건드릴 땐 이 매크로 분기 확인.
4. enum 값은 클라/서버가 **숫자까지 일치**해야 함 (예: `PLAYER_STATE`, `MONSTER_STATE`, `DIRECTION/MONSTER_DIR`, `TILE_TYPE`). 단, `TILE_TYPE`은 클라(`TILE_BLOCK=10`)와 서버(`TILE_BLOCK=1`)의 숫자가 다르니 주의 — 타일 동기화 시 매핑 확인.

## 네트워크 프로토콜 (핵심 계약)

`Protocol.h` — `PacketHeader{ uint16 size; uint16 id; }` 뒤에 페이로드. ID 규칙:
- `CS_*` = 1000번대 (Client→Server): LOGIN, MOVE_DEST(목적지 클릭), MOVE_POS(현재위치 갱신), ATTACK_MONSTER, RESPAWN, PORTAL(존 이동 요청).
- `SC_*` = 2000번대 플레이어, 2100번대 몬스터: LOGIN_OK/FAIL, ENTER_GAME, ADD/REMOVE/MOVE_PLAYER, PLAYER_STATE/HIT, RESPAWN, CHANGE_ZONE(존 전환 확정) / ADD/REMOVE/MOVE_MONSTER, MONSTER_STATE/HIT.

**월드 구성(존)**: 허브인 **마을(ZONE_TOWN)** 중심으로 동서남북 4개 몬스터 필드 — 북=`ZONE_TEST`(기존), 동=`ZONE_FIELD_E`, 남=`ZONE_FIELD_S`, 서=`ZONE_FIELD_W`. 모든 필드 20×30, 테두리 동일, 장애물 배치만 다름. 마을 4모서리(N(6,6)/E(19,6)/S(19,29)/W(6,29))에 포탈, 각 필드의 복귀 포탈은 마을 반대편에 위치(동쪽 필드면 서쪽). 몬스터는 서버가 각 필드에 랜덤 배치(`Zone_Manager.cpp::SpawnRandomMonsters`). 클라 필드 존 클래스: `CZone_Test`(북), `CZone_Field_E/S/W`(개별 클래스, 각 .h/.cpp + vcxproj 등록됨). **각 필드 블록맵은 클라(`CZone_*::Build`)와 서버(`BLOCK_MAP_FIELD_*`)가 반드시 동일해야 함**(이동 검증). 좌표: 내부 grass X 3..22, Z 3..32. 로그인 시작 존은 ZONE_TEST(북).

**아이템 드롭/인벤토리(서버 권위적, Phase 1 완료)**: 몬스터 사망 → 서버 `RollDrop`(`ServerItem.h`) → `CZone::SpawnDrop`(존별 고정 풀 `FDrop m_drops[]`, **new 없음**) → `SC_ADD_DROP` 브로드캐스트. 클라 `CDropItem`(OBJ_DROP)로 월드에 아이콘 렌더. 플레이어가 **드롭에 접촉한 채 좌클릭** → `Object_Manager::Find_DropInContact` → `CS_PICKUP` → 서버 `OnPlayerPickup`(거리검증) → `CPlayer::AddItem/AddGold`(서버 값배열 인벤) → `SC_REMOVE_DROP` + `SC_INVEN_UPDATE`(전체 스냅샷) → 클라 `CInventory::Set_From_Snapshot`로 표시. **아이템 고유코드 = category×1000+subType** (1xxx 포션,2xxx 스크롤,3xxx 장비,4xxx 기타,9000 골드). 클라 팩토리 `CInventory::Create_ItemFromCode`. 존 전환 시 클라가 `DeleteID(OBJ_DROP)`로 옛 드롭 정리(새 존은 `EnterZone`의 `Send_AllDrops`로 재수신). **장비 장착 공/방·포션 사용 회복은 Phase 2 예정**(현재 장비/스탯은 아직 클라 보유).

**장비/스탯/포션(서버 권위, Phase 2 완료)**: 우클릭 장착/사용·퀵슬롯은 클라가 `CS_EQUIP`/`CS_UNEQUIP`/`CS_USE_ITEM`만 전송 → 서버가 인벤/장비/HP·MP/버프 변경 후 `SC_INVEN_UPDATE`(장비 포함)·`SC_PLAYER_HP`로 스냅샷 반환 → 클라 `CInventory::Set_From_Snapshot` + `CEquipment::Set_From_Snapshot`로 표시. 서버 `CPlayer::Get_Atk()=기본10+장비+ATK버프`, `Get_Def()=기본5+장비`. 전투: `OnPlayerAttackMonster` 데미지=`Get_Atk()`, `OnMonsterAttackHit` 피해=`max(1, 몬공−Get_Def())`(무적 시 0). 장비 스탯표는 서버 `ServerItem.h g_EquipTable`(클라 s_EquipTable 39종과 동일해야 함). 포션 효과표 `g_PotionTable`은 클라 s_PotionTable **배열 순서**와 일치(주의: POTION_TYPE enum의 [4]ATK/[5]무적이 클라에서 뒤바뀜 → 배열 인덱스 기준으로 맞춤). 버프 지속시간은 서버 상수(ATK 15s/무적 8s). **버프 쿨타임 UI**: 서버가 `SC_BUFF`(buffType/durationMs)로 사용 확정을 알리면 클라 `CPlayer::Add_Buff`로 자체 타이머, `CUI_HUD::Render_Buffs`가 HP UI 위에 아이콘+검은 오버레이(경과 비율)로 표시. **퀵슬롯은 코드 기반**(`UI_QuickSlot::m_aSlotCode` + `CItemData::Get_ItemCode` + `CPlayer::Use_QuickSlot_ByCode`)이라 인벤 스냅샷 재구성에도 등록 유지(아이템 소진 시만 해제). 몬스터 사망 시 클라가 `MON_DEAD`에서 HP=0 표시.

**존 전환(포탈)**: 서버 권위적. 클릭→`CS_PORTAL`(targetZone, spawnX/Z) → 서버 `Handle_CS_PORTAL`: `LeaveZone`(옛 존) → `SC_CHANGE_ZONE`(클라가 맵 로드+플레이어 이동) → `EnterZone`(새 존, 새 플레이어/몬스터 add + 옛 몬스터 remove 전송). **adds보다 SC_CHANGE_ZONE을 먼저 보내야** 클라가 새 객체를 지우지 않음. 클라 `Handle_SC_CHANGE_ZONE`은 `Change_Zone_Async` + `Set_WorldPos`만 수행(객체 정리는 서버 remove 패킷에 의존). 비연결 시 포탈은 로컬 전환 폴백.

이동 모델: 서버는 "출발위치 + 방향*속도*경과시간"으로 정확한 위치를 계산(`CPlayer::GetCurrentPos`). 클릭→`CS_MOVE_DEST`, 타일 바뀔 때→`CS_MOVE_POS`(시야 갱신용).

## 서버 아키텍처 (IOCP)

- `CIOCP_Server` — 리슨소켓 + AcceptEx 풀, 워커스레드 풀, 타이머 스레드, 디버그 콘솔 스레드.
- `CSession` (`SessionRef=shared_ptr`) — 소켓당 세션. `CIOEvent`(WSAOVERLAPPED 확장, `IOType`으로 Accept/Recv/Send/MonsterAI/MonsterRespawn/MonsterAttackHit 구분). recv 링버퍼 + send 락.
- `CPacket_Handler::Handle` — 패킷 ID로 디스패치(static). `Handle_CS_*` / `Send_SC_*`.
- 매니저 싱글턴: `CSession_Manager`(세션ID 발급/관리), `CZone_Manager`(zoneID→`CZone*`), `CPlayer_Manager`, `CMonster_Manager`.
- `CZone` — 맵 한 칸. 타일맵 보유, 플레이어/몬스터 ID 집합, **시야처리**(VIEW_RANGE=5, `GetNearPlayers`/`CanSee`/`UpdateViewAndBroadcast`), 이동 브로드캐스트, **몬스터 AI**(Chase/Attack/Patrol 상태머신, A*/Theta* 길찾기 `PathFinder`), 전투(`OnPlayerAttackMonster`, 타이머 기반 공격모션→Hit 분기).
- `CPlayer`(서버) — 위치/이동/HP/시야리스트(`m_viewList`, `m_monsterViewList` + 각 mutex)/공격쿨다운/사망플래그.
- `CMonster` — 상태(IDLE/WALK/ATTACK_0/1/HIT/DEAD), 어그로/공격 사거리, 공격쿨, hit delay, 타겟ID.
- `CTimer` — IOCP에 타이머 이벤트를 던져 워커가 처리(공격 모션 후 실제 Hit, 리스폰, AI 틱).

## 클라이언트 아키텍처 (Direct2D)

- 게임루프: `Game.cpp` WinMain → 144FPS 고정, `CTimer_Manager`로 dt 계산 → `CMainApp::Update/Late_Update/Render`. D2D는 **싱글스레드**.
- 매니저 싱글턴 패턴(`Get_Instance`/`Destroy_Instance`): `CLevel_Manager`, `CObject_Manager`, `CUI_Manager`, `CImg_Manager`, `CInput_Manager`, `CMap_Manager`, `CCollision_Manager`, `CTimer_Manager`, `CNetwork_Manager`.
- **레벨**: `CLevel` 기반 — `LEVEL_MENU/LOGIN/CHOICE/TEST`. `CLevel_Manager::Level_Change`.
- **오브젝트**: `CGameObject`(추상, 순수가상 Initialize/Update/Late_Update/Render/Release) 기반. `OBJ_ID` = PORTAL/PLAYER/OTHER_PLAYER/NPC/MONSTER. `CObject_Manager`가 `m_ObjectList[OBJ_END]` 리스트로 관리, `Find_OtherPlayer(id)`/`Find_Monster(id)`/`Pick_Monster(mouse)`.
- 주요 오브젝트: `Player`, `Other_Player`, `Monster`/`Monster_Orc`, `NPC`/`NPC_Shop`, `Portal`. 아이소 좌표는 `ISO_INFO{fWorldX,fWorldZ,fHeight}`, 충돌 `COLLIDER`, 마우스픽 `MOUSE_COLLIDER`.
- **네트워크**: `CNetwork_Manager` — recv 전용 스레드가 패킷을 파싱해 `std::function` task로 큐에 push, 메인스레드가 `Dispatch()`로 pop 실행(D2D 싱글스레드 안전성 확보). `Handle_SC_*` 핸들러들.
- **UI**: `CUI`(`UI_BUTTON/INVENTORY/QUICKSLOT/HUD/BOX`), `CUI_Manager`. HUD/퀵슬롯/인벤토리/로그인박스 완성.
- **아이템**: `CItemData` 기반 상속(Equipment/Potion/Scroll/Etc/UseItem), `Inventory`, `Equipment`.
- 이미지: `MyBmp`/`MyPng`, `CImg_Manager`.

## 좌표/타일 상수 (클라 `define.h`)

- 창 1280×720. 타일 `TILE_WIDTH=160`, `TILE_HEIGHT=80`(아이소 2:1). 맵 `MAP_TILE_X/Z=20`.
- 방향 8방위 `DIRECTION{DIR_B,LB,L,LT,T,RT,R,RB}` — 서버 `MONSTER_DIR`와 순서 일치.

## 현재 진행 상황 (git Develop 브랜치 기준)

완료: 클라 프레임워크, 플레이어/오브젝트/NPC/몬스터 싱글, UI/인벤토리/HUD/퀵슬롯, 멀티스레드 로딩, 로그인/이동 서버연동(보간+해킹방지 검증), A*/Theta* 길찾기, 몬스터 AI+애니메이션 연동, 시야처리, **플레이어↔몬스터 전투 시스템(사망/리스폰 포함)**, **다중 존(ZONE_TEST 몬스터필드 / ZONE_TOWN 마을) + 포탈 존 전환**. 마을=테두리만 장애물 없음, NPC는 마을 전용.

다음 예정(ProjectPlane.md): 일일/주간 퀘스트, 경매장, 채널 구조, DB 연동(캐릭터/인벤/스탯/로그인).
