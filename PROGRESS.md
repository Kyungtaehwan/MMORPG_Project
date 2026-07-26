# 진행 기록 (Phase 1~2 완료, Phase 3 예정)

> 작성: 2026-06-30. 다음 세션(Phase 3) 이어가기용 핸드오프 문서.
> 상세 구조/규칙은 루트 `CLAUDE.md` 참고. 이 문서는 "무엇을 했고 다음에 뭘 할지" 요약.

## 현재 상태
- **서버(`MMO_GameServer`) / 클라(`MMO_Client`) 둘 다 빌드 성공(Debug x64, exit 0).**
- 모든 신규/편집 소스는 **UTF-8 + BOM**으로 정리됨(한글 안 깨짐).
- 실행: 서버 먼저(포트 7777) → 클라 실행. 로그인 시작 존은 북쪽 필드(ZONE_TEST).

---

## 완료한 작업

### A. 다중 존 + 포탈 (맵 디자인)
- 마을(ZONE_TOWN) 허브 + 동서남북 4개 몬스터 필드(북=ZONE_TEST 기존, 동/남/서=`CZone_Field_E/S/W` 개별 클래스).
- 마을 4모서리 포탈 → 각 필드, 각 필드 복귀 포탈은 마을 반대편. 서버 권위 존 전환(`CS_PORTAL`/`SC_CHANGE_ZONE`).
- 몬스터는 서버가 각 필드에 랜덤 배치(`Zone_Manager.cpp::SpawnRandomMonsters`).
- **필드 블록맵은 클라(`CZone_*::Build`)와 서버(`BLOCK_MAP_FIELD_*`)가 반드시 동일**해야 함(이동 검증).

### B. 아이템 드롭 & 획득 (Phase 1, 서버 권위)
- 몬스터 사망 → 서버 `RollDrop`(`ServerItem.h`) → `CZone::SpawnDrop`(존별 고정 풀, **new 없음**) → `SC_ADD_DROP`.
- 클라 `CDropItem`(OBJ_DROP)로 월드에 아이콘 렌더. **접촉+좌클릭** → `CS_PICKUP` → 서버 거리검증 → `CPlayer::AddItem/AddGold` → `SC_REMOVE_DROP` + `SC_INVEN_UPDATE`(전체 스냅샷) → 클라 `CInventory::Set_From_Snapshot` 표시.
- 드롭 렌더: 바닥(콜라이더 중심) 정렬, GAME_DEBUG 히트박스, 마우스 호버 시 이름 표시.

### C. 장비 / 스탯 / 포션 / 버프 (Phase 2, 서버 권위)
- 우클릭 장착/사용·퀵슬롯 → 클라는 `CS_EQUIP`/`CS_UNEQUIP`/`CS_USE_ITEM` **요청만**, 서버 처리 후 `SC_INVEN_UPDATE`(장비 포함)/`SC_PLAYER_HP` 스냅샷으로만 반영.
- 서버 `CPlayer::Get_Atk()=기본10+장비+ATK버프`, `Get_Def()=기본5+장비`. 전투: 플레이어 데미지=`Get_Atk()`, 받는 피해=`max(1, 몬공−Get_Def())`, 무적 시 0.
- 포션: HP/MP 회복 + 공격력/무적 버프(서버 지속시간 ATK 15s/무적 8s).
- **버프 쿨타임 UI**: 서버 `SC_BUFF`(타입+지속) → 클라 `CPlayer::Add_Buff` 자체 타이머 → `CUI_HUD::Render_Buffs`가 HP UI 위에 아이콘+검은 오버레이(경과 비율) 표시.
- **퀵슬롯 코드 기반**: `UI_QuickSlot::m_aSlotCode` + `CItemData::Get_ItemCode()` + `CPlayer::Use_QuickSlot_ByCode`. 인벤 재구성에도 등록 유지(아이템 소진 시만 해제).

### D. 버그 수정
- **몬스터 피격 중 계속 걷던 버그**: 클라 `OnStatePacket`이 HIT/ATTACK에서도 `m_bMoving=false`, `Move_To_Dest`가 HIT/ATTACK/DEAD 중 이동·애니 override 금지. 서버 피격 시 `m_fDest=m_fCur`로 이동 취소.
- **몬스터 사망 시 HP 잔상(10으로 죽어보임)**: 클라 `MON_DEAD`에서 HP=0.

