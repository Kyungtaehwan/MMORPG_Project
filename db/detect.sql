-- ============================================================
--  사고 탐지 / 범위 확인 / 부정 이득 산정
--
--  ★ 무엇을 하는 파일인가
--    "골드가 이상하다"는 신고를 받았을 때, 감이 아니라 데이터로
--      1) 누가        (탐지)
--      2) 어디까지 퍼졌나 (범위)
--      3) 얼마나       (산정)
--      4) 어느 시점으로 되돌리나 (조치)
--    를 순서대로 답하는 쿼리 묶음이다.
--
--  ★ 읽기 전용이다. 이 파일은 아무것도 바꾸지 않는다.
--    실제 조치는 마지막에 알려주는 sp_rollback_account 로 한다.
--    분석 계정으로 실행할 것 - 조사하다 실수로 UPDATE 가 나가는 걸 막는다.
--
--      mysql -u mmo_analyst -p
--
--  ★ 순서대로 실행할 것. 앞 결과를 보고 다음 질문이 정해진다.
-- ============================================================

USE mmorpg_log;

-- 이 DB 는 utf8mb4_general_ci 로 만들어져 있는데 접속 기본값은
-- utf8mb4_0900_ai_ci 라, 아래에서 컬럼과 변수를 비교할 때
-- Error 1267 (Illegal mix of collations) 이 난다. 접속 쪽을 맞춰준다.
SET NAMES utf8mb4 COLLATE utf8mb4_general_ci;

-- ------------------------------------------------------------
--  0. 조사 대상
--     [1] 을 먼저 돌려서 나온 이름을 여기에 적고 나머지를 실행한다.
-- ------------------------------------------------------------
SET @suspect := 'user17';

-- 정상적인 드롭 한 번의 골드 상한. 서버 드롭 테이블 기준값이다.
-- 이 값을 넘는 드롭은 정상 경로로는 나올 수 없다.
SET @normal_drop_max := 100;


-- ============================================================
--  [1] 탐지 - 집계 이상치
--
--  ★ 왜 평균이 아니라 중앙값과 비교하나
--    악용자 한 명이 평균을 통째로 끌어올려 버린다. 여기 데이터에서도
--    평균은 2,000 이 넘지만 중앙값은 500 대다. 평균과 비교하면
--    정작 그 악용자가 "평균의 몇 배" 인지가 작아 보인다.
--    중앙값은 극단값에 흔들리지 않아서 기준선으로 쓰기에 맞다.
--
--  ★ 왜 유저별이 아니라 유저x날짜별인가
--    오래 한 사람과 짧고 굵게 뽑아낸 사람을 구분하기 위해서다.
--    "하루에 얼마나 벌었나" 가 비교 가능한 단위다.
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
    -- 중앙값을 구하려고 줄을 세운다. MySQL 에는 MEDIAN 함수가 없다.
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
--
--  하루 단위로는 "그날" 까지만 안다. 시간별로 쪼개면 몇 시부터
--  몇 시까지였는지가 나오고, 그게 되돌릴 시점을 정하는 근거가 된다.
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
--  ★ 이 쿼리가 0 행을 내는 것이 이 사건의 성격이다.
--
--    복제 버그였다면 골드가 로그 없이 생겨나므로
--      직전 잔액 + 이번 증감 != 이번 잔액
--    이 되어 여기 걸린다.
--
--    그런데 이번 건은 드롭이라는 정상 경로를 반복해서 탄 것이라
--    로그도 잔액도 완벽하다. 즉 "장부는 멀쩡한데 양이 이상한" 사건이다.
--    그래서 [1] 의 집계로만 잡힌다.
--
--    같은 질문에 두 검사가 다른 답을 주고, 그 차이가 조치를 가른다.
--      연속성 위반 있음 -> 로그를 못 믿는다 -> 백섭 검토
--      연속성 정상      -> 로그를 믿을 수 있다 -> 선별 환수 가능
--
--  AUCTION_SOLD 를 빼는 이유: 판매 성사 로그는 판매자가 접속 중이
--  아닐 수 있어 잔액을 0 으로 남긴다. 잔액 검사 대상이 아니다.
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
--
--  ★ 이 결과가 조치를 정한다.
--      0 건        -> 혼자 갖고 있다 -> 선별 환수로 끝난다 (백섭 안 함)
--      수십~수백 건 -> 이미 퍼졌다   -> 선별이 불가능 -> 전면 백섭 검토
--
--    조치를 감으로 고르지 않고 이 숫자로 고르는 것이 목적이다.
--
--  target 컬럼이 거래 상대다. 경매 구매면 판매자, 판매 성사면 구매자.
--  드롭이나 상점처럼 상대가 없는 거래는 NULL 이다.
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
--
--  ★ 번 것 전부를 뺏는 게 아니다.
--    그 시간에 정상적으로 플레이했어도 얼마쯤은 벌었을 것이므로,
--    그만큼은 남겨야 한다. 안 그러면 과잉 환수가 된다.
--
--      부정 이득 = 실제로 번 것 - 정상 유저가 같은 시간에 벌었을 양
--
--    "정상 유저가 벌었을 양" 은 [1] 에서 쓴 중앙값을 시간당으로 환산해 쓴다.
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
--
--  첫 비정상 드롭 "직전" 으로 되돌린다. 1 초를 빼는 이유는
--  sp_rollback_account 가 그 시점 "이후" 의 로그를 되감기 때문에,
--  첫 비정상 로그가 되감기 대상에 들어와야 하기 때문이다.
--
--  아래가 뱉어주는 문장을 그대로 복사해서 실행하면 된다.
--  미리보기(0) 로 먼저 확인하고, 맞으면 마지막 인자만 1 로 바꾼다.
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

-- ★ 실행 전에 게임서버를 끌 것.
--   5초 주기 자동저장이 메모리 상태로 DB 를 덮어써서
--   환수가 조용히 원복된다.
