-- ============================================================
--  시나리오 2 - 2단계 : 사건 발생과 확산
--
--  ★ 반드시 gen_dupe_stage1.sql 을 먼저 실행할 것.
--    이 파일은 1단계가 만든 "각 계정의 마지막 잔액" 을 이어받는다.
--    이어받지 않고 잔액을 새로 지어내면 120 명 전원이 연속성 검사에
--    걸려서, "딱 한 건만 어긋난다" 는 이 시나리오의 요점이 무너진다.
--
--  ============================================================
--  PITR 실습 전체 순서
--  ============================================================
--    0) dup 계정 정리        : gen_dupe_stage1.sql 의 1장이 알아서 지운다.
--                              단, 백업 전에 지워져 있어야 하므로 순서 주의.
--    1) 운영 도구 메뉴 6 백업 <- dup 계정이 0 개인 상태가 백업에 들어간다
--    2) gen_dupe_stage1.sql   <- 계정 120 개 + 평범한 플레이
--    3) ★ 지금 시각을 목표 T 로 메모하고 30 초쯤 기다린다
--    4) 이 파일 실행          <- 사건 발생. binlog 에 지금 시각으로 찍힌다
--    5) 게임 서버 종료 후, 운영 도구를 관리자 권한으로 실행
--    6) 메뉴 7 -> 시각 T -> APPLY
--
--    복구 결과 판정
--      dup 계정 0 개              -> 백업만 깔리고 binlog 재생이 안 됐다
--      dup 120 개 + 사건 있음     -> 너무 뒤까지 재생했다
--      dup 120 개 + 사건 없음     -> 성공
--
--    계정이 살아 있다는 것 자체가 재생의 증거다(백업엔 없었으므로).
--    사건이 없다는 것이 시점을 정확히 잘랐다는 증거다.
--
--  ============================================================
--  이 파일이 만드는 사건
--  ============================================================
--    dup001 의 골드가 로그 없이 500,000 늘었다.
--    dup001 은 그 골드로 경매장에서 100 명에게서 물건을 사들여
--    300,000 을 시장에 풀었다.
--    그리고 사건 이후에도 120 명 전원이 평범하게 플레이했다.
--    (이 구간이 있어야 "백섭이 무엇을 날리는가" 와 보상 산정이 성립한다)
--
--  ★ 운영 DB 에는 절대 실행하지 말 것. 로그를 위조하는 스크립트다.
-- ============================================================

USE mmorpg;

SET NAMES utf8mb4 COLLATE utf8mb4_general_ci;

SET @old_safe_updates := @@SQL_SAFE_UPDATES;
SET SQL_SAFE_UPDATES = 0;

--  로그에 써넣을 사건 시각. binlog 시각과는 별개다(조사 쿼리가 이 값을 쓴다).
SET @incident_at  := '2026-08-04 02:00:00';
SET @dupe_gold    := 500000;   -- 로그 없이 생겨난 골드
SET @spread_count := 100;      -- 대금을 받은 사람 수
SET @spread_price := 3000;     -- 건당 거래액


-- ============================================================
--  0. 1단계가 돌았는지 확인
-- ============================================================
SELECT '=== 1단계 계정 수 (120 이어야 진행 가능) ===' AS check_step;
SELECT COUNT(*) AS accounts FROM account WHERE account_id LIKE 'dup%';


-- ============================================================
--  1. 번호표
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


-- ============================================================
--  2. 1단계의 마지막 잔액을 가져온다 (이어받기)
--
--   ★ 이게 이 파일의 핵심이다.
--     2단계 로그의 잔액은 0 이나 5000 에서 다시 시작하는 게 아니라
--     "그 계정이 1단계를 마쳤을 때의 잔액" 에서 이어져야 한다.
--     AUCTION_SOLD 는 잔액을 0 으로 남기므로 기준에서 제외한다.
-- ============================================================
DROP TEMPORARY TABLE IF EXISTS tmp_base;
CREATE TEMPORARY TABLE tmp_base (
    actor    VARCHAR(20) PRIMARY KEY,
    base_bal INT NOT NULL
);

INSERT INTO tmp_base (actor, base_bal)
SELECT c.account_id,
       IFNULL((SELECT g.gold_balance FROM mmorpg_log.game_log g
                WHERE g.actor = c.account_id AND g.log_type <> 'AUCTION_SOLD'
                ORDER BY g.created_at DESC, g.log_id DESC LIMIT 1), 5000)
  FROM `character` c
 WHERE c.account_id LIKE 'dup%';