### E. DB 연동 환경 구축 + 멀티플레이 동기화 버그 수정 (2026-07-12, 데스크탑)

**DB 환경(새 PC에서 처음부터 세팅)**
- MySQL Server 8.0.46(서비스 `MySQL80`, root/`1234`) + Connector/ODBC **9.7.0**(코드 하드코딩 `MySQL ODBC 9.7 Unicode Driver`와 일치해야 함).
- DB `mmorpg` + 스키마/프로시저/시드 실행(`db/schema.sql`→`procedures.sql`→`seed.sql`). 게임계정 `mmo_server`@localhost/`1234`(`mysql_native_password`)로 생성.
- 서버 기동 시 `[DB] connected: MySQL 8.0.46` 확인. 시드 계정 test1(마을)/test2(북필드), 둘 다 비번 1234.
- 상세 절차는 메모리 `mmorpg-db-setup` 참고. (경매장/로그인 DB 연동은 직전 커밋 `f5454b2`에서 이미 코드화됨.)

**멀티플레이 동기화 버그 5건 수정(모두 2인 실기동 검증 완료)** — 공통 원인: 관찰자(다른 플레이어 클라)에게 방향/정지위치/시야 진입이 전달 안 됨. 자기 클라만 정상이던 문제들.
1. **공격 시 남의 방향 어긋남**: `SC_PLAYER_STATE`에 방향 없음 → 마지막 이동 방향으로 공격. → 패킷에 `dir`+`fCurX/Z` 추가, 서버 `OnPlayerAttackMonster`가 몬스터 향한 방향 계산해 브로드캐스트.
2. **통과 중 피격 시 남에겐 목적지까지 계속 이동**: `SC_PLAYER_HIT`에 정지위치 없고 관찰자 `OnHitPacket`이 `m_bMoving`을 안 끔. → 패킷에 정지위치 추가, 관찰자 스냅+정지.
3. **쫓기는 몬스터가 시야 진입해도 안 보임**: `Broadcast_MoveMonster`가 이미 시야에 있는 플레이어에게만 전송(몬스터 이동 시 시야 진입 처리 없음). → 몬스터 기준 시야 진입/이탈 대칭 처리 추가.
4. **리스폰 후 남에게 안 보임**: 클라 remove는 지연삭제(`Set_Dead`)인데 add는 기존 객체 있으면 무시 → 리스폰의 remove→add 연속에서 add가 버려짐. → `Handle_SC_ADD_PLAYER`가 기존 객체 재사용(부활), `COther_Player::Initialize`에서 `m_bDead=false`.
5. **시야 진입한 몬스터 방향 엉뚱**: `SC_ADD_MONSTER`에 `dir` 없어 기본 DIR_B로 이동. → 패킷에 `dir` 추가(서버 채우고 클라 `Set_Dir`).
- 편집 파일: 양쪽 `Protocol.h`, 서버 `Zone.cpp`, 클라 `Network_Manager.cpp`/`Other_Player.{h,cpp}`. **미커밋 상태**(사용자 요청으로 커밋 보류).

**함정 발견 — `NO_SERVER` 매크로(define.h)는 이름과 정반대**: 정의돼 있어도 서버 접속은 항상 함(온라인 모드). 끄면 `Monster.h` `#else` 분기의 `m_fServerX/Z`가 미선언이라 **컴파일 깨짐**(죽은 코드). 온라인 테스트한다고 끄지 말 것. 언젠가 이 `#else` 분기 정리 필요.

### F. 레벨/경험치 + 퀵슬롯 DB 연동 + 대형 맵 (2026-07-14)

**레벨/경험치 (서버 권위, DB 영속)**
- 신규 패킷 `SC_PLAYER_EXP`(2016). 몬스터 처치 → 막타 친 플레이어에게 `CPlayer::AddExp` → 전송. 레벨업 시 `SC_PLAYER_HP`도 함께(최대치 변동 + 풀회복).
- 필요 경험치 `100 × 레벨`, 만렙 50. 오크 20 / 윙 25 exp.
- **파생 스탯은 증분이 아니라 `ApplyLevelStats()` 한 곳에서 레벨로부터 계산** — 레벨업 때도 DB 로드 때도 같은 식이라 값이 어긋나지 않는다.
- DB: `character.level/exp` 컬럼(`migration_2026-07-14_exp.sql`).
- **잠복 버그 2개 발견**: 클라 `CPlayer` 생성자가 `m_iCurExp = m_iMaxExp`라 경험치 바가 항상 꽉 차 있었음 / `Handle_SC_PLAYER_HP`가 MaxHP·MaxMP를 무시해 레벨업 후 HP바가 멈춰 보일 뻔함.

