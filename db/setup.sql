-- ============================================================
--  MMORPG_Project  DB 전체 설치 스크립트  (이 파일 하나면 끝)
--
--  실행: MySQL Workbench 에서 이 파일 열고 전체 실행(Ctrl+Shift+Enter)
--        또는  mysql -u root -p < db/setup.sql
--
--  ★ 반드시 root(또는 DDL + GRANT 권한 계정)로 접속해서 실행할 것.
--     CREATE DATABASE / CREATE USER / GRANT 가 들어 있어서
--     게임 서버용 계정(mmo_server)으로는 실행되지 않는다.
--     Workbench 왼쪽 Connections 에서 어느 계정으로 붙었는지 먼저 확인.
--
--  ============================================================
--  이 파일이 해주는 것
--  ============================================================
--   1. 게임 DB(mmorpg) 생성
--   2. 서버 전용 계정(mmo_server) 생성 + 최소 권한 부여
--   3. 테이블 6개 생성
--   4. 예전 DB 자동 업그레이드 (없는 컬럼/테이블만 추가)
--   5. 저장 프로시저(sp_login) 생성
--   6. 테스트 계정 + 경매장 기본 매물 시드
--   7. 로그 DB(mmorpg_log) 생성 + 권한
--   8. 설치 확인 출력
--
--  ============================================================
--  재실행해도 안전한가 - 예. 다만 아래 두 가지는 초기화된다.
--  ============================================================
--   - 테스트 계정 test1 / test2  (지우고 다시 넣음)
--   - 경매장 기본 매물(판매자 '경매장')  (지우고 다시 넣음)
--  플레이어가 만든 계정과 플레이어가 올린 매물은 건드리지 않는다.
--  기존 캐릭터의 골드/인벤/레벨도 그대로 보존된다.
--
--  ============================================================
--  새 PC 에 서버를 세팅하는 순서
--  ============================================================
--   1) MySQL 8.x 설치
--   2) MySQL ODBC Unicode Driver 설치
--        (DB_Manager.cpp 의 연결 문자열과 드라이버 이름이 같아야 한다.
--         현재: "MySQL ODBC 9.7 Unicode Driver")
--   3) Workbench 에서 root 로 접속해 이 파일 전체 실행
--   4) 서버 실행 - 콘솔에 "[DB] connected: MySQL 8.x" 가 뜨면 성공
--
--  통합 전에는 schema/procedures/seed/auction_seed/migration 2개/fix_login
--  총 7개 파일을 순서 맞춰 실행해야 했고, 그러고도 mmo_server 계정을
--  만드는 SQL 이 없어서 새 PC 에서는 서버가 DB 에 붙지 못했다.
-- ============================================================


-- ============================================================
--  0. 실행 환경 준비
--
--  MySQL Workbench 는 기본으로 safe update mode 가 켜져 있다.
--  이 모드에서는 "키 컬럼을 쓰지 않는 WHERE" 로 DELETE/UPDATE 를 하면
--  Error 1175 로 거부한다. (실수로 전체 행을 날리는 사고 방지용)
--
--  아래 6장 시드에서
--      DELETE FROM auction WHERE seller_name = '경매장';
--  를 하는데 seller_name 에는 인덱스가 없어서 이 모드에 걸린다.
--
--  Workbench 설정을 손대라고 하면 새 PC 마다 똑같은 삽질을 반복해야 하므로,
--  스크립트가 세션 단위로 잠시 껐다가 끝에서 원래 값으로 되돌린다.
--  (세션 변수라 이 스크립트를 실행한 창에만 영향을 준다)
-- ============================================================
SET @old_safe_updates := @@SQL_SAFE_UPDATES;
SET SQL_SAFE_UPDATES = 0;


-- ============================================================
--  1. 게임 데이터베이스
--     utf8mb4 : 한글(예: 경매장 판매자명)을 안전하게 저장
-- ============================================================
CREATE DATABASE IF NOT EXISTS mmorpg
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_general_ci;


-- ============================================================
--  2. 서버 전용 계정
--
--  게임 서버는 root 가 아니라 이 계정으로 접속한다(최소 권한 원칙).
--  DDL(CREATE/ALTER/DROP) 권한을 주지 않았으므로, 서버 코드에 버그가
--  있어도 테이블을 날리는 사고는 구조적으로 일어나지 않는다.
--
--  비밀번호는 DB_Manager.cpp 의 연결 문자열과 반드시 같아야 한다.
--    DB_Manager.cpp : "User=mmo_server;Password=1234;"
--  개발용 비밀번호이므로 외부에 노출되는 서버라면 양쪽 다 바꿀 것.
--
--  IF NOT EXISTS 라서 이미 있으면 비밀번호를 덮어쓰지 않는다.
--  비밀번호를 바꾸고 싶으면 아래 ALTER USER 주석을 풀어 쓸 것.
-- ============================================================
CREATE USER IF NOT EXISTS 'mmo_server'@'localhost' IDENTIFIED BY '1234';
-- ALTER USER 'mmo_server'@'localhost' IDENTIFIED BY '1234';