-- ============================================================
--  3. 사건 목록
-- ============================================================
DROP TEMPORARY TABLE IF EXISTS tmp_ev3;
CREATE TEMPORARY TABLE tmp_ev3 (
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
--  3-1. 확산 - dup001 이 100 명에게서 물건을 사들인다
--
--   ★ 여기서 만들어지는 로그는 전부 정상이다.
--     구매도 판매도 대금 수령도 서버가 제대로 기록한 것이다.
--     문제는 "그 돈이 어디서 왔는가" 뿐이고, 받은 사람 입장에서는
--     평범한 판매라 로그상 흠잡을 데가 없다.
--     이것이 선별 환수를 불가능하게 만드는 이유다.
-- ------------------------------------------------------------

-- 구매자(dup001) : 골드가 나간다
INSERT INTO tmp_ev3 (actor, seq, log_type, item_code, quantity, gold, target, detail, created_at)
SELECT 'dup001', 5000 + n, 'AUCTION_BUY', 3001, 1, -@spread_price,
       CONCAT('dup', LPAD(n + 1, 3, '0')),
       CONCAT('listing=', 9000 + n),
       TIMESTAMPADD(SECOND, n * 100, TIMESTAMPADD(MINUTE, 10, @incident_at))
  FROM tmp_seq WHERE n <= @spread_count;

-- 판매자 : 매물이 팔렸다. 대금은 아직 pending 이라 수량/골드/잔액이 전부 0.
--          목적은 수치가 아니라 target(거래 상대)이다.
INSERT INTO tmp_ev3 (actor, seq, log_type, item_code, quantity, gold, target, detail, created_at)
SELECT CONCAT('dup', LPAD(n + 1, 3, '0')), 6000 + n, 'AUCTION_SOLD', 3001, 0, 0,
       'dup001', CONCAT('listing=', 9000 + n),
       TIMESTAMPADD(SECOND, n * 100 + 1, TIMESTAMPADD(MINUTE, 10, @incident_at))
  FROM tmp_seq WHERE n <= @spread_count;

-- 판매자 : 대금 수령. 여기서 오염된 골드가 실제로 남의 지갑에 들어간다.
INSERT INTO tmp_ev3 (actor, seq, log_type, item_code, quantity, gold, target, detail, created_at)
SELECT CONCAT('dup', LPAD(n + 1, 3, '0')), 7000 + n, 'AUCTION_COLLECT', 0, 0, @spread_price,
       NULL, CONCAT('listing=', 9000 + n),
       TIMESTAMPADD(SECOND, n * 100 + 600, TIMESTAMPADD(MINUTE, 10, @incident_at))
  FROM tmp_seq WHERE n <= @spread_count;

-- ------------------------------------------------------------
--  3-2. 사건 이후의 평범한 플레이 (사건 시각 ~ +14 시간)
--
--   ★ 이 구간이 있어야 시나리오가 성립한다.
--     버그는 새벽에 터졌지만 발견은 한참 뒤다. 그 사이 아무 잘못 없는
--     사람들이 계속 게임을 했다. 백섭은 이 구간을 통째로 날린다.
--     그래서 백섭에는 반드시 보상이 따라붙고, 얼마를 줄지는
--     살아남은 로그로 계산한다. 그 계산 대상이 바로 이 구간이다.
-- ------------------------------------------------------------
INSERT INTO tmp_ev3 (actor, seq, log_type, item_code, quantity, gold, target, detail, created_at)
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
--  ★ 여기가 사건이 만들어지는 지점이다.
--
--    잔액은 "1단계 마지막 잔액 + 2단계 누적합" 이어야 한다.
--    그런데 dup001 은 거기에 500,000 이 더 얹혀 있다.
--    그 500,000 을 더해준 로그는 없다. 골드가 로그를 안 거치고
--    생겨났기 때문이다.
--
--    그래서 dup001 의 2단계 첫 로그에서
--        직전 잔액 + 이번 증감  !=  이번 잔액
--    이 되고, 연속성 검사가 정확히 그 지점 하나를 짚는다.
--
--    AUCTION_SOLD 만 잔액을 0 으로 둔다(서버 동작과 동일).
-- ============================================================
INSERT INTO mmorpg_log.game_log
    (log_type, actor, target, item_code, quantity, gold, gold_balance, detail, created_at)
SELECT
    e.log_type, e.actor, e.target, e.item_code, e.quantity, e.gold,
    CASE WHEN e.log_type = 'AUCTION_SOLD' THEN 0
         ELSE b.base_bal
              + SUM(e.gold) OVER (PARTITION BY e.actor ORDER BY e.created_at, e.seq
                                  ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW)
              + IF(e.actor = 'dup001', @dupe_gold, 0)
    END,
    e.detail, e.created_at
FROM tmp_ev3 e
JOIN tmp_base b ON b.actor = e.actor
ORDER BY e.created_at, e.actor, e.seq;


-- ============================================================
--  5. 게임 DB 를 로그와 맞추기
--
--   인벤토리는 1단계에서 넣은 것이 있으므로 지우고 전체 로그로 다시 만든다.
-- ============================================================
UPDATE `character` c
   SET gold = IFNULL((
        SELECT g.gold_balance FROM mmorpg_log.game_log g
         WHERE g.actor = c.account_id AND g.log_type <> 'AUCTION_SOLD'
         ORDER BY g.created_at DESC, g.log_id DESC LIMIT 1), 5000)
 WHERE c.account_id LIKE 'dup%';

DELETE FROM inventory WHERE account_id LIKE 'dup%';

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
SELECT '=== 로그 건수 (1+2 단계 합계) ===' AS check_step;
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
  FROM mmorpg_log.game_log WHERE target = 'dup001' AND log_type = 'AUCTION_SOLD';

SELECT '=== dup001 골드 (1단계 잔액 + 500000 - 300000) ===' AS check_step;
SELECT account_id, gold FROM `character` WHERE account_id = 'dup001';

SELECT '=== 지금 시각 - 목표 T 는 이 시각보다 앞이어야 한다 ===' AS check_step;
SELECT NOW(3) AS now_;

SET SQL_SAFE_UPDATES = @old_safe_updates;
