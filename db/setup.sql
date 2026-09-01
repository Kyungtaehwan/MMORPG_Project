-- ============================================================
--  MMORPG_Project  DB 전체 설치 스크립트
--
--  반드시 root(또는 DDL + GRANT 권한 계정)로 접속해서 실행할 것.
--  ============================================================
--  


--  ============================================================
--   1. 게임 DB(mmorpg) 생성
--   2. 서버 전용 계정(mmo_server) 생성 + 최소 권한 부여
--   3. 테이블 6개 생성
--   4. 예전 DB 자동 업그레이드 (없는 컬럼/테이블만 추가)
--   5. 저장 프로시저(sp_login) 생성
--   6. 테스트 계정 + 경매장 기본 매물 시드
--   7. 로그 DB(mmorpg_log) 생성 + 권한
--   8. 설치 확인 출력

-- ============================================================
--  0. 실행 환경 준비
-- ============================================================
SET @old_safe_updates := @@SQL_SAFE_UPDATES;
SET SQL_SAFE_UPDATES = 0;


-- ============================================================
--  1. 게임 데이터베이스
--     utf8mb4 : 한글(경매장 판매자명 등 )을 안전하게 저장
-- ============================================================
CREATE DATABASE IF NOT EXISTS mmorpg
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_general_ci;


-- ============================================================
--  2. 서버 전용 계정
--  게임 서버는 root 가 아니라 이 계정으로 접속한다.
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
--   - account_id 를 PK 겸 FK 로 사용
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `character` (
    account_id  VARCHAR(20)  NOT NULL,
    zone_id     INT          NOT NULL DEFAULT 0,   -- 로그인 시작 존
    spawn_x     FLOAT        NOT NULL DEFAULT 0,
    spawn_z     FLOAT        NOT NULL DEFAULT 0,
    gold        INT          NOT NULL DEFAULT 0,
    level       INT          NOT NULL DEFAULT 1,   
    exp         INT          NOT NULL DEFAULT 0,   -- 현재 레벨에서 쌓은 양(누적 아님)
    PRIMARY KEY (account_id),
    CONSTRAINT fk_character_account
        FOREIGN KEY (account_id) REFERENCES account (account_id)
        ON DELETE CASCADE                          -- 계정 지우면 캐릭터도 자동 삭제
);

-- ------------------------------------------------------------
--  inventory : 인벤토리 (아이템 1개 = 행 1개, slot 0~15)
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


-- ------------------------------------------------------------
--  sp_rollback_account(계정, 시점, 적용여부)
--
--    계정 하나를 지정한 시점의 상태로 되돌린다.
--    시나리오 1(단독 악용자 선별 환수)에서 쓰는 도구다.
--
--      CALL sp_rollback_account('test1', '2026-08-04 13:47:30', 0);   -- 미리보기
--      CALL sp_rollback_account('test1', '2026-08-04 13:47:30', 1);   -- 실제 적용
--
--    p_apply = 0 이면 아무것도 바꾸지 않고 "이렇게 바뀔 것이다" 만 보여준다.
--    되돌리기는 되돌릴 수 없으므로 항상 0 으로 먼저 확인할 것.
--
--  어떻게 되돌리나 - 백업이 필요 없다
--
--      되돌릴 값 = 지금 값 - (그 시점 이후 로그의 증감 합계)
--
--    골드는 game_log 에 gold_balance(그 순간의 잔액)를
--    남겨두었으므로, 그 시점 이전 마지막 로그의 잔액을 그냥 읽으면 된다.
--    이 프로시저는 두 방법을 모두 계산해서 결과가 같은지 대조한다.
-- ------------------------------------------------------------
DROP PROCEDURE IF EXISTS sp_rollback_account;

DELIMITER $$
CREATE PROCEDURE sp_rollback_account(
    IN p_account VARCHAR(20),
    IN p_time    DATETIME(3),
    IN p_apply   TINYINT
)
    SQL SECURITY INVOKER
