-- ============================================================
--  마이그레이션: 퀵슬롯 테이블 추가 (2026-07-14)
--
--  CREATE TABLE IF NOT EXISTS 라서 기존 DB에 그대로 실행해도 안전하고,
--  두 번 실행해도 아무 일도 일어나지 않는다.
--  (schema.sql 에도 같은 정의가 들어 있으므로 새 PC는 이 파일 불필요)
--
--  실행:  mysql -u root -p < db/migration_2026-07-14_quickslot.sql
--  주의: 이 파일 실행 후 procedures.sql 을 다시 실행해야 sp_login 이
--        퀵슬롯 결과셋(4번째)을 돌려준다.
-- ============================================================

USE mmorpg;

CREATE TABLE IF NOT EXISTS quickslot (
    account_id  VARCHAR(20)  NOT NULL,
    slot        TINYINT      NOT NULL,   -- 0~7 (0~3 소비아이템, 4~7 스킬 예정)
    item_code   INT          NOT NULL,
    PRIMARY KEY (account_id, slot),
    CONSTRAINT fk_quickslot_account
        FOREIGN KEY (account_id) REFERENCES account (account_id)
        ON DELETE CASCADE
);

-- 확인
SELECT account_id, slot, item_code FROM quickslot ORDER BY account_id, slot;
