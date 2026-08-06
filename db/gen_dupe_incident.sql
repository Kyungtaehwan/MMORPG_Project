-- ============================================================
--  시나리오 2 - 복제 버그 확산 (백섭이 필요한 경우)
--
--  ★ 시나리오 1 과 무엇이 다른가
--
--    시나리오 1 (gen_test_data.sql)
--      버그 몬스터를 혼자 반복 사냥해서 골드를 많이 벌었다.
--      드롭은 정상 경로라 로그도 잔액도 완벽하다.
--      -> 연속성 검사로는 안 잡히고 집계로만 잡힌다.
--      -> 혼자 갖고 있으므로 그 계정만 되돌리면 끝난다.
--
--    시나리오 2 (이 파일)
--      골드가 로그를 안 거치고 그냥 생겨났다.
--      -> 장부가 어긋나므로 연속성 검사에 즉시 걸린다.
--      -> 그 골드로 100 명에게서 물건을 사서 대금이 퍼졌다.
--      -> 받은 사람들의 로그는 완벽하게 정상이라 "누가 오염됐는지"를
--         골라낼 수가 없다. 그래서 전면 백섭을 검토하게 된다.
--
--    두 사건의 탐지 방법이 다르고, 그 차이가 조치를 가른다는 것이
--    이 설계의 핵심이다.
--
--  ★ 사건 내용
--    2026-08-04 02:00, 서버 버그로 dup001 의 골드가 로그 없이 500,000 늘었다.
--    dup001 은 02:10 부터 경매장에서 100 명에게 물건을 사들이며
--    그 골드를 300,000 만큼 시장에 풀었다.
--
--  ★ 실행
--    root 로 전체 실행. 재실행 안전(dup% 계정과 그 로그만 지우고 다시 만든다).
--    user% (시나리오 1) 와 test1 은 건드리지 않는다.
--
--  ★ 운영 DB 에는 절대 실행하지 말 것. 로그를 위조하는 스크립트다.
-- ============================================================

USE mmorpg;

SET NAMES utf8mb4 COLLATE utf8mb4_general_ci;

SET @old_safe_updates := @@SQL_SAFE_UPDATES;
SET SQL_SAFE_UPDATES = 0;

-- 사건 시각과 규모. 한군데서 바꾸면 전체가 따라간다.
SET @incident_at   := '2026-08-04 02:00:00';
SET @dupe_gold     := 500000;   -- 로그 없이 생겨난 골드
SET @spread_count  := 100;      -- 대금을 받은 사람 수
SET @spread_price  := 3000;     -- 건당 거래액


-- ============================================================
--  1. 이전 실행분 정리
-- ============================================================
DELETE FROM mmorpg_log.game_log WHERE actor LIKE 'dup%' OR target LIKE 'dup%';
DELETE FROM account            WHERE account_id LIKE 'dup%';


-- ============================================================
--  2. 계정 120 개
-- ============================================================
DROP TEMPORARY TABLE IF EXISTS tmp_num;
DROP TEMPORARY TABLE IF EXISTS tmp_seq;
CREATE TEMPORARY TABLE tmp_num (n INT PRIMARY KEY);
CREATE TEMPORARY TABLE tmp_seq (n INT PRIMARY KEY);

INSERT INTO tmp_num (n)
WITH RECURSIVE seq AS (
    SELECT 1 AS n UNION ALL SELECT n + 1 FROM seq WHERE n < 200
)
SELECT n FROM seq;

INSERT INTO tmp_seq (n) SELECT n FROM tmp_num;

INSERT INTO account (account_id, password)
SELECT CONCAT('dup', LPAD(n, 3, '0')), '1234' FROM tmp_num WHERE n <= 120;

INSERT INTO `character` (account_id, zone_id, spawn_x, spawn_z, gold, level, exp)
SELECT CONCAT('dup', LPAD(n, 3, '0')), 0, 12, 20, 0, 1 + (n % 10), 0
  FROM tmp_num WHERE n <= 120;