BEGIN
    DECLARE v_exists      INT DEFAULT 0;
    DECLARE v_log_cnt     INT DEFAULT 0;
    DECLARE v_auc_cnt     INT DEFAULT 0;
    DECLARE v_bad         INT DEFAULT 0;
    DECLARE v_slots       INT DEFAULT 0;

    DECLARE v_gold_now    INT DEFAULT 0;
    DECLARE v_gold_bal    INT DEFAULT NULL;   -- 방법1: 로그의 잔액을 읽는다
    DECLARE v_gold_calc   INT DEFAULT 0;      -- 방법2: 지금 값에서 증감을 뺀다
    DECLARE v_gold_target INT DEFAULT 0;

    DECLARE v_code   INT DEFAULT 0;
    DECLARE v_target INT DEFAULT 0;
    DECLARE v_need   INT DEFAULT 0;
    DECLARE v_cnt    INT DEFAULT 0;
    DECLARE v_slot   INT DEFAULT 0;
    DECLARE v_used   INT DEFAULT 0;

    -- 인벤토리 칸 수. 서버 INVEN_SIZE 와 반드시 같아야 한다.
    DECLARE c_inven_size INT DEFAULT 40;

    -- --------------------------------------------------------
    --  0. 계정 확인
    -- --------------------------------------------------------
    SELECT COUNT(*) INTO v_exists FROM `character` WHERE account_id = p_account;
    IF v_exists = 0 THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT = 'sp_rollback_account: no such account';
    END IF;

    SELECT gold INTO v_gold_now FROM `character` WHERE account_id = p_account;

    SELECT COUNT(*) INTO v_log_cnt
      FROM mmorpg_log.game_log
     WHERE actor = p_account AND created_at > p_time;

    -- --------------------------------------------------------
    --  1. 되돌릴 골드 구하기 (두 방법을 대조한다)
    -- --------------------------------------------------------
    --  방법1: 그 시점 이전 마지막 로그에 찍힌 잔액.
    --         gold <> 0 조건은 AUCTION_SOLD 때문이다. 판매 성사 로그는
    --         판매자가 접속 중이 아닐 수 있어 잔액을 0 으로 남긴다.
    SELECT gold_balance INTO v_gold_bal
      FROM mmorpg_log.game_log
     WHERE actor = p_account
       AND gold <> 0
       AND created_at <= p_time
     ORDER BY created_at DESC, log_id DESC
     LIMIT 1;

    --  방법2: 지금 골드에서 그 시점 이후의 증감을 전부 뺀다.
    SELECT v_gold_now - IFNULL(SUM(gold), 0) INTO v_gold_calc
      FROM mmorpg_log.game_log
     WHERE actor = p_account AND created_at > p_time;

    --  로그가 그 시점 이전에 아예 없으면 방법1 은 못 쓴다(NULL).
    SET v_gold_target = IFNULL(v_gold_bal, v_gold_calc);

    -- --------------------------------------------------------
    --  2. 되돌릴 아이템 구하기
    --
    --     그 시점 이후 로그의 수량 증감을 코드별로 합쳐서
    --     지금 개수에서 빼면 그 시점의 개수가 된다.
    --     9000(골드 드롭)은 아이템이 아니므로 뺀다.
    -- --------------------------------------------------------
    DROP TEMPORARY TABLE IF EXISTS tmp_rollback;
    CREATE TEMPORARY TABLE tmp_rollback (
        item_code    INT     NOT NULL PRIMARY KEY,
        now_count    INT     NOT NULL DEFAULT 0,
        delta        INT     NOT NULL DEFAULT 0,
        target_count INT     NOT NULL DEFAULT 0,
        processed    TINYINT NOT NULL DEFAULT 0
    );

    INSERT INTO tmp_rollback (item_code, delta)
    SELECT item_code, SUM(quantity)
      FROM mmorpg_log.game_log
     WHERE actor = p_account
       AND created_at > p_time
       AND item_code NOT IN (0, 9000)
     GROUP BY item_code;

    UPDATE tmp_rollback
       SET now_count = IFNULL((SELECT SUM(i.count) FROM inventory i
                                WHERE i.account_id = p_account
                                  AND i.item_code = tmp_rollback.item_code), 0);

    UPDATE tmp_rollback SET target_count = now_count - delta;

    --  개수가 음수로 나오면 로그가 실제 변화를 다 담지 못한 것이다.
    --  이 상태로 적용하면 데이터가 더 망가지므로 멈춘다.
    SELECT COUNT(*) INTO v_bad FROM tmp_rollback WHERE target_count < 0;
    IF v_bad > 0 THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT = 'sp_rollback_account: negative item count - log is incomplete';
    END IF;

    --  되돌린 결과가 인벤토리 칸 수를 넘지 않는지 본다.
    --  장비(3xxx)는 겹치지 않아 개수만큼 칸을 먹는다.
    SELECT IFNULL(SUM(CASE WHEN item_code BETWEEN 3000 AND 3999 THEN target_count
                           WHEN target_count > 0                THEN 1
                           ELSE 0 END), 0)
      INTO v_slots FROM tmp_rollback;

    SELECT v_slots + COUNT(*) INTO v_slots
      FROM inventory
     WHERE account_id = p_account
       AND item_code NOT IN (SELECT item_code FROM tmp_rollback);

    IF v_slots > c_inven_size THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT = 'sp_rollback_account: result exceeds inventory size';
    END IF;

    -- --------------------------------------------------------
    --  3. 경매 매물
    --     그 시점 이후에 올린 매물은 등록 자체를 되돌리는 것이므로
    --     아이템을 인벤으로 돌려주고 매물은 내린다.
    --     (돌려주는 쪽은 위 2번 계산에 이미 들어가 있다.)
    -- --------------------------------------------------------
    SELECT COUNT(*) INTO v_auc_cnt
      FROM auction
     WHERE seller_name = p_account AND created_at > p_time;

    -- --------------------------------------------------------
    --  4. 실제 적용
    -- --------------------------------------------------------
    IF p_apply = 1 THEN
        START TRANSACTION;

        UPDATE `character` SET gold = v_gold_target WHERE account_id = p_account;

        DELETE FROM auction
         WHERE seller_name = p_account AND created_at > p_time;

        --  대상 코드의 인벤 행을 지우고 목표 개수로 다시 넣는다.
        --  (서버의 저장 방식과 같다 - 다 지우고 다시 넣기)
        DELETE FROM inventory
         WHERE account_id = p_account
           AND item_code IN (SELECT item_code FROM tmp_rollback);

        WHILE (SELECT COUNT(*) FROM tmp_rollback
                WHERE processed = 0 AND target_count > 0) > 0 DO

            SELECT item_code, target_count INTO v_code, v_target
              FROM tmp_rollback
             WHERE processed = 0 AND target_count > 0
             LIMIT 1;

            IF v_code BETWEEN 3000 AND 3999 THEN
                SET v_need = v_target;   -- 장비는 한 칸에 하나씩
                SET v_cnt  = 1;
            ELSE
                SET v_need = 1;          -- 포션/스크롤/기타는 한 칸에 겹쳐 넣는다
                SET v_cnt  = v_target;
            END IF;

            SET v_slot = 0;
            WHILE v_need > 0 AND v_slot < c_inven_size DO
                SELECT COUNT(*) INTO v_used
                  FROM inventory
                 WHERE account_id = p_account AND slot = v_slot;

                IF v_used = 0 THEN
                    INSERT INTO inventory (account_id, slot, item_code, count)
                    VALUES (p_account, v_slot, v_code, v_cnt);
                    SET v_need = v_need - 1;
                END IF;
                SET v_slot = v_slot + 1;
            END WHILE;

            UPDATE tmp_rollback SET processed = 1 WHERE item_code = v_code;
        END WHILE;

        --  되돌린 사실도 로그로 남긴다.

        INSERT INTO mmorpg_log.game_log
            (log_type, actor, target, item_code, quantity, gold, gold_balance, detail)
        VALUES
            ('ADMIN_ROLLBACK', p_account, NULL, 0, 0,
             v_gold_target - v_gold_now, v_gold_target,
             CONCAT('rollback to ', DATE_FORMAT(p_time, '%Y-%m-%d %H:%i:%s.%f')));

        COMMIT;
    END IF;

    -- --------------------------------------------------------
    --  5. 결과 보고
    -- --------------------------------------------------------
    SELECT
        p_account                                          AS account,
        p_time                                             AS rollback_to,
        IF(p_apply = 1, 'APPLIED', 'PREVIEW ONLY')         AS mode,
        v_log_cnt                                          AS logs_undone,
        v_gold_now                                         AS gold_before,
        v_gold_target                                      AS gold_after,
        v_gold_bal                                         AS gold_by_balance,
        v_gold_calc                                        AS gold_by_sum,
        IF(v_gold_bal IS NULL, 'no log before that time',
           IF(v_gold_bal = v_gold_calc, 'OK - two methods agree',
              'MISMATCH - unlogged gold path exists'))     AS gold_check,
        v_auc_cnt                                          AS auction_listings_removed;

    SELECT item_code, now_count AS count_before, delta AS logged_change,
           target_count AS count_after
      FROM tmp_rollback
     ORDER BY item_code;

    SELECT listing_id, item_code, count, unit_price, created_at
      FROM auction
     WHERE seller_name = p_account AND created_at > p_time
     ORDER BY listing_id;

    DROP TEMPORARY TABLE IF EXISTS tmp_rollback;