-- 데이터 읽고 쓰고 프로시저 호출만. 테이블 구조는 못 건드린다.
GRANT SELECT, INSERT, UPDATE, DELETE, EXECUTE ON mmorpg.* TO 'mmo_server'@'localhost';


USE mmorpg;

-- ============================================================
--  3. 테이블
-- ============================================================

-- ------------------------------------------------------------
--  account : 로그인 계정
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS account (
    account_id  VARCHAR(20)  NOT NULL,                            -- 아이디
    password    VARCHAR(64)  NOT NULL,                            -- 비번(지금 평문, 추후 해시 대비 64자)
    created_at  TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,  -- 가입 시각
    PRIMARY KEY (account_id)
);

-- ------------------------------------------------------------
--  character : 캐릭터 상태 (계정당 1행)
--   - account_id 를 PK 겸 FK 로 써서 "계정=캐릭터 1:1" 을 구조로 강제.
--   - 서버 FAccountData 에 대응.
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `character` (
    account_id  VARCHAR(20)  NOT NULL,
    zone_id     INT          NOT NULL DEFAULT 0,   -- 로그인 시작 존 (ZONE_ID enum)
    spawn_x     FLOAT        NOT NULL DEFAULT 0,
    spawn_z     FLOAT        NOT NULL DEFAULT 0,
    gold        INT          NOT NULL DEFAULT 0,
    level       INT          NOT NULL DEFAULT 1,   -- 1 ~ CPlayer::MAX_LEVEL(50)
    exp         INT          NOT NULL DEFAULT 0,   -- 현재 레벨에서 쌓은 양(누적 아님)
    PRIMARY KEY (account_id),
    CONSTRAINT fk_character_account
        FOREIGN KEY (account_id) REFERENCES account (account_id)
        ON DELETE CASCADE                          -- 계정 지우면 캐릭터도 자동 삭제
);

-- ------------------------------------------------------------
--  inventory : 인벤토리 (아이템 1개 = 행 1개, slot 0~15)
--   - item_code = category*1000 + subType (1xxx 포션 … 9000 골드)
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS inventory (
    account_id  VARCHAR(20)  NOT NULL,
    slot        INT          NOT NULL,
    item_code   INT          NOT NULL,
    count       INT          NOT NULL DEFAULT 1,
    PRIMARY KEY (account_id, slot),
    CONSTRAINT fk_inventory_account
        FOREIGN KEY (account_id) REFERENCES account (account_id)
        ON DELETE CASCADE
);

-- ------------------------------------------------------------
--  equipment : 장착 장비 (slot 0~5, 클라 EQUIP_SLOT 순서)
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS equipment (
    account_id  VARCHAR(20)  NOT NULL,
    slot        TINYINT      NOT NULL,
    item_code   INT          NOT NULL,
    PRIMARY KEY (account_id, slot),
    CONSTRAINT fk_equipment_account
        FOREIGN KEY (account_id) REFERENCES account (account_id)
        ON DELETE CASCADE
);

-- ------------------------------------------------------------
--  quickslot : 퀵슬롯 등록 내용 (slot 0~7)
--   - "무엇을 등록했나"만 저장. 수량의 정본은 inventory 다.
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS quickslot (
    account_id  VARCHAR(20)  NOT NULL,
    slot        TINYINT      NOT NULL,
    item_code   INT          NOT NULL,
    PRIMARY KEY (account_id, slot),
    CONSTRAINT fk_quickslot_account
        FOREIGN KEY (account_id) REFERENCES account (account_id)
        ON DELETE CASCADE
);

-- ------------------------------------------------------------
--  auction : 경매장 매물 (매물 1건 = 행 1개)
--   - listing_id 는 DB 가 자동 발급 - 서버가 ID 를 관리하지 않는다.
--   - 최신순 = listing_id DESC (PK 인덱스로 정렬이 빠름).
--   - seller_name 에 FK 를 안 건 이유: 기본 매물 판매자 '경매장' 은
--     실제 계정이 아니라서 FK 를 걸면 INSERT 가 막힌다.
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS auction (
    listing_id   INT          NOT NULL AUTO_INCREMENT,
    item_code    INT          NOT NULL,
    count        INT          NOT NULL,        -- 남은 수량
    unit_price   INT          NOT NULL,        -- 개당 가격
    pending_gold INT          NOT NULL DEFAULT 0,  -- 판매됐지만 미수령한 골드
    seller_name  VARCHAR(20)  NOT NULL,
    created_at   TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (listing_id)
);


