-- ============================================================
--  MMORPG_Project  DB 스키마 (구조 정의)
--  - 이 파일이 "진짜 공유해야 할 것". git으로 두 PC 동기화.
--  - 실행: Workbench에서 이 파일 열고 전체 실행(⚡),
--          또는 터미널  mysql -u root -p < db/schema.sql
--  - 문자셋 utf8mb4: 한글(예: 경매장 판매자명) 안전하게 저장.
-- ============================================================

-- 표들을 담을 데이터베이스(스키마) 생성. 이미 있으면 건너뜀.
CREATE DATABASE IF NOT EXISTS mmorpg
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_general_ci;

USE mmorpg;                 -- 이후 명령은 mmorpg 데이터베이스 안에서 실행

-- ------------------------------------------------------------
--  account : 로그인 계정 정보
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS account (
    account_id  VARCHAR(20)  NOT NULL,                        -- 아이디(최대 20자)
    password    VARCHAR(64)  NOT NULL,                        -- 비번(지금 평문, 추후 해시 대비 64자)
    created_at  TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 가입 시각 자동기록
    PRIMARY KEY (account_id)                                  -- 대표 열쇠(중복불가+빠른검색)
);

-- ------------------------------------------------------------
--  character : 캐릭터 상태 (계정당 1행)
--   - account_id 를 그대로 PK 겸 FK 로 사용 - "계정=캐릭터 1:1" 을 구조로 강제.
--   - FAccountData 의 zoneID / spawnX,Z / gold 에 대응.
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `character` (
    account_id  VARCHAR(20)  NOT NULL,                        -- 어느 계정인지 (account 참조)
    zone_id     INT          NOT NULL DEFAULT 0,              -- 로그인 시작 존 (ZONE_ID enum 값)
    spawn_x     FLOAT        NOT NULL DEFAULT 0,              -- 시작 좌표 X
    spawn_z     FLOAT        NOT NULL DEFAULT 0,              -- 시작 좌표 Z
    gold        INT          NOT NULL DEFAULT 0,              -- 보유 골드
    level       INT          NOT NULL DEFAULT 1,              -- 레벨 (1 ~ CPlayer::MAX_LEVEL=50)
    exp         INT          NOT NULL DEFAULT 0,              -- 현재 레벨에서 쌓은 경험치 (누적 아님)
    PRIMARY KEY (account_id),                                 -- 계정당 1행 보장
    CONSTRAINT fk_character_account                           -- account 없는 아이디면 생성 불가
        FOREIGN KEY (account_id) REFERENCES account (account_id)
        ON DELETE CASCADE                                     -- 계정 삭제 시 캐릭터도 자동 삭제
);

-- ------------------------------------------------------------
--  inventory : 인벤토리 (아이템 1개 = 행 1개)
--   - (account_id, slot) 복합 PK - 한 칸에는 아이템 하나만.
--   - FAccountData 의 inven[16] 에 대응 (slot 0~15).
--   - item_code = category*1000 + subType (1xxx 포션 … 9000 골드).
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS inventory (
    account_id  VARCHAR(20)  NOT NULL,                        -- 소유 계정
    slot        INT          NOT NULL,                        -- 인벤 칸 번호 (0~15)
    item_code   INT          NOT NULL,                        -- 아이템 고유코드
    count       INT          NOT NULL DEFAULT 1,              -- 수량
    PRIMARY KEY (account_id, slot),                           -- 계정+칸 = 유일
    CONSTRAINT fk_inventory_account
        FOREIGN KEY (account_id) REFERENCES account (account_id)
        ON DELETE CASCADE
);

-- ------------------------------------------------------------
--  equipment : 장착 장비 (슬롯 1개 = 행 1개)
--   - (account_id, slot) 복합 PK - 한 슬롯에는 장비 하나만.
--   - FAccountData 의 equip[6] 에 대응 (slot 0~5, 클라 EQUIP_SLOT 순서).
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS equipment (
    account_id  VARCHAR(20)  NOT NULL,                        -- 소유 계정
    slot        TINYINT      NOT NULL,                        -- 장비 슬롯 (0~5)
    item_code   INT          NOT NULL,                        -- 장착 아이템 코드
    PRIMARY KEY (account_id, slot),                           -- 계정+슬롯 = 유일
    CONSTRAINT fk_equipment_account
        FOREIGN KEY (account_id) REFERENCES account (account_id)
        ON DELETE CASCADE
);

-- ------------------------------------------------------------
--  quickslot : 퀵슬롯 등록 내용 (슬롯 1개 = 행 1개)
--   - (account_id, slot) 복합 PK - 한 칸에는 아이템 하나만.
--   - FAccountData 의 quick[8] 에 대응 (slot 0~7; 0~3 소비아이템, 4~7 스킬 예정).
--   - 인벤과 달리 "무엇을 등록했나"만 저장 - 수량은 인벤이 정본이다.
--   - 아이템을 다 쓰면 클라가 슬롯을 비우고 서버에 알리므로 행이 지워진다.
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS quickslot (
    account_id  VARCHAR(20)  NOT NULL,                        -- 소유 계정
    slot        TINYINT      NOT NULL,                        -- 퀵슬롯 번호 (0~7)
    item_code   INT          NOT NULL,                        -- 등록 아이템 코드
    PRIMARY KEY (account_id, slot),                           -- 계정+칸 = 유일
    CONSTRAINT fk_quickslot_account
        FOREIGN KEY (account_id) REFERENCES account (account_id)
        ON DELETE CASCADE
);

-- ------------------------------------------------------------
--  auction : 경매장 매물 (매물 1건 = 행 1개)
--   - listing_id 는 DB가 자동 발급(AUTO_INCREMENT) - 서버가 ID 관리 불필요.
--   - 최신 등록순 = listing_id DESC (PK 인덱스로 정렬 빠름).
--   - 페이지네이션: ORDER BY listing_id DESC LIMIT ? OFFSET ? 로 한 페이지씩.
--   - seller_name 은 계정 이름 문자열로만 식별(기본매물 "경매장"은 실계정 아님이라
--     account FK 를 걸지 않음). 한글 판매자명 위해 스키마 utf8mb4.
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS auction (
    listing_id   INT          NOT NULL AUTO_INCREMENT,        -- 매물 고유번호(DB 자동발급)
    item_code    INT          NOT NULL,                       -- 아이템 코드
    count        INT          NOT NULL,                       -- 남은 수량
    unit_price   INT          NOT NULL,                       -- 개당 가격
    pending_gold INT          NOT NULL DEFAULT 0,             -- 판매되어 미수령한 골드
    seller_name  VARCHAR(20)  NOT NULL,                       -- 판매자(계정 이름)
    created_at   TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (listing_id)
);