END$$

DELIMITER ;


-- ============================================================
--  5-2. 부정 이득 몰수 (구간 지정)

--
--  지금 상태에서 "그 구간에 번 것만" 걷어낸다. 이후 로그는 안 본다.
--  사고가 로그 중간에 끼어 있어 뒤쪽 정상 플레이를 지켜야 할 때 쓴다.
--  대신 완전한 복원이 아니다 - 이미 써버린 것은 못 걷는다.
--
--    어느 쪽을 쓸지는 "사고 뒤에 정상 플레이가 있는가" 로 정한다.
-- ============================================================

-- ------------------------------------------------------------
--  미회수 몰수분
--
--  백섭하면 몰수 자체가 없던 일이 되므로 빚도 같이 사라져야 한다.
--  로그 DB 에 두면 백섭 후에도 빚이 남아
--  같은 재화를 두 번 걷게 된다.
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS confiscate_pending (
    account_id  VARCHAR(20) NOT NULL,
    item_code   INT         NOT NULL,              -- 9000 = 골드
    amount      INT         NOT NULL DEFAULT 0,    -- 아직 못 걷은 수량/금액
    updated_at  TIMESTAMP   NOT NULL DEFAULT CURRENT_TIMESTAMP
                            ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (account_id, item_code),
    CONSTRAINT fk_confiscate_account FOREIGN KEY (account_id)
        REFERENCES account(account_id) ON DELETE CASCADE
);

DROP PROCEDURE IF EXISTS sp_confiscate_gains;

DELIMITER $$