-- ============================================================
--  4. 예전 DB 자동 업그레이드
--
--  위의 CREATE TABLE IF NOT EXISTS 는 "테이블이 없을 때만" 만든다.
--  이미 테이블이 있는 예전 DB 에는 새로 생긴 컬럼을 추가해주지 않는다.
--  그래서 컬럼이 없을 때만 ALTER 하는 블록을 따로 둔다.
--
--  이 블록 덕분에 이 파일 하나가 "새 설치"와 "기존 DB 업그레이드"를
--  둘 다 처리한다. (예전엔 migration_*.sql 을 따로 실행해야 했다)
--  이미 컬럼이 있으면 아무 일도 하지 않으므로 몇 번을 실행해도 안전하다.
-- ============================================================

-- character.level
SET @has_col := (SELECT COUNT(*) FROM information_schema.columns
                 WHERE table_schema='mmorpg' AND table_name='character' AND column_name='level');
SET @ddl := IF(@has_col = 0,
    'ALTER TABLE `character` ADD COLUMN level INT NOT NULL DEFAULT 1',
    'SELECT ''level 컬럼 이미 있음 - 건너뜀'' AS upgrade_level');
PREPARE s FROM @ddl; EXECUTE s; DEALLOCATE PREPARE s;

-- character.exp
SET @has_col := (SELECT COUNT(*) FROM information_schema.columns
                 WHERE table_schema='mmorpg' AND table_name='character' AND column_name='exp');
SET @ddl := IF(@has_col = 0,
    'ALTER TABLE `character` ADD COLUMN exp INT NOT NULL DEFAULT 0',
    'SELECT ''exp 컬럼 이미 있음 - 건너뜀'' AS upgrade_exp');
PREPARE s FROM @ddl; EXECUTE s; DEALLOCATE PREPARE s;


-- ============================================================
--  5. 저장 프로시저
--
--  프로시저 본문 안에 세미콜론이 여러 개라, DELIMITER 로 문장 끝 기호를
--  잠시 $$ 로 바꿔서 CREATE 전체가 한 덩어리로 넘어가게 한다.
--  ★ DELIMITER 줄 뒤에는 주석이나 공백을 붙이지 말 것 -
--    뒤 내용이 통째로 구분자가 되어 CREATE 가 조용히 누락된다.
--  ★ Workbench 에서 부분 실행하지 말 것. 반드시 전체 실행(Ctrl+Shift+Enter).
-- ============================================================

-- ------------------------------------------------------------
--  sp_login(id, pw)
--    맞으면 그 계정 데이터를 결과셋 4개로 반환:
--      1) character  (성공 시 1행, 실패 시 0행) - 서버는 이걸로 성공/실패 판단
--      2) inventory
--      3) equipment
--      4) quickslot
--    인증 실패면 p_id 를 NULL 로 바꿔 네 조회가 모두 0행이 되게 한다.
-- ------------------------------------------------------------
DROP PROCEDURE IF EXISTS sp_login;

DELIMITER $$
CREATE PROCEDURE sp_login(
    IN p_id VARCHAR(20),
    IN p_pw VARCHAR(64)
)
BEGIN
    DECLARE v_ok INT DEFAULT 0;

    SELECT COUNT(*) INTO v_ok
    FROM account
    WHERE account_id = p_id AND password = p_pw;

    IF v_ok = 0 THEN
        SET p_id = NULL;
    END IF;

    SELECT zone_id, spawn_x, spawn_z, gold, level, exp
    FROM `character`
    WHERE account_id = p_id;

    SELECT slot, item_code, count
    FROM inventory
    WHERE account_id = p_id
    ORDER BY slot;

    SELECT slot, item_code
    FROM equipment
    WHERE account_id = p_id
    ORDER BY slot;

    SELECT slot, item_code
    FROM quickslot
    WHERE account_id = p_id
    ORDER BY slot;
END$$

DELIMITER ;


