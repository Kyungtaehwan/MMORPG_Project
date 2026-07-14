-- ============================================================
--  마이그레이션: character 테이블에 레벨/경험치 컬럼 추가 (2026-07-14)
--
--  왜 필요한가: schema.sql 은 CREATE TABLE IF NOT EXISTS 라서,
--  이미 character 테이블이 있는 DB(=기존에 돌리던 PC)에서는 다시 실행해도
--  새 컬럼이 생기지 않는다. 기존 DB에는 이 파일을 한 번 실행할 것.
--  (DB를 처음부터 만드는 새 PC는 schema.sql 만으로 충분 - 이 파일 불필요)
--
--  실행:  mysql -u root -p < db/migration_2026-07-14_exp.sql
--  두 번 실행해도 안전하도록 컬럼 존재 여부를 먼저 확인한다.
-- ============================================================

USE mmorpg;

-- level 컬럼이 없을 때만 ADD (재실행 안전)
SET @has_level := (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = 'mmorpg' AND table_name = 'character' AND column_name = 'level'
);
SET @sql := IF(@has_level = 0,
    'ALTER TABLE `character` ADD COLUMN level INT NOT NULL DEFAULT 1',
    'SELECT "level 컬럼 이미 있음 - 건너뜀"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- exp 컬럼이 없을 때만 ADD
SET @has_exp := (
    SELECT COUNT(*) FROM information_schema.columns
    WHERE table_schema = 'mmorpg' AND table_name = 'character' AND column_name = 'exp'
);
SET @sql := IF(@has_exp = 0,
    'ALTER TABLE `character` ADD COLUMN exp INT NOT NULL DEFAULT 0',
    'SELECT "exp 컬럼 이미 있음 - 건너뜀"');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- 확인
SELECT account_id, zone_id, gold, level, exp FROM `character`;