**퀵슬롯 DB 연동 (유일하게 "클라 권위")**
- 퀵슬롯은 표시 정보일 뿐이고 조작해도 이득이 없다(사용은 `CS_USE_ITEM`이 따로 검증). 서버 권위로 만들면 드래그마다 왕복 지연으로 UI만 굳는다 → **클라가 정본, 서버는 저장소**.
- 인게임: 클라가 먼저 바꾸고 `CS_QUICKSLOT_SET`(1018)로 통보(응답 없음). 로그인: 서버가 `SC_QUICKSLOT_UPDATE`(2017)로 8칸 스냅샷 반환.
- 변경 창구를 `CUI_QuickSlot::Set_Slot()` 하나로 모으고 **값이 실제 바뀔 때만 전송**(`Update_SlotValidity`가 매 프레임 돌기 때문).
- **`SC_QUICKSLOT_UPDATE`는 반드시 인벤 스냅샷 뒤에** 보낼 것(클라가 코드를 인벤에서 찾아 아이콘을 그리므로).
- 서버가 슬롯 범위·카테고리 검증(장비는 퀵슬롯 등록 거부). DB: `quickslot` 테이블(`migration_2026-07-14_quickslot.sql`), `sp_login` 결과셋 4번째.

**대형 맵 2종 (부하/길찾기 측정 기반)**
- `ZONE_TEST` → **`ZONE_FIELD_N`**으로 개명(enum 값 0 유지 → DB 마이그레이션 불필요). 잔디 타일 키 `TEST_*` → `GRASS_*`.
- **`ZONE_RAID`(5, 장애물 13.4%) / `ZONE_RAID_FLAT`(6, 평지)**, 각 150×150. **장애물 유무만 다르고 나머지 동일** — A* 개선의 before/after 대조군.
- 블록맵은 `RaidMap.h`가 고정 시드 LCG로 생성(클라/서버 복제본 2벌). 연결성 검증 완료(고립 구역 0).
- 몬스터 존당 120마리, 어그로 8타일(기본 3이면 A* 경로가 4칸 이하라 길찾기 부하가 아예 측정되지 않음).
- 진입: 마을 천사 NPC 클릭 → `CUI_ZoneSelect` 선택창 → `CS_PORTAL`.
- 검증: 파이썬으로 같은 LCG를 재현해 **막힌 칸은 서버가 거부, 뚫린 칸은 통과**, 평지 맵에선 같은 칸이 통과됨을 확인(= 서버 맵 == 공유 헤더 맵).

**버그 수정**
- `Object_Manager::Get_Player`/`Get_List`, `UI_Manager::Get_List`가 **빈 리스트일 때 return이 없어 UB**(경고 C4715). Get_Player는 nullptr 반환, Get_List는 항상 유효 주소 반환으로 수정.

---

## 핵심 규칙 / 함정 (꼭 지킬 것)
1. **인코딩**: 대부분 소스가 원래 CP949. Edit/Write 도구로 한글 든 CP949 파일을 고치면 깨짐. **편집 전 UTF-8+BOM 변환**, 편집 후 BOM 유지 확인. (PowerShell: `GetEncoding(949)`로 읽어 `UTF8Encoding($true)`로 저장)
2. **`Protocol.h`는 클라/서버 2벌**. 패킷 바꾸면 **양쪽 동일하게** 수정.
3. **아이템 코드 = category×1000 + subType** (1xxx 포션, 2xxx 스크롤, 3xxx 장비, 4xxx 기타, 9000 골드).
4. **포션 enum 함정**: 클라 `POTION_TYPE` enum의 [4]무적/[5]공격이 데이터 테이블 순서([4]공격/[5]무적)와 뒤바뀜 → 서버 `g_PotionTable`은 **배열 순서(아이콘 기준)**로 맞춰져 있음. 건드릴 때 주의.
5. **기본 공격력 10/방어 5**로 서버=클라 통일(예전 서버 고정 20에서 변경). 조정하려면 서버 `CPlayer::m_baseAtk/m_baseDef`.
6. **장비/필드 블록맵 등 테이블은 클라↔서버 값 일치** 필수(`g_EquipTable` ↔ 클라 `s_EquipTable`).
7. 새 .cpp 추가 시 **클라 vcxproj/filters에 등록** 필요(헤더 온리는 불필요).

