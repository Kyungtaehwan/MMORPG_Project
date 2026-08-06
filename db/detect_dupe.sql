-- ============================================================
--  시나리오 2 - 복제 버그 조사 / 백섭 판단 / 보상 산정
--
--  ★ detect.sql 과 무엇이 다른가
--    detect.sql 은 "집계가 이상한" 사건을 다뤘다. 로그는 멀쩡했다.
--    이 파일은 "장부 자체가 안 맞는" 사건을 다룬다.
--    그래서 첫 질문부터 다르다.
--
--      detect.sql      : 누가 남들보다 너무 많이 벌었나  (집계)
--      detect_dupe.sql : 설명되지 않는 골드가 있나        (연속성)
--
--  ★ 이 파일이 답하는 것
--    [1] 로그를 안 거치고 생긴 골드가 있나        (탐지)
--    [2] 언제부터인가                              (백섭 시점)
--    [3] 어디까지 퍼졌나                           (범위)
--    [4] 선별 환수로 끝낼 수 있나                  (판단)
--    [5] 백섭하면 죄 없는 사람이 무엇을 잃나       (보상 산정)
--
--  ★ 읽기 전용. mmo_analyst 로 실행할 것.
-- ============================================================

USE mmorpg_log;

SET NAMES utf8mb4 COLLATE utf8mb4_general_ci;


-- ============================================================
--  [1] 탐지 - 설명되지 않는 골드
--
--  ★ 원리
--    로그에는 증감(gold)과 그 직후 잔액(gold_balance)이 같이 있다.
--    재화가 움직이는 모든 경로에 로그가 있다면 언제나
--        직전 잔액 + 이번 증감 = 이번 잔액
--    이 성립해야 한다. 어긋난다면 둘 중 하나다.
--      - 로그를 안 거치고 골드를 바꾼 코드 경로가 있거나
--      - 복제 버그로 골드가 그냥 생겨났거나
--
--    어긋난 금액(gap)이 곧 "설명되지 않는 금액"이다.
--
--  ★ AUCTION_SOLD 를 빼는 이유
--    판매 성사 로그는 판매자가 접속 중이 아닐 수 있어 잔액을 0 으로 남긴다.
--    잔액 검사의 대상이 아니다. 이 필터를 빼먹으면 정상 거래가
--    전부 위반으로 잡혀서 진짜 사건이 묻힌다.
-- ============================================================
SELECT '[1] 설명되지 않는 골드 - 여기 나오면 로그를 믿을 수 없다' AS step;

SELECT log_id,
       actor,
       log_type,
       created_at,
       prev_balance                          AS 직전잔액,
       gold                                  AS 증감,
       gold_balance                          AS 기록된잔액,
       prev_balance + gold                   AS 계산상잔액,
       gold_balance - (prev_balance + gold)  AS 설명안되는금액
  FROM (
    SELECT log_id, actor, log_type, gold, gold_balance, created_at,
           LAG(gold_balance) OVER (PARTITION BY actor
                                   ORDER BY created_at, log_id) AS prev_balance
      FROM game_log
     WHERE log_type <> 'AUCTION_SOLD'
  ) t
 WHERE prev_balance IS NOT NULL
   AND prev_balance + gold <> gold_balance
 ORDER BY ABS(gold_balance - (prev_balance + gold)) DESC;


-- ------------------------------------------------------------
--  [1] 결과를 보고 아래를 채운다.
--    최초 감염자와, 그 직전 시각(= 되돌릴 시점).
-- ------------------------------------------------------------
SET @patient_zero := 'dup001';
SET @incident_at  := '2026-08-04 02:00:00';


-- ============================================================
--  [2] 백섭 시점 정하기
--
--  ★ 마지막 정상 로그와 첫 이상 로그 사이 어딘가가 사건 시각이다.
--    되돌릴 시점은 "첫 이상 로그 직전" 으로 잡는다.
--    너무 이르게 잡으면 멀쩡한 시간까지 날리고,
--    너무 늦게 잡으면 오염이 남는다.
-- ============================================================
SELECT '[2] 사건 구간' AS step;

SELECT
    (SELECT MAX(created_at) FROM game_log
      WHERE actor = @patient_zero AND created_at < @incident_at) AS 마지막_정상로그,
    (SELECT MIN(created_at) FROM game_log
      WHERE actor = @patient_zero AND created_at >= @incident_at) AS 첫_이상로그,
    (SELECT MAX(created_at) FROM game_log)                        AS 현재_마지막로그;