-- ============================================================
--  3. 사건 목록
-- ============================================================
DROP TEMPORARY TABLE IF EXISTS tmp_ev2;
CREATE TEMPORARY TABLE tmp_ev2 (
    actor      VARCHAR(20)  NOT NULL,
    seq        INT          NOT NULL,
    log_type   VARCHAR(24)  NOT NULL,
    item_code  INT          NOT NULL,
    quantity   INT          NOT NULL,
    gold       INT          NOT NULL,
    target     VARCHAR(20)  NULL,
    detail     VARCHAR(128) NULL,
    created_at DATETIME(3)  NOT NULL,
    INDEX idx_actor_time (actor, created_at, seq)
);

-- ------------------------------------------------------------
--  3-1. 평범한 플레이 (120 명 전원, 08-01 ~ 08-03)
--       사건 전에 정상 활동이 있어야 "언제부터 이상해졌나"를 말할 수 있다.
-- ------------------------------------------------------------
INSERT INTO tmp_ev2 (actor, seq, log_type, item_code, quantity, gold, target, detail, created_at)
SELECT
    e.actor, e.seq,
    CASE WHEN e.t < 55 THEN 'DROP_GAIN'
         WHEN e.t < 75 THEN 'SHOP_BUY'
         WHEN e.t < 90 THEN 'SHOP_SELL'
         ELSE               'ITEM_USE' END,
    CASE WHEN e.t < 55 THEN 9000 ELSE e.code END,
    CASE WHEN e.t < 55 THEN 0
         WHEN e.t < 75 THEN 1
         ELSE               -1 END,
    CASE WHEN e.t < 55 THEN 20 + (e.m % 61)
         WHEN e.t < 75 THEN -30
         WHEN e.t < 90 THEN 15
         ELSE               0 END,
    NULL, NULL,
    TIMESTAMPADD(SECOND,
                 (CRC32(e.actor) % 172800) + e.seq * (900 + (e.m % 3600)),
                 '2026-08-01 00:00:00')
FROM (
    SELECT CONCAT('dup', LPAD(a.n, 3, '0')) AS actor,
           s.n AS seq,
           CAST(CRC32(CONCAT('dt', a.n, '-', s.n)) % 100 AS SIGNED) AS t,
           CAST(CRC32(CONCAT('dm', a.n, '-', s.n)) % 997 AS SIGNED) AS m,
           ELT(1 + (CRC32(CONCAT('dc', a.n, '-', s.n)) % 4),
               1000, 1001, 1002, 4004) AS code
      FROM tmp_num a
      JOIN tmp_seq s ON s.n <= 10 + (CRC32(CONCAT('dcnt', a.n)) % 16)
     WHERE a.n <= 120
) e;

-- ------------------------------------------------------------
--  3-2. 전파 - dup001 이 100 명에게서 물건을 사들인다
--
--   ★ 여기서 만들어지는 로그는 전부 정상이다.
--     구매도 판매도 대금 수령도 서버가 제대로 기록한 것이다.
--     문제는 "그 돈이 어디서 왔는가" 뿐이다.
--     받은 사람 입장에서는 평범한 판매이고, 로그상 흠잡을 데가 없다.
--     이것이 선별 환수를 불가능하게 만드는 이유다.
-- ------------------------------------------------------------

-- 구매자(dup001) 쪽: 골드가 나간다
INSERT INTO tmp_ev2 (actor, seq, log_type, item_code, quantity, gold, target, detail, created_at)
SELECT 'dup001', 5000 + n, 'AUCTION_BUY', 3001, 1, -@spread_price,
       CONCAT('dup', LPAD(n + 1, 3, '0')),
       CONCAT('listing=', 9000 + n),
       TIMESTAMPADD(SECOND, n * 100, TIMESTAMPADD(MINUTE, 10, @incident_at))
  FROM tmp_seq WHERE n <= @spread_count;