CREATE PROCEDURE sp_confiscate_gains(
    IN p_account VARCHAR(20),
    IN p_from    DATETIME(3),
    IN p_to      DATETIME(3),   -- NULL 이면 p_from 부터 끝까지
    IN p_apply   TINYINT        -- 0 미리보기 / 1 실제 적용
)
BEGIN
    DECLARE v_to          DATETIME(3);
    DECLARE v_exists      INT DEFAULT 0;
    DECLARE v_gold_now    INT DEFAULT 0;
    DECLARE v_gold_gained INT DEFAULT 0;
    DECLARE v_gold_taken  INT DEFAULT 0;
    DECLARE v_gold_missed  INT DEFAULT 0;
    DECLARE v_log_cnt     INT DEFAULT 0;

    DECLARE v_code INT;
    DECLARE v_need INT;
    DECLARE v_cnt  INT;
    DECLARE v_slot INT;
    DECLARE v_lid  INT;

    IF p_account IS NULL OR p_from IS NULL THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT = 'account and from-time are required';
    END IF;

    SELECT COUNT(*) INTO v_exists FROM `character` WHERE account_id = p_account;
    IF v_exists = 0 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'no such account';
    END IF;

    -- 끝 시각을 안 주면 사실상 무한대로 둔다.
    SET v_to = IFNULL(p_to, '9999-12-31 23:59:59.999');

    IF v_to < p_from THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'to-time is earlier than from-time';
    END IF;

    -- --------------------------------------------------------
    --  1. 그 구간에 "얻은 것" 집계
    --
    --   쓴 것(gold < 0, quantity < 0)은 대상이 아니다. 몰수는 이득만 걷는다.
    -- --------------------------------------------------------
    SELECT IFNULL(SUM(gold), 0) INTO v_gold_gained
      FROM mmorpg_log.game_log
     WHERE actor = p_account
       AND created_at >= p_from AND created_at <= v_to
       AND gold > 0;

    SELECT COUNT(*) INTO v_log_cnt
      FROM mmorpg_log.game_log
     WHERE actor = p_account
       AND created_at >= p_from AND created_at <= v_to
       AND (gold > 0 OR quantity > 0);

    DROP TEMPORARY TABLE IF EXISTS tmp_confiscate;
    CREATE TEMPORARY TABLE tmp_confiscate (
        item_code INT PRIMARY KEY,
        gained    INT NOT NULL DEFAULT 0,
        held      INT NOT NULL DEFAULT 0,   -- 인벤 + 내 경매 매물
        taken     INT NOT NULL DEFAULT 0,
        missed INT NOT NULL DEFAULT 0,
        processed TINYINT NOT NULL DEFAULT 0
    );

    INSERT INTO tmp_confiscate (item_code, gained)
    SELECT item_code, SUM(quantity)
      FROM mmorpg_log.game_log
     WHERE actor = p_account
       AND created_at >= p_from AND created_at <= v_to
       AND quantity > 0
       AND item_code <> 9000
     GROUP BY item_code;

    -- 지금 얼마나 갖고 있나. 경매에 올려둔 것도 아직 본인 재산이므로 센다.
    UPDATE tmp_confiscate t
       SET held =
           IFNULL((SELECT SUM(i.count) FROM inventory i
                    WHERE i.account_id = p_account AND i.item_code = t.item_code), 0)
         + IFNULL((SELECT SUM(a.count) FROM auction a
                    WHERE a.seller_name = p_account AND a.item_code = t.item_code), 0);

    --  ★ 여기가 최선 회수다. 있는 만큼만 걷고 모자란 만큼은 따로 적는다.
    UPDATE tmp_confiscate
       SET taken     = LEAST(gained, held),
           missed = gained - LEAST(gained, held);

    SELECT gold INTO v_gold_now FROM `character` WHERE account_id = p_account;
    SET v_gold_taken = LEAST(v_gold_gained, v_gold_now);
    SET v_gold_missed = v_gold_gained - v_gold_taken;

    -- --------------------------------------------------------
    --  2. 적용
    -- --------------------------------------------------------
    IF p_apply = 1 THEN
        START TRANSACTION;

        UPDATE `character` SET gold = gold - v_gold_taken WHERE account_id = p_account;

        -- 아이템은 인벤 먼저, 모자라면 경매 매물에서 마저 걷는다.
        WHILE EXISTS (SELECT 1 FROM tmp_confiscate WHERE processed = 0 AND taken > 0) DO
            SELECT item_code, taken INTO v_code, v_need
              FROM tmp_confiscate WHERE processed = 0 AND taken > 0 LIMIT 1;

            --  인벤에서 먼저. 같은 아이템이 여러 슬롯에 흩어져 있을 수 있으므로
            --  앞 슬롯부터 훑는다. LEAVE 로 빠져나가야 v_need(남은 수량)가 보존돼
            --  아래 경매 단계로 이어진다.
            inv_loop: WHILE v_need > 0 DO
                SET v_slot = NULL;
                SELECT slot, count INTO v_slot, v_cnt
                  FROM inventory
                 WHERE account_id = p_account AND item_code = v_code AND count > 0
                 ORDER BY slot LIMIT 1;

                IF v_slot IS NULL THEN
                    LEAVE inv_loop;          -- 인벤엔 더 없다. 남은 만큼은 매물에서
                ELSEIF v_cnt <= v_need THEN
                    DELETE FROM inventory WHERE account_id = p_account AND slot = v_slot;
                    SET v_need = v_need - v_cnt;
                ELSE
                    UPDATE inventory SET count = count - v_need
                     WHERE account_id = p_account AND slot = v_slot;
                    SET v_need = 0;
                END IF;
            END WHILE;

            --  경매에 올려둔 것도 아직 본인 재산이므로 회수 대상이다.
            --  매물 수량이 0 이 되면 매물 자체를 내린다.
            auc_loop: WHILE v_need > 0 DO
                SET v_lid = NULL;
                SELECT listing_id, count INTO v_lid, v_cnt
                  FROM auction
                 WHERE seller_name = p_account AND item_code = v_code AND count > 0
                 ORDER BY listing_id LIMIT 1;

                IF v_lid IS NULL THEN
                    LEAVE auc_loop;          -- 더 걷을 데가 없다. 나머지는 미회수로 남는다
                ELSEIF v_cnt <= v_need THEN
                    DELETE FROM auction WHERE listing_id = v_lid;
                    SET v_need = v_need - v_cnt;
                ELSE
                    UPDATE auction SET count = count - v_need WHERE listing_id = v_lid;
                    SET v_need = 0;
                END IF;
            END WHILE;

            UPDATE tmp_confiscate SET processed = 1 WHERE item_code = v_code;
        END WHILE;

        --  몰수한 사실을 로그에 남긴다.

        INSERT INTO mmorpg_log.game_log
            (log_type, actor, target, item_code, quantity, gold, gold_balance, detail)
        VALUES
            ('ADMIN_ROLLBACK', p_account, NULL, 0, 0,
             -v_gold_taken, v_gold_now - v_gold_taken,
             CONCAT('confiscate ', DATE_FORMAT(p_from, '%Y-%m-%d %H:%i:%s'),
                    ' missed_gold=', v_gold_missed));

        INSERT INTO mmorpg_log.game_log
            (log_type, actor, target, item_code, quantity, gold, gold_balance, detail)
        SELECT 'ADMIN_ROLLBACK', p_account, NULL, item_code, -taken, 0,
               v_gold_now - v_gold_taken,
               CONCAT('confiscate missed_item=', missed)
          FROM tmp_confiscate
         WHERE taken > 0;

        -- 못 걷은 만큼은 빚으로 쌓아둔다(여러 번 몰수하면 누적).
        INSERT INTO confiscate_pending (account_id, item_code, amount)
        SELECT p_account, 9000, v_gold_missed FROM DUAL WHERE v_gold_missed > 0
        ON DUPLICATE KEY UPDATE amount = amount + v_gold_missed;

        INSERT INTO confiscate_pending (account_id, item_code, amount)
        SELECT p_account, item_code, missed
          FROM tmp_confiscate WHERE missed > 0
        ON DUPLICATE KEY UPDATE amount = amount + VALUES(amount);

        COMMIT;
    END IF;

    -- --------------------------------------------------------
    --  3. 결과 보고
    -- --------------------------------------------------------
    SELECT
        p_account                                   AS account,
        p_from                                      AS range_from,
        IF(p_to IS NULL, '(끝까지)', CAST(p_to AS CHAR)) AS range_to,
        IF(p_apply = 1, 'APPLIED', 'PREVIEW ONLY')  AS mode,
        v_log_cnt                                   AS gain_logs,
        v_gold_gained                               AS gold_gained,
        v_gold_now                                  AS gold_before,
        v_gold_taken                                AS gold_taken,
        v_gold_now - v_gold_taken                   AS gold_after,
        v_gold_missed                                AS gold_missed;

    SELECT item_code, gained, held, taken, missed
      FROM tmp_confiscate
     ORDER BY item_code;

    SELECT item_code, amount, updated_at
      FROM confiscate_pending
     WHERE account_id = p_account
     ORDER BY item_code;

    DROP TEMPORARY TABLE IF EXISTS tmp_confiscate;
