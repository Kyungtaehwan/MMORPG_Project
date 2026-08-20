-- ============================================================
--  사고 탐지 / 범위 확인 / 부정 이득 산정
-- ============================================================

USE mmorpg_log;
SET NAMES utf8mb4 COLLATE utf8mb4_general_ci;

-- ------------------------------------------------------------
--  0. 조사 대상.
-- ------------------------------------------------------------
SET @suspect := 'user17';

-- 정상적인 드롭 한 번의 골드 상한. 
SET @normal_drop_max := 100;


-- ============================================================
--  [1] 탐지 - 집계 이상치
--  중앙값과 비교
-- ============================================================
SELECT '[1] 집계 이상치 - 하루에 번 골드가 중앙값의 10배를 넘는 경우' AS step;

WITH user_day AS (
    -- 유저별 하루 골드 획득량. item_code 9000 = 골드 드롭.
    SELECT actor,
           DATE(created_at) AS day,
           COUNT(*)         AS drop_count,
           SUM(gold)        AS gold_gained
      FROM game_log
     WHERE log_type = 'DROP_GAIN'
       AND item_code = 9000
     GROUP BY actor, DATE(created_at)
),
ranked AS (
    -- 중앙값을 구하려고 줄을 세운다.
    SELECT gold_gained,
           ROW_NUMBER() OVER (ORDER BY gold_gained) AS rn,
           COUNT(*)     OVER ()                    AS total
      FROM user_day
),
baseline AS (
    -- 홀수면 가운데 하나, 짝수면 가운데 둘의 평균.
    SELECT AVG(gold_gained) AS median_gold
      FROM ranked
     WHERE rn IN (FLOOR((total + 1) / 2), CEILING((total + 1) / 2))
)
SELECT u.actor,
       u.day,
       u.drop_count,
       u.gold_gained,
       ROUND(b.median_gold)                       AS median_of_all,
       ROUND(u.gold_gained / b.median_gold, 1)    AS times_median,
       -- 로그는 append-only 라 환수를 해도 사건 기록은 남는다.
       -- 그래서 처리한 건이 계속 다시 잡힌다. 이미 조치했는지를 같이 본다.
       IF(EXISTS(SELECT 1 FROM game_log r
                  WHERE r.actor = u.actor
                    AND r.log_type = 'ADMIN_ROLLBACK'
                    AND r.created_at > u.day),
          'HANDLED', 'OPEN')                      AS status
  FROM user_day u
 CROSS JOIN baseline b
 WHERE u.gold_gained > b.median_gold * 10
 ORDER BY u.gold_gained DESC;


-- ============================================================
--  [2] 같은 것을 시간 단위로 - 언제였나
-- ============================================================
SELECT '[2] 시간대별 - 언제 벌어졌나' AS step;

SELECT DATE_FORMAT(created_at, '%Y-%m-%d %H시') AS hour_slot,
       COUNT(*)  AS drop_count,
       SUM(gold) AS gold_gained,
       MIN(gold) AS min_per_drop,
       MAX(gold) AS max_per_drop
  FROM game_log
 WHERE actor = @suspect
   AND log_type = 'DROP_GAIN'
   AND item_code = 9000
 GROUP BY hour_slot
 ORDER BY gold_gained DESC
 LIMIT 10;


-- ============================================================
--  [3] 대조 - 연속성 검사로는 안 잡힌다
--
--    이 쿼리가 0 을 내야 한다
--
--    복제 버그였다면 골드가 로그 없이 생겨나므로
--      직전 잔액 + 이번 증감 != 이번 잔액이 되어 여기 걸린다.

-- ============================================================
SELECT '[3] 연속성 위반 - 0 행이면 로그를 믿을 수 있다' AS step;

SELECT *
  FROM (
    SELECT log_id, actor, log_type, gold, gold_balance,
           LAG(gold_balance) OVER (PARTITION BY actor
                                   ORDER BY created_at, log_id) AS prev_balance
      FROM game_log
     WHERE log_type <> 'AUCTION_SOLD'
  ) t
 WHERE prev_balance IS NOT NULL
   AND prev_balance + gold <> gold_balance;


-- ============================================================
--  [4] 전파 범위 - 여기가 판단의 갈림길