-- ============================================================
--  6. 시드 데이터
--
--  재실행 안전: 아래에서 지우고 다시 넣는 대상은
--    - test1 / test2 계정
--    - 판매자가 '경매장' 인 기본 매물
--  뿐이다. 플레이어 계정과 플레이어 매물은 손대지 않는다.
--
--  account 만 지워도 FK ON DELETE CASCADE 덕분에
--  character / inventory / equipment / quickslot 의 해당 행이 함께 지워진다.
-- ============================================================

DELETE FROM account WHERE account_id IN ('test1', 'test2');

-- test1 : 마을(ZONE_TOWN=1), 골드 1000, 인벤 4칸, 무기 장착
INSERT INTO account (account_id, password) VALUES ('test1', '1234');
INSERT INTO `character` (account_id, zone_id, spawn_x, spawn_z, gold)
    VALUES ('test1', 1, 12, 20, 1000);
INSERT INTO inventory (account_id, slot, item_code, count) VALUES
    ('test1', 0, 1000, 5),    -- HP포션(중) 5
    ('test1', 1, 1001, 2),    -- HP포션(대) 2
    ('test1', 2, 1004, 1),    -- 공격력 물약 1
    ('test1', 3, 3001, 1);    -- 소드1
INSERT INTO equipment (account_id, slot, item_code) VALUES
    ('test1', 0, 3000);       -- 슬롯0(무기)에 소드0

-- test2 : 북쪽필드(ZONE_FIELD_N=0), 골드 300, 인벤 2칸, 장비 없음
INSERT INTO account (account_id, password) VALUES ('test2', '1234');
INSERT INTO `character` (account_id, zone_id, spawn_x, spawn_z, gold)
    VALUES ('test2', 0, 10, 10, 300);
INSERT INTO inventory (account_id, slot, item_code, count) VALUES
    ('test2', 0, 1002, 5),    -- MP포션(중) 5
    ('test2', 1, 1005, 1);    -- 무적 물약 1

-- 경매장 상시 매물 (판매자 '경매장' 은 실제 계정이 아님)
DELETE FROM auction WHERE seller_name = '경매장';
INSERT INTO auction (item_code, count, unit_price, pending_gold, seller_name) VALUES
    (1000, 20,  18, 0, '경매장'),   -- HP 물약(중)
    (1001, 10,  30, 0, '경매장'),   -- HP 물약(대)
    (1002, 20,  18, 0, '경매장'),   -- MP 물약(중)
    (1005,  2, 150, 0, '경매장'),   -- 무적 물약
    (3001,  1, 400, 0, '경매장'),   -- 소드
    (3007,  1, 250, 0, '경매장'),   -- 방어구
    (3022,  1, 200, 0, '경매장');   -- 방패


-- ============================================================
--  7. 로그 데이터베이스 (mmorpg_log)
--
--  ★ 왜 게임 DB 와 다른 데이터베이스인가
--
--  백섭(특정 시점으로 DB 되돌리기)을 하면 그 시점 이후 변경이 전부 사라진다.
--  로그를 mmorpg 안에 두면 백섭할 때 로그도 같이 날아가고,
--  그러면 "되돌린 구간에서 정상 유저가 무엇을 잃었나"를 산정할 수 없다.
--  보상하려고 만든 로그가 정작 보상이 필요한 순간 사라지는 것이다.
--
--    - 백섭 대상      : mmorpg      (게임 상태)
--    - 백섭에서 제외  : mmorpg_log  (무슨 일이 있었는지의 기록)
--
--  실서비스라면 아예 다른 서버에 두는 게 맞다. 게임 DB 가 통째로 망가져도
--  로그는 살아있어야 하기 때문이다. 여기서는 별도 데이터베이스로 그 구조만 재현한다.
-- ============================================================

CREATE DATABASE IF NOT EXISTS mmorpg_log
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_general_ci;

