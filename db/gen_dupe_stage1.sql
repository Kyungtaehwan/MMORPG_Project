-- ============================================================
--  시나리오 2 - 1단계 : 사건 이전의 평범한 세계
--
--  ★ 왜 gen_dupe_incident.sql 을 둘로 쪼갰나
--
--    PITR(시점 복구) 실습 때문이다.
--    PITR 은 우리가 로그에 써넣은 created_at 이 아니라
--    "그 SQL 이 실제로 실행된 시각"(binlog)을 보고 자른다.
--    그래서 계정 생성부터 사건까지 한 번에 실행하면
--    binlog 상으로는 전부 같은 순간이라 자를 지점이 없다.
--
--    1단계와 2단계를 시간 간격을 두고 따로 실행하면
--    binlog 에도 그 간격이 그대로 찍히고, 그 사이를 목표 시점으로
--    삼을 수 있다.
--
--  ★ 실습 순서 (자세한 건 2단계 파일 머리말에)
--      1) dup 정리 + 백업(운영 도구 메뉴 6)      <- dup 계정이 0 개인 상태가 백업에 들어간다
--      2) 이 파일 실행                            <- 계정 120 개가 생긴다
--      3) 시각 T 메모 후 30 초 대기
--      4) gen_dupe_stage2.sql 실행                <- 사건 발생
--      5) 메뉴 7 로 T 로 복구
--
--    복구 결과가 "계정 120 개는 있고 사건은 없음" 이어야 성공이다.
--    계정이 있다는 것 자체가 binlog 재생이 됐다는 증거다(백업엔 없었으므로).
--
--  ★ 이 파일이 만드는 것
--      계정 dup001 ~ dup120
--      2026-08-01 ~ 08-03 사이의 평범한 플레이 로그
--      사건은 아직 일어나지 않았다. 연속성 위반 0 건이어야 정상.
--
--  ★ 실행: root 로 전체 실행. 재실행 안전(dup% 만 지우고 다시 만든다).
--    user% (시나리오 1) 와 test1 은 건드리지 않는다.
--
--  ★ 운영 DB 에는 절대 실행하지 말 것.
-- ============================================================

USE mmorpg;

SET NAMES utf8mb4 COLLATE utf8mb4_general_ci;

SET @old_safe_updates := @@SQL_SAFE_UPDATES;
SET SQL_SAFE_UPDATES = 0;


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
--  3. 평범한 플레이 (120 명 전원, 08-01 ~ 08-03)
--
--   사건 전에 정상 활동이 있어야 "언제부터 이상해졌나" 를 말할 수 있다.
--   원본 파일의 3-1 블록과 같은 내용이다(해시가 같으므로 결과도 같다).
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


-- ============================================================
--  4. game_log 에 넣기
--
--   시작 골드 5000 + 시간순 누적합. 지어내면 전원이 연속성 검사에 걸린다.
--   2단계는 여기서 나온 "각 계정의 마지막 잔액" 을 이어받는다.
-- ============================================================
INSERT INTO mmorpg_log.game_log
    (log_type, actor, target, item_code, quantity, gold, gold_balance, detail, created_at)
SELECT
    log_type, actor, target, item_code, quantity, gold,
    5000 + SUM(gold) OVER (PARTITION BY actor ORDER BY created_at, seq
                           ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW),
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

SELECT '=== 연속성 위반 - 아직 사건 전이므로 0 행이어야 정상 ===' AS check_step;
SELECT * FROM (
    SELECT log_id, actor, gold, gold_balance,
           LAG(gold_balance) OVER (PARTITION BY actor ORDER BY created_at, log_id) AS prev
      FROM mmorpg_log.game_log
     WHERE log_type <> 'AUCTION_SOLD' AND actor LIKE 'dup%'
) t
WHERE prev IS NOT NULL AND prev + gold <> gold_balance;

SELECT '=== dup001 의 지금 골드 (2단계에서 여기에 50만이 더해진다) ===' AS check_step;
SELECT account_id, gold FROM `character` WHERE account_id = 'dup001';

SELECT '=== 지금 시각 - 2단계 실행 전에 이 근처를 목표 시점 T 로 메모할 것 ===' AS check_step;
SELECT NOW(3) AS now_;

SET SQL_SAFE_UPDATES = @old_safe_updates;
