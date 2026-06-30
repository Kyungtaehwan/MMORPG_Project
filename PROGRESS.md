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

## Phase 3 (내일 할 것)
- **상점(NPC_Shop) 매매**: 골드로 구매/판매 (서버 권위, `CS_BUY`/`CS_SELL` + 골드/인벤 스냅샷).
- **스크롤 효과**: 마을 귀환(town scroll) / 감정(identify) — 현재는 사용 시 소비만 됨.
- (선택) 버프 상태를 다른 플레이어에게도 표시 / 버프 UI 미세조정(오버레이 방향, 위치).
- (선택) **DB 영속화**: 인벤(코드+개수)/장비/골드/스탯 저장·로드. 코드 기반이라 DB 친화적.
- (선택) 퀵슬롯에 코드 비었을 때 회색 아이콘 유지 등 UX.

## 참고
- 계획 파일(Phase 2/3 개요): `C:\Users\kkth3\.claude\plans\transient-enchanting-cook.md`
- 빌드: MSBuild `MMO_GameServer.sln` / `MMO_Client.sln` `/p:Configuration=Debug /p:Platform=x64`.