END$$

DELIMITER ;


-- ============================================================
--  5-3. 전면 복구(PITR) 후 장부 재연결 + 보상 지급
--    PITR 은 게임 서버를 거치지 않고 DB 파일 수준에서 되돌리므로
--    로그가 한 줄도 안 남는다.
--
--    그 결과 복구 직후에는 장부와 현실이 어긋나 있다.
--
--    이 상태에서 플레이어가 아무 거래나 하면 그 다음 로그에서
--        직전 잔액 + 증감 != 이번 잔액
--    이 되어 연속성 검사가 터진다. 복구 대상 전원이 그렇다.
--    보상만 지급하고 끝내면 그 보상 자체가 복제 버그로 오탐된다.
--
--    로그의 마지막 잔액 != 현재 DB 골드  인 계정이 곧 백섭이 건드린 계정이다.
--    계정 목록을 넘겨받거나 이름으로 거르지 않는다.
--
--      보상액 = 그 구간의 DROP_GAIN 골드 합계
--
--    교환(상점/경매)은 백섭이 양쪽을 다 되돌렸다. 판 아이템은 인벤에
--    돌아왔고 쓴 골드는 지갑에 돌아왔다. 거기에 보상까지 하면
--    물건도 갖고 돈도 갖는 이중 지급이 된다.
--    반면 드롭은 되돌릴 반대편이 없다. 몬스터를 되살릴 수는 없으므로
--    사냥한 시간과 노력이 순수하게 사라진다. 이것만 보상 대상이다.
--
--       오염된 골드를 따로 판별하지 않는다.
-- ============================================================

DROP PROCEDURE IF EXISTS sp_compensate_rollback;

DELIMITER $$

CREATE PROCEDURE sp_compensate_rollback(
    IN p_from    DATETIME(3),   -- 보상 산정 구간 시작 (= 백섭으로 되돌린 시점)
    IN p_exclude VARCHAR(20),   -- 보상에서 제외할 계정(최초 감염자). 없으면 NULL
    IN p_apply   TINYINT        -- 0 미리보기 / 1 실제 지급
)
BEGIN
    DECLARE v_accounts   INT DEFAULT 0;
    DECLARE v_paid_cnt   INT DEFAULT 0;
    DECLARE v_delta_sum  BIGINT DEFAULT 0;
    DECLARE v_comp_sum   BIGINT DEFAULT 0;
    DECLARE v_violations INT DEFAULT 0;
    DECLARE v_before     INT DEFAULT 0;

    IF p_from IS NULL THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'from-time is required';
    END IF;

    --  지금 이미 있는 연속성 위반 건수를 먼저 세어 둔다.

    SELECT COUNT(*) INTO v_before FROM (
        SELECT gold, gold_balance,
               LAG(gold_balance) OVER (PARTITION BY actor
                                       ORDER BY created_at, log_id) AS prev
          FROM mmorpg_log.game_log
         WHERE log_type <> 'AUCTION_SOLD'
    ) t
    WHERE prev IS NOT NULL AND prev + gold <> gold_balance;

    -- --------------------------------------------------------
    --  1. 대상과 금액을 모은다
    --
    --   last_bal  : 로그가 말하는 마지막 잔액
    --   db_gold   : 실제 현재 골드
    --   delta     : 백섭이 없앤 금액 (= last_bal - db_gold)
    --   comp_gold : 그 구간에 사냥으로 번 골드
    -- --------------------------------------------------------
    DROP TEMPORARY TABLE IF EXISTS tmp_comp;
    CREATE TEMPORARY TABLE tmp_comp (
        actor     VARCHAR(20) PRIMARY KEY,
        last_bal  INT NOT NULL,
        db_gold   INT NOT NULL,
        delta     INT NOT NULL,
        comp_gold INT NOT NULL DEFAULT 0,
        comp_cnt  INT NOT NULL DEFAULT 0
    );

    INSERT INTO tmp_comp (actor, last_bal, db_gold, delta)
    SELECT c.account_id, l.bal, c.gold, l.bal - c.gold
      FROM `character` c
      JOIN (SELECT g.actor, g.gold_balance AS bal
              FROM mmorpg_log.game_log g
              JOIN (SELECT actor, MAX(log_id) AS mx
                      FROM mmorpg_log.game_log
                     WHERE log_type <> 'AUCTION_SOLD'
                     GROUP BY actor) m
                ON m.actor = g.actor AND m.mx = g.log_id) l
        ON l.actor = c.account_id
     WHERE l.bal <> c.gold;          -- 백섭이 건드린 계정만

    --  보상액: 구간의 골드 드롭 합계. 최초 감염자는 0 으로 둔다.
    UPDATE tmp_comp t
       SET comp_gold = IF(t.actor <=> p_exclude, 0,
             IFNULL((SELECT SUM(g.gold) FROM mmorpg_log.game_log g
                      WHERE g.actor = t.actor AND g.created_at >= p_from
                        AND g.log_type = 'DROP_GAIN' AND g.item_code = 9000), 0)),
           comp_cnt  = IF(t.actor <=> p_exclude, 0,
             IFNULL((SELECT COUNT(*) FROM mmorpg_log.game_log g
                      WHERE g.actor = t.actor AND g.created_at >= p_from
                        AND g.log_type = 'DROP_GAIN' AND g.item_code = 9000), 0));

    SELECT COUNT(*), SUM(delta), SUM(comp_gold), SUM(comp_gold > 0)
      INTO v_accounts, v_delta_sum, v_comp_sum, v_paid_cnt
      FROM tmp_comp;

    -- --------------------------------------------------------
    --  2. 적용
    --
    --   순서가 중요하다. (1) 을 먼저 통째로 넣어야 log_id 가 작아지고,
    --   연속성 검사(created_at, log_id 순)에서 (1) -> (2) 로 읽힌다.
    -- --------------------------------------------------------
    IF p_apply = 1 THEN
        START TRANSACTION;

        --  (1) 백섭을 장부에 반영. 골드는 이미 바뀌어 있으므로 기록만 한다.
        INSERT INTO mmorpg_log.game_log
            (log_type, actor, target, item_code, quantity, gold, gold_balance, detail)
        SELECT 'ADMIN_ROLLBACK', actor, NULL, 0, 0,
               -delta, db_gold,
               CONCAT('pitr rollback to ', DATE_FORMAT(p_from, '%Y-%m-%d %H:%i:%s'))
          FROM tmp_comp
         ORDER BY actor;

        --  (2) 보상 지급
        UPDATE `character` c
          JOIN tmp_comp t ON t.actor = c.account_id
           SET c.gold = c.gold + t.comp_gold
         WHERE t.comp_gold > 0;

        INSERT INTO mmorpg_log.game_log
            (log_type, actor, target, item_code, quantity, gold, gold_balance, detail)
        SELECT 'ADMIN_COMPENSATE', actor, NULL, 0, 0,
               comp_gold, db_gold + comp_gold,
               CONCAT('rollback compensation drops=', comp_cnt)
          FROM tmp_comp
         WHERE comp_gold > 0
         ORDER BY actor;

        COMMIT;
    END IF;

    -- --------------------------------------------------------
    --  3. 결과 보고
    -- --------------------------------------------------------
    SELECT IF(p_apply = 1, 'APPLIED', 'PREVIEW ONLY') AS mode,
           p_from                                     AS range_from,
           IFNULL(p_exclude, '(none)')                AS excluded,
           v_accounts                                 AS accounts,
           v_delta_sum                                AS rollback_total,
           v_paid_cnt                                 AS paid_accounts,
           v_comp_sum                                 AS compensation_total;

    SELECT actor, last_bal, db_gold, delta, comp_cnt, comp_gold,
           db_gold + comp_gold AS gold_after
      FROM tmp_comp
     ORDER BY comp_gold DESC, actor
     LIMIT 15;

    --  적용했다면 장부가 다시 이어졌는지 확인한다. 0 이어야 정상.
    IF p_apply = 1 THEN
        SELECT COUNT(*) INTO v_violations FROM (
            SELECT gold, gold_balance,
                   LAG(gold_balance) OVER (PARTITION BY actor
                                           ORDER BY created_at, log_id) AS prev
              FROM mmorpg_log.game_log
             WHERE log_type <> 'AUCTION_SOLD'
        ) t
        WHERE prev IS NOT NULL AND prev + gold <> gold_balance;

        SELECT v_before                AS violations_before,
               v_violations            AS violations_after,
               v_violations - v_before AS newly_broken,
               IF(v_violations <= v_before, 'OK - ledger reconnected',
                  'CHECK - this run broke the ledger') AS ledger_check;
    END IF;

    DROP TEMPORARY TABLE IF EXISTS tmp_comp;