--      0 건        -> 혼자 갖고 있다 -> 선별 환수로 끝난다 (백섭 안 함)
--      수십~수백 건 -> 이미 퍼졌다   -> 선별이 불가능 -> 전면 백섭 검토
--
-- ============================================================
SELECT '[4-1] 이 계정이 남에게 넘긴 것' AS step;

SELECT log_type,
       target        AS counterparty,
       COUNT(*)      AS trades,
       SUM(quantity) AS items,
       SUM(gold)     AS gold
  FROM game_log
 WHERE actor = @suspect
   AND target IS NOT NULL
 GROUP BY log_type, target
 ORDER BY trades DESC;

SELECT '[4-2] 이 계정에게서 받은 사람' AS step;

SELECT actor         AS counterparty,
       log_type,
       COUNT(*)      AS trades,
       SUM(quantity) AS items,
       SUM(gold)     AS gold
  FROM game_log
 WHERE target = @suspect
 GROUP BY actor, log_type
 ORDER BY trades DESC;

SELECT '[4-3] 경매장에 아직 남아 있는 매물 (다른 유저가 사가면 퍼진다)' AS step;

SELECT listing_id, item_code, count, unit_price, created_at
  FROM mmorpg.auction
 WHERE seller_name = @suspect;


-- ============================================================
--  [5] 부정 이득 산정 - 얼마를 환수할 것인가
--      부정 이득 = 실제로 번 것 - 정상 유저가 같은 시간에 벌었을 양
-- ============================================================
SELECT '[5] 부정 이득 산정' AS step;

WITH user_day AS (
    SELECT actor, DATE(created_at) AS day, SUM(gold) AS gold_gained
      FROM game_log
     WHERE log_type = 'DROP_GAIN' AND item_code = 9000
     GROUP BY actor, DATE(created_at)
),
ranked AS (
    SELECT gold_gained,
           ROW_NUMBER() OVER (ORDER BY gold_gained) AS rn,
           COUNT(*)     OVER ()                    AS total
      FROM user_day
),
baseline AS (
    SELECT AVG(gold_gained) AS median_gold FROM ranked
     WHERE rn IN (FLOOR((total + 1) / 2), CEILING((total + 1) / 2))
),
abuse AS (
    -- 정상 드롭 상한을 넘는 드롭만 = 버그로 얻은 것
    SELECT MIN(created_at) AS first_at,
           MAX(created_at) AS last_at,
           COUNT(*)        AS bad_drops,
           SUM(gold)       AS gold_from_bug,
           TIMESTAMPDIFF(MINUTE, MIN(created_at), MAX(created_at)) AS minutes
      FROM game_log
     WHERE actor = @suspect
       AND log_type = 'DROP_GAIN'
       AND item_code = 9000
       AND gold > @normal_drop_max
)
SELECT a.first_at,
       a.last_at,
       a.minutes                                   AS duration_min,
       a.bad_drops,
       a.gold_from_bug,
       ROUND(b.median_gold)                        AS normal_gold_per_day,
       ROUND(b.median_gold * a.minutes / 1440)     AS normal_expected,
       ROUND(a.gold_from_bug - b.median_gold * a.minutes / 1440) AS unfair_gain
  FROM abuse a CROSS JOIN baseline b;


-- ============================================================
--  [6] 조치 - 되돌릴 시점 정하기
--  첫 비정상 드롭 직전 으로 되돌린다.
-- ============================================================
SELECT '[6] 다음에 실행할 명령' AS step;

SELECT CONCAT(
        'CALL mmorpg.sp_rollback_account(''', @suspect, ''', ''',
        DATE_FORMAT(DATE_SUB(MIN(created_at), INTERVAL 1 SECOND),
                    '%Y-%m-%d %H:%i:%s'),
        ''', 0);'
       ) AS preview_command,
       CONCAT(
        'CALL mmorpg.sp_rollback_account(''', @suspect, ''', ''',
        DATE_FORMAT(DATE_SUB(MIN(created_at), INTERVAL 1 SECOND),
                    '%Y-%m-%d %H:%i:%s'),
        ''', 1);'
       ) AS apply_command
  FROM game_log
 WHERE actor = @suspect
   AND log_type = 'DROP_GAIN'
   AND item_code = 9000
   AND gold > @normal_drop_max;

-- 실행 전에 게임서버를 끌 것.
--   5초 주기 자동저장이 메모리 상태로 DB 를 덮어쓴다.