-- 판매자 쪽 1: 판매 성사. 수치는 전부 0 이고 목적은 target 으로 상대를 남기는 것.
--  (판매자가 접속 중이 아닐 수 있어 잔액을 쓸 수 없다)
INSERT INTO tmp_ev2 (actor, seq, log_type, item_code, quantity, gold, target, detail, created_at)
SELECT CONCAT('dup', LPAD(n + 1, 3, '0')), 6000 + n, 'AUCTION_SOLD', 3001, 0, 0,
       'dup001', CONCAT('listing=', 9000 + n),
       TIMESTAMPADD(SECOND, n * 100 + 1, TIMESTAMPADD(MINUTE, 10, @incident_at))
  FROM tmp_seq WHERE n <= @spread_count;

-- 판매자 쪽 2: 대금 수령. 여기서 오염된 골드가 실제로 남의 지갑에 들어간다.
INSERT INTO tmp_ev2 (actor, seq, log_type, item_code, quantity, gold, target, detail, created_at)
SELECT CONCAT('dup', LPAD(n + 1, 3, '0')), 7000 + n, 'AUCTION_COLLECT', 0, 0, @spread_price,
       NULL, CONCAT('listing=', 9000 + n),
       TIMESTAMPADD(SECOND, n * 100 + 600, TIMESTAMPADD(MINUTE, 10, @incident_at))
  FROM tmp_seq WHERE n <= @spread_count;


-- ------------------------------------------------------------
--  3-3. 사건 이후의 평범한 플레이 (08-04 02:00 ~ 14:00)
--
--   ★ 이 구간이 있어야 시나리오가 성립한다.
--     버그는 새벽 2시에 터졌지만 발견은 한참 뒤다. 그 사이에
--     아무 잘못 없는 사람들이 계속 게임을 했고, 사냥하고 거래해서
--     뭔가를 얻었다.
--
--     백섭은 이 구간을 통째로 날린다. 잘못한 사람의 이득만 골라서
--     지우는 게 아니라 시간을 되감는 것이기 때문이다.
--     그래서 백섭에는 반드시 보상이 따라붙고, 얼마를 보상할지는
--     살아남은 로그로 계산한다. 그 계산 대상이 바로 이 구간이다.
-- ------------------------------------------------------------
INSERT INTO tmp_ev2 (actor, seq, log_type, item_code, quantity, gold, target, detail, created_at)
SELECT
    e.actor, 8000 + e.seq,
    CASE WHEN e.t < 60 THEN 'DROP_GAIN'
         WHEN e.t < 80 THEN 'SHOP_BUY'
         ELSE               'SHOP_SELL' END,
    CASE WHEN e.t < 60 THEN 9000 ELSE e.code END,
    CASE WHEN e.t < 60 THEN 0
         WHEN e.t < 80 THEN 1
         ELSE               -1 END,
    CASE WHEN e.t < 60 THEN 20 + (e.m % 61)
         WHEN e.t < 80 THEN -30
         ELSE               15 END,
    NULL, NULL,
    TIMESTAMPADD(SECOND,
                 (CRC32(e.actor) % 3600) + e.seq * (600 + (e.m % 2400)),
                 @incident_at)
FROM (
    SELECT CONCAT('dup', LPAD(a.n, 3, '0')) AS actor,
           s.n AS seq,
           CAST(CRC32(CONCAT('pt', a.n, '-', s.n)) % 100 AS SIGNED) AS t,
           CAST(CRC32(CONCAT('pm', a.n, '-', s.n)) % 997 AS SIGNED) AS m,
           ELT(1 + (CRC32(CONCAT('pc', a.n, '-', s.n)) % 4),
               1000, 1001, 1002, 4004) AS code
      FROM tmp_num a
      JOIN tmp_seq s ON s.n <= 3 + (CRC32(CONCAT('pcnt', a.n)) % 10)
     WHERE a.n <= 120
) e;