END$$

DELIMITER ;


-- ============================================================
--  5-5. 과실 케어 지급 (운영자가 특정 계정에 아이템/골드를 준다)
--
--   오프라인 계정에만 쓸 것
--   로그를 반드시 남긴다
--
--  아이템 코드 = 카테고리*1000 + 세부번호 (클라 Item_define.h 기준)
--     1000~1005  포션        2000~2001  스크롤
--     3000~3038  장비        4000~4009  기타
--     9000       골드 (로그 전용 코드. 인벤에 넣는 코드가 아니다)
-- ============================================================

DROP PROCEDURE IF EXISTS sp_admin_grant;

DELIMITER $$

CREATE PROCEDURE sp_admin_grant(
    IN p_account   VARCHAR(20),   -- 받을 계정
    IN p_item_code INT,           -- 줄 아이템 코드. 0 이면 골드만 준다
    IN p_count     INT,           -- 아이템 개수
    IN p_gold      INT,           -- 줄 골드. 0 이면 아이템만 준다
    IN p_gold_take INT,           -- 도로 걷을 골드. 0 이면 회수 없음
    IN p_reason    VARCHAR(64),   -- 사유(문의번호 등). 로그 detail 에 남는다
    IN p_apply     TINYINT        -- 0 미리보기 / 1 실제 지급
)
BEGIN
    --  인벤 규격은 클라/서버와 같아야 한다
    DECLARE c_slots     INT DEFAULT 40;
    DECLARE c_stack     INT DEFAULT 99;

    DECLARE v_exists    INT DEFAULT 0;
    DECLARE v_gold_now  INT DEFAULT 0;
    DECLARE v_have      INT DEFAULT 0;   -- 지금 갖고 있는 같은 코드 개수
    DECLARE v_used      INT DEFAULT 0;   -- 사용 중인 슬롯 수
    DECLARE v_free      INT DEFAULT 0;   -- 남은 슬롯 수
    DECLARE v_need      INT DEFAULT 0;   -- 이번 지급에 필요한 새 슬롯 수
    DECLARE v_room      INT DEFAULT 0;   -- 기존 스택에 더 담을 수 있는 양
    DECLARE v_isequip   TINYINT DEFAULT 0;
    DECLARE v_last_bal  INT DEFAULT NULL;
    DECLARE v_net_gold  INT DEFAULT 0;   -- 지급 - 회수. 이 값이 장부에 적힌다
    DECLARE v_left      INT DEFAULT 0;
    DECLARE v_slot      INT DEFAULT 0;
    DECLARE v_cnt       INT DEFAULT 0;
    DECLARE v_put       INT DEFAULT 0;

    -- --------------------------------------------------------
    --  0. 입력 검사 - 잘못된 값으로 DB 를 건드리지 않는다
    -- --------------------------------------------------------
    IF p_account IS NULL OR p_account = '' THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'account is required';
    END IF;

    SELECT COUNT(*) INTO v_exists FROM `character` WHERE account_id = p_account;
    IF v_exists = 0 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'no such account';
    END IF;

    SET p_item_code = IFNULL(p_item_code, 0);
    SET p_count     = IFNULL(p_count, 0);
    SET p_gold      = IFNULL(p_gold, 0);
    SET p_gold_take = IFNULL(p_gold_take, 0);

    --  회수는 음수가 아니라 '걷을 금액' 을 양수로 받는다.
    --  부호를 사람이 직접 넣게 하면 반대로 넣어 골드를 두 배로 주는 사고가 난다.
    IF p_count < 0 OR p_gold < 0 OR p_gold_take < 0 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'negative amount is not allowed';
    END IF;

    SET v_net_gold = p_gold - p_gold_take;

    --  회수는 이 프로시저의 일이 아니다(메뉴 5/8 이 한다).
    IF p_item_code = 9000 THEN
        SIGNAL SQLSTATE '45000'
            SET MESSAGE_TEXT = 'item_code 9000 is gold - use the gold field instead';
    END IF;

    IF p_item_code <> 0 AND NOT (
           (p_item_code BETWEEN 1000 AND 1005)     -- 포션
        OR (p_item_code BETWEEN 2000 AND 2001)     -- 스크롤
        OR (p_item_code BETWEEN 3000 AND 3038)     -- 장비
        OR (p_item_code BETWEEN 4000 AND 4009))    -- 기타
    THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'unknown item_code';
    END IF;

    IF p_item_code <> 0 AND p_count <= 0 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'item count must be >= 1';
    END IF;

    IF (p_item_code = 0 OR p_count = 0) AND p_gold = 0 AND p_gold_take = 0 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'nothing to grant';
    END IF;

    -- --------------------------------------------------------
    --  1. 현재 상태와 들어갈 자리를 계산한다
    -- --------------------------------------------------------
    SELECT gold INTO v_gold_now FROM `character` WHERE account_id = p_account;

    SELECT COUNT(*) INTO v_used FROM inventory WHERE account_id = p_account;
    SET v_free = c_slots - v_used;

    SET v_isequip = IF(p_item_code BETWEEN 3000 AND 3999, 1, 0);

    IF p_item_code <> 0 THEN
        SELECT IFNULL(SUM(count), 0) INTO v_have
          FROM inventory WHERE account_id = p_account AND item_code = p_item_code;

        IF v_isequip = 1 THEN
            --  장비는 겹쳐지지 않는다. 한 칸에 하나씩.
            SET v_room = 0;
            SET v_need = p_count;
        ELSE
            --  기존 스택의 빈 자리부터 채우고, 모자라는 만큼만 새 슬롯을 쓴다.
            SELECT IFNULL(SUM(c_stack - count), 0) INTO v_room
              FROM inventory
             WHERE account_id = p_account AND item_code = p_item_code AND count < c_stack;

            SET v_need = CEIL(GREATEST(p_count - v_room, 0) / c_stack);
        END IF;
    END IF;

    --  마지막 로그 잔액(연속성을 이어 적기 위해)
    SELECT gold_balance INTO v_last_bal
      FROM mmorpg_log.game_log
     WHERE actor = p_account AND log_type <> 'AUCTION_SOLD'
     ORDER BY created_at DESC, log_id DESC
     LIMIT 1;

    -- --------------------------------------------------------
    --  2. 미리보기 (p_apply = 0)
    -- --------------------------------------------------------
    IF p_apply <> 1 THEN
        SELECT p_account                       AS account,
               v_gold_now                      AS gold_now,
               p_gold                          AS gold_grant,
               p_gold_take                     AS gold_take,
               v_net_gold                      AS gold_net,
               v_gold_now + v_net_gold         AS gold_after,
               p_item_code                     AS item_code,
               v_have                          AS item_now,
               p_count                         AS item_grant,
               v_have + p_count                AS item_after,
               v_free                          AS free_slots,
               v_need                          AS slots_needed,
               IF(v_need > v_free, 'FAIL - not enough inventory slots', 'OK') AS slot_check,
               IF(v_gold_now + v_net_gold < 0,
                  'FAIL - not enough gold to take back', 'OK')                AS gold_check,
               IFNULL(v_last_bal, v_gold_now)  AS log_balance_now,
               IFNULL(v_last_bal, v_gold_now) + v_net_gold AS log_balance_after;
    ELSE
        -- --------------------------------------------------------
        --  3. 실제 지급
        -- --------------------------------------------------------
        IF v_need > v_free THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'not enough inventory slots';
        END IF;

        --  골드를 마이너스로 만들지 않는다.
        IF v_gold_now + v_net_gold < 0 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'not enough gold to take back';
        END IF;

        START TRANSACTION;

        IF v_net_gold <> 0 THEN
            UPDATE `character` SET gold = gold + v_net_gold WHERE account_id = p_account;
        END IF;

        IF p_item_code <> 0 AND p_count > 0 THEN
            SET v_left = p_count;

            --  (1) 장비가 아니면 기존 스택의 빈 자리부터 채운다.
            IF v_isequip = 0 THEN
                stack_loop: WHILE v_left > 0 DO
                    SET v_slot = NULL;
                    SELECT slot, count INTO v_slot, v_cnt
                      FROM inventory
                     WHERE account_id = p_account
                       AND item_code = p_item_code AND count < c_stack
                     ORDER BY slot LIMIT 1;

                    IF v_slot IS NULL THEN
                        LEAVE stack_loop;
                    END IF;

                    SET v_put = LEAST(v_left, c_stack - v_cnt);
                    UPDATE inventory SET count = count + v_put
                     WHERE account_id = p_account AND slot = v_slot;
                    SET v_left = v_left - v_put;
                END WHILE;
            END IF;

            --  (2) 남은 만큼은 빈 슬롯에 새로 넣는다.
            --      장비는 한 칸에 1개, 나머지는 한 칸에 99 까지.
            new_slot_loop: WHILE v_left > 0 DO
                --  0번부터 차례대로 확인해서 가장 앞의 빈 슬롯을 찾는다.
                SET v_slot = 0;
                find_slot_loop: WHILE v_slot < c_slots DO
                    IF NOT EXISTS (SELECT 1
                                     FROM inventory
                                    WHERE account_id = p_account
                                      AND slot = v_slot) THEN
                        LEAVE find_slot_loop;
                    END IF;

                    SET v_slot = v_slot + 1;
                END WHILE;

                IF v_slot >= c_slots THEN
                    ROLLBACK; --트랜잭션 전체 롤백(빈 공간이 없음)
                    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'inventory full';
                END IF;

                SET v_put = IF(v_isequip = 1, 1, LEAST(v_left, c_stack)); -- 만약 장비면 한칸 아니면 스택

                INSERT INTO inventory (account_id, slot, item_code, count)
                VALUES (p_account, v_slot, p_item_code, v_put);

                SET v_left = v_left - v_put;
            END WHILE;
        END IF;

        --  로그. 골드든 아이템이든 한 줄로 남긴다.
        INSERT INTO mmorpg_log.game_log
            (log_type, actor, target, item_code, quantity, gold, gold_balance, detail)
        VALUES ('ADMIN_GRANT', p_account, NULL,
                p_item_code, p_count, v_net_gold,
                IFNULL(v_last_bal, v_gold_now) + v_net_gold,
                CONCAT('admin grant: ',
                       IFNULL(NULLIF(p_reason, ''), 'no reason given'),
                       IF(p_gold_take > 0,
                          CONCAT(' (give ', p_gold, ' / take ', p_gold_take, ')'), '')));

        COMMIT;

        SELECT p_account                   AS account,
               p_item_code                 AS item_code,
               p_count                     AS item_granted,
               p_gold                      AS gold_granted,
               p_gold_take                 AS gold_taken,
               v_net_gold                  AS gold_net,
               (SELECT gold FROM `character` WHERE account_id = p_account) AS gold_after,
               (SELECT IFNULL(SUM(count), 0) FROM inventory
                 WHERE account_id = p_account AND item_code = p_item_code) AS item_after,
               (SELECT COUNT(*) FROM inventory WHERE account_id = p_account) AS slots_used,
               'granted and logged (ADMIN_GRANT)' AS result;
    END IF;
