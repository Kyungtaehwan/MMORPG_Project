-- ============================================================
--  MMORPG_Project  저장 프로시저 (Stored Procedures)
--  - 서버가 CALL 로 호출하는 "DB 안의 함수"들.
--  - 실행: schema.sql / seed.sql 먼저 실행된 상태에서 이 파일 전체 실행.
--  - 프로시저 본문 안에 세미콜론이 여러 개라, DELIMITER 로 문장 끝 기호를
--    잠시 $$ 로 바꿔서 CREATE 전체가 한 덩어리로 넘어가게 한다.
--   DELIMITER 줄 뒤에는 주석/공백을 붙이지 말 것 — 뒤 내용이 통째로
--     구분자가 되어 CREATE 가 실행되지 않는다(조용히 누락됨).
-- ============================================================

USE mmorpg;

-- ------------------------------------------------------------
--  sp_login(id, pw)
--    - id/pw 가 맞으면 그 계정의 데이터를 결과셋 4개로 돌려준다:
--        결과셋1: character  (성공 시 1행, 실패 시 0행)  - 서버는 이걸로 성공/실패 판단
--        결과셋2: inventory  (아이템 행들)
--        결과셋3: equipment  (장착 행들)
--        결과셋4: quickslot  (퀵슬롯 등록 행들)
--    - 인증 실패면 p_id 를 NULL 로 바꿔 네 조회 모두 0행이 나오게 한다.
-- ------------------------------------------------------------
DROP PROCEDURE IF EXISTS sp_login;

DELIMITER $$
CREATE PROCEDURE sp_login(
    IN p_id VARCHAR(20),                -- 입력: 아이디
    IN p_pw VARCHAR(64)                 -- 입력: 비밀번호
)
BEGIN
    DECLARE v_ok INT DEFAULT 0;         -- 인증 성공 여부(1/0)를 담을 지역변수

    -- 1) 아이디+비번이 일치하는 account 가 있는지 개수를 센다
    SELECT COUNT(*) INTO v_ok
    FROM account
    WHERE account_id = p_id AND password = p_pw;

    -- 2) 인증 실패면 p_id 를 NULL 로 - 아래 세 조회의 WHERE 가 전부 0행이 됨
    IF v_ok = 0 THEN
        SET p_id = NULL;
    END IF;

    -- 결과셋1: 캐릭터 상태 (성공 시 정확히 1행)
    SELECT zone_id, spawn_x, spawn_z, gold, level, exp
    FROM `character`
    WHERE account_id = p_id;

    -- 결과셋2: 인벤토리 (칸 순서대로)
    SELECT slot, item_code, count
    FROM inventory
    WHERE account_id = p_id
    ORDER BY slot;

    -- 결과셋3: 장비 (슬롯 순서대로)
    SELECT slot, item_code
    FROM equipment
    WHERE account_id = p_id
    ORDER BY slot;

    -- 결과셋4: 퀵슬롯 (칸 순서대로)
    SELECT slot, item_code
    FROM quickslot
    WHERE account_id = p_id
    ORDER BY slot;
END$$

DELIMITER ;