-- ============================================================
--  4. game_log 에 넣기
--
--  ★ 여기가 이 스크립트의 핵심이다.
--
--    잔액은 원래 "시작 골드 + 그때까지의 증감 누적" 이어야 한다.
--    그런데 dup001 은 사건 시각 이후로 그 값에 500,000 이 더해져 있다.
--    더해준 로그는 없다. 골드가 로그를 안 거치고 생겨났기 때문이다.
--
--    그래서 사건 직후 dup001 의 첫 로그에서
--        직전 잔액 + 이번 증감  !=  이번 잔액
--    이 되고, 연속성 검사가 정확히 그 지점을 짚어낸다.
--
--    AUCTION_SOLD 만 잔액을 0 으로 둔다(서버 동작과 동일).
-- ============================================================
INSERT INTO mmorpg_log.game_log
    (log_type, actor, target, item_code, quantity, gold, gold_balance, detail, created_at)
SELECT
    log_type, actor, target, item_code, quantity, gold,
    CASE WHEN log_type = 'AUCTION_SOLD' THEN 0
         ELSE 5000
              + SUM(gold) OVER (PARTITION BY actor ORDER BY created_at, seq
                                ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW)
              + IF(actor = 'dup001' AND created_at >= @incident_at, @dupe_gold, 0)
    END,
    detail, created_at
FROM tmp_ev2
ORDER BY created_at, actor, seq;


-- ============================================================
--  5. 게임 DB 를 로그와 맞추기
-- ============================================================
UPDATE `character` c
   SET gold = IFNULL((
        SELECT g.gold_balance FROM mmorpg_log.game_log g
         WHERE g.actor = c.account_id AND g.log_type <> 'AUCTION_SOLD'
         ORDER BY g.created_at DESC, g.log_id DESC LIMIT 1), 5000)
 WHERE c.account_id LIKE 'dup%';

INSERT INTO inventory (account_id, slot, item_code, count)
SELECT x.actor,
       ROW_NUMBER() OVER (PARTITION BY x.actor ORDER BY x.item_code) - 1,
       x.item_code, x.cnt
FROM (
    SELECT c.account_id AS actor, codes.item_code,
           20 + IFNULL((SELECT SUM(g.quantity) FROM mmorpg_log.game_log g
                         WHERE g.actor = c.account_id
                           AND g.item_code = codes.item_code), 0) AS cnt
      FROM `character` c
      JOIN (SELECT 1000 AS item_code UNION ALL SELECT 1001 UNION ALL
            SELECT 1002 UNION ALL SELECT 4004) codes
     WHERE c.account_id LIKE 'dup%'
) x
WHERE x.cnt > 0;


-- ============================================================
--  6. 확인
-- ============================================================
SELECT '=== 계정 수 (120) ===' AS check_step;
SELECT COUNT(*) AS accounts FROM account WHERE account_id LIKE 'dup%';

SELECT '=== 로그 건수 ===' AS check_step;
SELECT COUNT(*) AS logs FROM mmorpg_log.game_log WHERE actor LIKE 'dup%';

SELECT '=== 연속성 위반 - dup001 한 건만 나와야 정상 ===' AS check_step;
SELECT * FROM (
    SELECT log_id, actor, log_type, gold, gold_balance, created_at,
           LAG(gold_balance) OVER (PARTITION BY actor ORDER BY created_at, log_id) AS prev
      FROM mmorpg_log.game_log
     WHERE log_type <> 'AUCTION_SOLD' AND actor LIKE 'dup%'
) t
WHERE prev IS NOT NULL AND prev + gold <> gold_balance;

SELECT '=== 오염된 골드를 받은 사람 수 (100) ===' AS check_step;
SELECT COUNT(DISTINCT actor) AS receivers
  FROM mmorpg_log.game_log
 WHERE log_type = 'AUCTION_SOLD' AND target = 'dup001';

SET SQL_SAFE_UPDATES = @old_safe_updates;