-- ------------------------------------------------------------
--  game_log : 골드/아이템이 생기고 사라지고 옮겨간 모든 기록
--
--  설계 원칙
--   1) 재화가 움직이는 모든 경로는 여기를 거친다.
--      경로 하나라도 빠지면 아래 gold_balance 검증이 무의미해진다.
--   2) 절대 UPDATE/DELETE 하지 않는다. 오직 INSERT (append-only).
--      고칠 수 있는 로그는 감사 기록으로서 가치가 없다.
--   3) 시각은 서버가 아니라 DB 가 찍는다 - 서버 여러 대의 시계가
--      어긋나도 기록 순서가 보장된다.
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS mmorpg_log.game_log (
    log_id      BIGINT       NOT NULL AUTO_INCREMENT,

    log_type    VARCHAR(24)  NOT NULL,   -- 아래 '로그 타입' 목록 참고
    actor       VARCHAR(20)  NOT NULL,   -- 행위 주체(계정 ID)
    target      VARCHAR(20)  NULL,       -- 거래 상대(경매 구매면 판매자)

    item_code   INT          NOT NULL DEFAULT 0,   -- 골드만 움직이면 0
    quantity    INT          NOT NULL DEFAULT 0,   -- 아이템 증감(부호 포함)
    gold        INT          NOT NULL DEFAULT 0,   -- 골드 증감(부호 포함)

    -- ---- 검증용 잔액 (이 설계의 핵심) ----
    --  변동이 "끝난 뒤"의 보유 골드. 증감만 있으면 검증할 수가 없다.
    --  같은 actor 의 로그를 시간순으로 훑으면서
    --      직전 gold_balance + 이번 gold == 이번 gold_balance
    --  가 항상 성립해야 한다. 어긋나는 지점이 있다면
    --    - 로그를 안 거치고 골드를 바꾼 코드 경로가 있거나
    --    - 복제 버그로 골드가 그냥 생겨난 것이다.
    --  즉 이 컬럼 하나로 "로그에 안 잡힌 변화"를 역으로 찾아낼 수 있다.
    gold_balance INT         NOT NULL DEFAULT 0,

    detail      VARCHAR(128) NULL,       -- 사람이 읽을 부가 정보(매물번호 등)

    -- DATETIME(3) = 밀리초까지. 같은 초에 여러 거래가 일어나므로 필요.
    created_at  DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),

    PRIMARY KEY (log_id),
    INDEX idx_actor_time (actor, created_at),   -- 유저별 조회(문의/보상)
    INDEX idx_type_time  (log_type, created_at) -- 종류별 집계(이상 탐지)
);

-- ------------------------------------------------------------
--  로그 타입 목록 (log_type 에 넣는 문자열)
--
--   경매
--     AUCTION_LIST     등록      아이템 -
--     AUCTION_BUY      구매      아이템 +, 골드 -
--     AUCTION_SOLD     판매성사  (판매자 기준, 아직 미수령)
--     AUCTION_COLLECT  대금수령  골드 +
--     AUCTION_CANCEL   취소      아이템 +
--
--   재화 생성 (경제에 새로 유입 - 인플레이션 원인)
--     DROP_GAIN        몬스터 드롭
--     QUEST_REWARD     퀘스트 보상
--
--   재화 소멸 (경제에서 빠져나감 - 싱크)
--     SHOP_BUY         상점 구매  골드 -
--     SHOP_SELL        상점 판매  골드 +
--
--   기타
--     LEVEL_UP
--
--  생성과 소멸을 구분해 넣는 이유: "하루에 골드가 얼마나 새로 생기고
--  얼마나 사라지나"를 집계해야 경제 밸런싱이 되고,
--  비정상 유입(복제 버그)도 이 집계에서 가장 먼저 드러난다.
-- ------------------------------------------------------------

-- 서버 계정에는 INSERT 만 준다.
--  서버는 로그를 남길 수만 있고 고치거나 지울 수 없다.
--  append-only 를 코드가 아니라 권한 수준에서 강제하는 것 -
--  서버 코드에 버그가 생겨도 로그는 조작될 수 없다. 감사 로그의 기본 요건.
--  조회/분석은 운영자가 root 나 별도 읽기전용 계정으로 한다.
GRANT INSERT ON mmorpg_log.* TO 'mmo_server'@'localhost';

FLUSH PRIVILEGES;


-- ============================================================
--  8. 설치 확인
--     아래 결과가 예상과 같으면 성공.
-- ============================================================
SELECT '=== 테이블 (6개여야 정상) ===' AS check_step;
SELECT table_name FROM information_schema.tables
WHERE table_schema = 'mmorpg' ORDER BY table_name;

SELECT '=== 테스트 계정 (test1/test2) ===' AS check_step;
SELECT account_id, zone_id, gold, level, exp FROM `character` ORDER BY account_id;

SELECT '=== 경매장 기본 매물 (7건) ===' AS check_step;
SELECT COUNT(*) AS auction_default FROM auction WHERE seller_name = '경매장';

SELECT '=== 서버 계정 권한 ===' AS check_step;
SHOW GRANTS FOR 'mmo_server'@'localhost';

SELECT '=== 로그 테이블 ===' AS check_step;
SELECT COUNT(*) AS game_log_rows FROM mmorpg_log.game_log;

-- 0장에서 껐던 safe update mode 를 원래 값으로 되돌린다.
SET SQL_SAFE_UPDATES = @old_safe_updates;