-- ============================================================
--  [3] 전파 범위
--
--  ★ 이 숫자가 조치를 정한다.
--    오염된 골드는 경매장 대금으로 남의 지갑에 들어갔다.
--    받은 사람의 로그는 완벽하게 정상이다. 평범한 판매였으니까.
--    즉 "받은 사람" 은 로그를 봐도 티가 안 나고,
--    오직 최초 감염자와의 거래 관계(target)로만 추적된다.
-- ============================================================
SELECT '[3-1] 1차 - 최초 감염자와 직접 거래한 사람' AS step;

SELECT COUNT(DISTINCT actor) AS 직접거래자수,
       SUM(CASE WHEN log_type = 'AUCTION_SOLD' THEN 1 ELSE 0 END) AS 판매건수
  FROM game_log
 WHERE target = @patient_zero
   AND created_at >= @incident_at;

SELECT '[3-2] 그 사람들이 실제로 받아간 골드' AS step;

SELECT COUNT(DISTINCT g.actor) AS 수령자수,
       SUM(g.gold)             AS 받아간골드
  FROM game_log g
 WHERE g.log_type = 'AUCTION_COLLECT'
   AND g.created_at >= @incident_at
   AND g.actor IN (SELECT DISTINCT actor FROM game_log
                    WHERE target = @patient_zero AND created_at >= @incident_at);

SELECT '[3-3] 2차 - 1차 감염자에게서 또 받아간 사람 (0 이면 확산은 1단계에서 멈춤)' AS step;

SELECT COUNT(DISTINCT actor) AS 이차감염자수
  FROM game_log
 WHERE created_at >= @incident_at
   AND target IN (SELECT DISTINCT actor FROM game_log
                   WHERE target = @patient_zero AND created_at >= @incident_at)
   AND actor <> @patient_zero;


-- ============================================================
--  [4] 판단 - 선별 환수로 끝낼 수 있나
--
--  ★ 시나리오 1 에서는 이 숫자가 0 이었다. 그래서 계정 하나만
--    되돌리고 끝냈다. 여기서는 100 명이 넘는다.
--
--  ★ 100 명을 전부 sp_rollback_account 로 되돌리면 안 되나?
--    되돌릴 수는 있다. 문제는 그 사람들이 그 사이에 한
--    "정상 거래" 까지 같이 날아간다는 것이다.
--    아래가 그 피해 규모다. 이 숫자를 보고 백섭과 비교한다.
--
--    선별 환수 : 100 명 + 그들의 정상 거래까지 손실
--    전면 백섭 : 전원이 그 시점 이후를 잃되, 로그로 보상 가능
--
--    "선별이 더 정밀하다" 는 직관이 규모가 커지면 뒤집힌다는 것이
--    이 쿼리의 요지다.
-- ============================================================
SELECT '[4] 선별 환수를 택했을 때 함께 날아가는 정상 거래' AS step;

SELECT COUNT(DISTINCT actor)                                  AS 되돌릴계정수,
       COUNT(*)                                               AS 되돌릴로그수,
       SUM(CASE WHEN target IS NULL THEN 1 ELSE 0 END)        AS 사건과무관한거래,
       MIN(created_at)                                        AS 최초,
       MAX(created_at)                                        AS 최종
  FROM game_log
 WHERE created_at >= @incident_at
   AND (actor = @patient_zero
        OR actor IN (SELECT DISTINCT actor FROM game_log
                      WHERE target = @patient_zero AND created_at >= @incident_at));


-- ============================================================
--  [5] 보상 산정
--
--  ★ 여기가 로그 DB 를 따로 둔 이유가 드러나는 곳이다.
--
--    백섭은 mmorpg 만 되돌린다. mmorpg_log 는 그대로 남는다.
--    그래서 "되돌린 구간에 정상 유저가 무엇을 얻었는지"를
--    되돌린 뒤에도 조회할 수 있다.
--
--    로그를 게임 DB 안에 뒀다면 백섭과 함께 로그도 사라져서,
--    보상하려고 만든 기록이 정작 보상이 필요한 순간에만
--    골라서 없어졌을 것이다.
--
--  ★ 오염된 골드는 보상액에서 뺀다 - 계정을 통째로 빼는 게 아니다.
--
--    대금을 받은 100 명도 그 시간에 자기 사냥을 했다. 그건 정상 소득이다.
--    "오염됐으니 너는 보상 없음" 으로 처리하면 죄 없는 사람을 두 번 벌하는 셈이다.
--    그래서 계정 단위로 제외하지 않고,
--        보상액 = 그 구간 총 소득 - 오염되어 받은 금액
--    으로 금액 단위로 뺀다. 최초 감염자만 통째로 제외한다.
--
--    오염 금액은 "dup001 에게 팔린 매물(AUCTION_SOLD)" 과
--    "그 매물의 대금 수령(AUCTION_COLLECT)" 을 매물번호로 이어 붙여 구한다.
-- ============================================================
SELECT '[5-1] 보상 대상자별 - 백섭으로 잃게 되는 골드' AS step;