---

## 다음에 할 것 — 부하 테스트와 최적화 (기능 추가보다 이쪽)

기능은 이미 충분하다. 지금 포트폴리오의 약점은 **"IOCP는 수천 커넥션을 처리한다"고 써놓고 실제로 2명까지밖에 안 붙여봤다**는 것.
"만들어봤다"에서 **"만들고 한계를 측정해서 개선했다"**로 올리는 게 목표.

1. **부하 봇 + 서버 계측** ← 여기서 시작
   - 봇: C++(파이썬은 부하가 부족). 로그인 → 랜덤 이동 → (옵션) 공격. **존/이동빈도/봇수를 인자로**.
   - 봇을 서버와 같은 PC에서 돌리면 측정이 오염됨(가능하면 별도 PC, 아니면 봇을 아주 가볍게).
   - 계측은 **서버 안에** 심을 것(봇에서 RTT만 재면 "어디가" 느린지 모름): 패킷 ID별 처리 지연(평균/**p99**), 락 대기 시간, 워커별 처리 건수 → 기존 디버그 콘솔 스레드에 출력.
2. **시나리오 3종**
   - A. 분산(5개 존에 골고루) = 순수 I/O 한계. 아마 1000명 여유.
   - B. **밀집(한 존에 50→100→200→300)** = 월드 로직 한계. **이쪽이 핵심 숫자.**
   - C. 전투 부하 = 타이머 큐(AI/타격) + DB 저장이 워커를 얼마나 막는지.
3. **예상 병목(이미 코드에서 보임)**
   - `CZone::GetNearPlayers()`가 **존 전체를 O(N) 순회**하며 `m_zoneLock`을 잡음. 타일 바뀔 때마다 호출 → **초당 O(N²)**. `Broadcast_MoveMonster`/`Broadcast_MonsterState`도 존 전체 순회.
   - **DB 저장이 IOCP 워커를 블로킹**(connection-per-call + 5초에 1명 라운드로빈).
4. **개선 → 재측정 (before/after 숫자 확보가 목적)**
   - **섹터(공간 분할) AOI**: 존 안을 격자로 쪼개 3×3 이웃만 순회. **섹터 크기 ≥ VIEW_RANGE(5)** 여야 3×3이 시야(11×11)를 덮는다. 존과 섹터는 경쟁 관계가 아니라 계층이 다름(존=논리 분리, 섹터=공간 최적화).
     - 단, **초고밀도에선 섹터도 못 구한다**(모두가 서로 시야면 전송량은 여전히 O(N²)) → 그땐 채널 분리/관심도 필터/패킷 배칭.
   - **전용 DB 스레드 + 커넥션 풀** (게임 스레드는 스냅샷만 큐에 던지고 즉시 리턴).
   - **A* 개선**: 지금은 **평지에서도 A*를 그대로 돌린다**(`FindPath`의 조기 반환은 "같은 타일"/"목적지가 벽" 둘뿐 — LOS 조기 반환 없음). 매 호출 `priority_queue` + `unordered_map` 2개를 새로 할당하는 것도 비용. → 직선 시야 조기 반환 + 자료구조 재사용. **대형 맵 2종(장애물/평지)이 이 개선의 대조군.**

## 남은 정리거리
- `AuctionManager.h` — DB 이전 후 **어디서도 include 안 하는 죽은 코드**. 삭제 필요.
- `NO_SERVER`의 `#else` 분기 — 컴파일 불가한 죽은 코드. 제거.
- 스크롤 효과(마을 귀환/감정) — 현재 소비만 됨.
- 퀵슬롯 4~7번(스킬 슬롯) 비어 있음.
- 비밀번호 평문 → 해시(30분이면 되는데 안 하면 면접에서 반드시 지적당함).

## 참고
- 빌드: MSBuild `MMO_GameServer.sln` / `MMO_Client.sln` `/p:Configuration=Debug /p:Platform=x64`.
- 포트폴리오: `PORTFOLIO.md`(전체, 면접용) / `PORTFOLIO_Server.md`(서버 요약).
- DB 마이그레이션: 다른 PC에선 `db/migration_2026-07-14_*.sql` 실행 후 **`procedures.sql` 재실행**(sp_login이 바뀜).
