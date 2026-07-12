-- ============================================================
--  MMORPG_Project  시드 데이터 (테스트 계정)
--  - 구조(schema.sql)와 분리된 "초기 데이터".
--  - 서버 AccountDB.h 의 하드코딩 계정 test1/test2 를 그대로 DB에 옮긴 것.
--  - 실행: schema.sql 을 먼저 실행해 테이블이 있는 상태에서 이 파일 전체 실행.
--  - 여러 번 돌려도 되도록 맨 위에서 기존 test 계정을 지우고 다시 넣는다.
-- ============================================================

USE mmorpg;

-- 재실행 안전장치: 기존 test1/test2 제거.
--   account 만 지워도 FK ON DELETE CASCADE 덕에 character/inventory/equipment 의
--   해당 계정 행이 전부 자동 삭제된다. (정규화 + CASCADE 의 실제 효과)
DELETE FROM account WHERE account_id IN ('test1', 'test2');

-- ------------------------------------------------------------
--  test1 : 마을(ZONE_TOWN=1), 골드 1000, 인벤 4칸, 무기 슬롯에 소드 장착
-- ------------------------------------------------------------
INSERT INTO account (account_id, password) VALUES ('test1', '1234');

INSERT INTO `character` (account_id, zone_id, spawn_x, spawn_z, gold)
    VALUES ('test1', 1, 12, 20, 1000);

INSERT INTO inventory (account_id, slot, item_code, count) VALUES
    ('test1', 0, 1000, 5),    -- HP포션(중) 5
    ('test1', 1, 1001, 2),    -- HP포션(대) 2
    ('test1', 2, 1004, 1),    -- 공격력 물약 1
    ('test1', 3, 3001, 1);    -- 소드1 (인벤)

INSERT INTO equipment (account_id, slot, item_code) VALUES
    ('test1', 0, 3000);       -- 슬롯0(무기)에 소드0 장착

-- ------------------------------------------------------------
--  test2 : 북쪽필드(ZONE_TEST=0), 골드 300, 인벤 2칸, 장착 장비 없음
-- ------------------------------------------------------------
INSERT INTO account (account_id, password) VALUES ('test2', '1234');

INSERT INTO `character` (account_id, zone_id, spawn_x, spawn_z, gold)
    VALUES ('test2', 0, 10, 10, 300);

INSERT INTO inventory (account_id, slot, item_code, count) VALUES
    ('test2', 0, 1002, 5),    -- MP포션(중) 5
    ('test2', 1, 1005, 1);    -- 무적 물약 1

-- test2 는 장착 장비가 없으므로 equipment 에 넣을 행이 없다.