WITH tainted AS (
    -- 최초 감염자에게 팔아서 받은 대금 = 원래 없던 돈
    SELECT c.actor, SUM(c.gold) AS tainted_gold
      FROM game_log c
      JOIN game_log s
        ON s.log_type = 'AUCTION_SOLD'
       AND s.target   = @patient_zero
       AND s.actor    = c.actor
       AND s.detail   = c.detail
     WHERE c.log_type = 'AUCTION_COLLECT'
       AND c.created_at >= @incident_at
     GROUP BY c.actor
),
earned AS (
    SELECT actor, COUNT(*) AS trades, SUM(gold) AS total_gold
      FROM game_log
     WHERE created_at >= @incident_at
       AND actor <> @patient_zero
     GROUP BY actor
)
SELECT e.actor                                        AS 계정,
       e.trades                                       AS 거래건수,
       e.total_gold                                   AS 구간총소득,
       IFNULL(t.tainted_gold, 0)                      AS 오염분,
       e.total_gold - IFNULL(t.tainted_gold, 0)       AS 보상액
  FROM earned e
  LEFT JOIN tainted t ON t.actor = e.actor
 WHERE e.total_gold - IFNULL(t.tainted_gold, 0) > 0
 ORDER BY 보상액 DESC
 LIMIT 15;

SELECT '[5-2] 보상 대상자별 - 백섭으로 잃게 되는 아이템' AS step;

SELECT actor      AS 계정,
       item_code  AS 아이템,
       SUM(quantity) AS 잃는수량
  FROM game_log
 WHERE created_at >= @incident_at
   AND item_code NOT IN (0, 9000)
   AND actor <> @patient_zero
   AND actor NOT IN (SELECT DISTINCT actor FROM game_log
                      WHERE target = @patient_zero AND created_at >= @incident_at)
 GROUP BY actor, item_code
HAVING SUM(quantity) > 0
 ORDER BY 잃는수량 DESC
 LIMIT 20;

SELECT '[5-3] 보상 총계' AS step;

WITH tainted AS (
    SELECT c.actor, SUM(c.gold) AS tainted_gold
      FROM game_log c
      JOIN game_log s
        ON s.log_type = 'AUCTION_SOLD'
       AND s.target   = @patient_zero
       AND s.actor    = c.actor
       AND s.detail   = c.detail
     WHERE c.log_type = 'AUCTION_COLLECT'
       AND c.created_at >= @incident_at
     GROUP BY c.actor
),
earned AS (
    SELECT actor, SUM(gold) AS total_gold
      FROM game_log
     WHERE created_at >= @incident_at
       AND actor <> @patient_zero
     GROUP BY actor
)
SELECT COUNT(*)                                                AS 보상대상자수,
       SUM(e.total_gold - IFNULL(t.tainted_gold, 0))           AS 보상할골드총액,
       SUM(IFNULL(t.tainted_gold, 0))                          AS 보상에서제외한오염분
  FROM earned e
  LEFT JOIN tainted t ON t.actor = e.actor
 WHERE e.total_gold - IFNULL(t.tainted_gold, 0) > 0;


-- ============================================================
--  [6] 다음 단계 - 실제 백섭
--
--   0. 게임서버를 끈다.
--      5초 주기 자동저장이 메모리 상태로 DB 를 덮어쓰므로,
--      서버가 켜져 있으면 복구가 조용히 원복된다.
--
--   1. 백업본으로 되돌린다.
--      db/backups/ 의 가장 최근 백업 + 그 옆의 *.info.txt 참고.
--
--   2. 백업 시점부터 사건 직전까지 binlog 를 재생한다 (PITR).
--      --stop-datetime 에 위 [2] 의 '첫_이상로그' 직전 시각을 넣는다.
--
--   3. mmorpg 만 복구한다. mmorpg_log 는 손대지 않는다.
--
--   4. 서버를 켜고, 위 [5] 결과로 보상을 지급한다.
-- ============================================================