END$$

DELIMITER ;


-- ============================================================
--  6. 시드 데이터
--    - test1 / test2 계정
--    - 판매자가 '경매장' 인 기본 매물
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
-- ============================================================

CREATE DATABASE IF NOT EXISTS mmorpg_log
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_general_ci;

-- ------------------------------------------------------------
--  game_log : 골드/아이템이 생기고 사라지고 옮겨간 모든 기록
-- 재화가 움직이는 모든 경로는 여기를 거친다.
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS mmorpg_log.game_log (
    log_id      BIGINT       NOT NULL AUTO_INCREMENT,

    log_type    VARCHAR(24)  NOT NULL,   -- 아래 '로그 타입' 목록 참고
    actor       VARCHAR(20)  NOT NULL,   -- 행위 주체(계정 ID)
    target      VARCHAR(20)  NULL,       -- 거래 상대(경매 구매면 판매자)

    item_code   INT          NOT NULL DEFAULT 0,   -- 골드만 움직이면 0
    quantity    INT          NOT NULL DEFAULT 0,   -- 아이템 증감(부호 포함)
    gold        INT          NOT NULL DEFAULT 0,   -- 골드 증감(부호 포함)

    -- ---- 검증용 잔액 ----
    gold_balance INT         NOT NULL DEFAULT 0,

    detail      VARCHAR(128) NULL,       -- 사람이 읽을 부가 정보(매물번호 등)

    -- DATETIME(3) = 밀리초까지. 같은 초에 여러 거래가 일어나므로 필요.
    created_at  DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),

    PRIMARY KEY (log_id),
    INDEX idx_actor_time (actor, created_at),   -- 유저별 조회(문의/보상)
    INDEX idx_type_time  (log_type, created_at) -- 종류별 집계(이상 탐지)
);

-- ------------------------------------------------------------
--  로그 타입 목록
--
--   경매
--     AUCTION_LIST     등록      아이템 -
--     AUCTION_BUY      구매      아이템 +, 골드 -
--     AUCTION_SOLD     판매성사  (판매자 기준, 아직 미수령)
--     AUCTION_COLLECT  대금수령  골드 +
--     AUCTION_CANCEL   취소      아이템 +
--
--   재화 생성
--     DROP_GAIN        몬스터 드롭
--     QUEST_REWARD     퀘스트 보상
--
--   재화 소멸
--     SHOP_BUY         상점 구매  골드 -
--     SHOP_SELL        상점 판매  골드 +
--     ITEM_USE         포션/스크롤 소비  아이템 -
--
--   기타
--     LEVEL_UP
--     ADMIN_ROLLBACK   운영자가 계정을 과거 시점으로 되돌림 (sp_rollback_account)

--     ADMIN_GRANT      과실 케어 - 운영자가 특정 계정에 아이템/골드 지급 (sp_admin_grant)

--     ADMIN_COMPENSATE 백섭으로 잃은 정상 소득을 돌려줌 (sp_compensate_rollback)

--
-- ------------------------------------------------------------

-- 서버 계정에는 INSERT 만 준다.
--  서버는 로그를 남길 수만 있고 고치거나 지울 수 없다.
GRANT INSERT ON mmorpg_log.* TO 'mmo_server'@'localhost';

-- ------------------------------------------------------------
--  분석 전용 계정 (읽기만)
-- ------------------------------------------------------------
CREATE USER IF NOT EXISTS 'mmo_analyst'@'localhost' IDENTIFIED BY 'analyst1234';
GRANT SELECT ON mmorpg.*     TO 'mmo_analyst'@'localhost';
GRANT SELECT ON mmorpg_log.* TO 'mmo_analyst'@'localhost';

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
