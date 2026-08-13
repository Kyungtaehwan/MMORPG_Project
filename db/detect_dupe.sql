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
SELECT '[5-1] 보상 대상자별 + 총액 (한 번에)' AS step;

--  ★ 왜 보상하는가 - 한 줄로
--    백섭은 잘못한 사람의 이득만 골라 지우는 게 아니라 시간을 되감는다.
--    그래서 그 시간에 정상적으로 플레이한 사람들의 소득도 같이 사라진다.
--
--  ★ 무엇을 보상하는가 - "교환" 이 아니라 "생성" 만 보상한다
--
--    백섭은 거래의 양쪽을 모두 되돌린다. 그래서 교환은 이미 원상복구된다.
--      아이템을 팔았다  -> 골드가 사라지고 아이템이 인벤에 돌아온다
--      아이템을 샀다    -> 아이템이 사라지고 골드가 돌아온다
--    여기에 보상까지 하면 물건도 갖고 돈도 갖는 이중 지급이 된다.
--
--    반면 드롭은 되돌릴 반대편이 없다. 몬스터를 다시 살려낼 수는 없다.
--    사냥한 시간과 노력이 순수하게 사라지므로, 이것만 보상 대상이다.
--
--    예) dup033 의 사건 이후 골드 증가를 경로별로 가르면
--        DROP_GAIN        +325   <- 새로 생긴 것. 되돌릴 반대편이 없다   => 보상
--        SHOP_SELL         +45   <- 아이템을 줬고, 그 아이템이 돌아왔다  => 보상 안 함
--        SHOP_BUY          -30   <- 골드를 줬고, 그 골드가 돌아왔다      => 보상 안 함
--        AUCTION_COLLECT +3,000  <- dup001 에게 받은 오염 대금           => 보상 안 함
--        ----------------------
--        보상액             325
--
--  ★ 덤 - 오염분을 따로 뺄 필요가 없어진다
--    오염 대금은 AUCTION_COLLECT 로 들어온다. DROP_GAIN 만 세면
--    애초에 집계에 안 들어오므로, 매물번호로 조인해 빼던 장치가 통째로 사라진다.
--    (그 조인은 [5-3] 회수 확인용으로만 남겨둔다)
--
--  ★ WITH ROLLUP
--    계정별 행 뒤에 합계 행 하나를 자동으로 덧붙인다.
--    보상 명세와 총액을 따로 두 번 조회할 필요가 없다.

WITH earned AS (
    SELECT actor,
           COUNT(*)  AS drop_cnt,
           SUM(gold) AS drop_gold
      FROM game_log
     WHERE created_at >= @incident_at
       AND log_type   = 'DROP_GAIN'
       AND item_code  = 9000            -- 골드 드롭만 (아이템 드롭은 [5-2])
       AND actor LIKE 'dup%'            -- 이번 사건의 모집단만
       AND actor <> @patient_zero       -- 최초 감염자는 보상 대상이 아니다
     GROUP BY actor
)
SELECT IFNULL(actor, '=== 합계 ===') AS 계정,
       SUM(drop_cnt)                 AS 드롭건수,
       SUM(drop_gold)                AS 보상액
  FROM earned
 GROUP BY actor WITH ROLLUP
HAVING 보상액 > 0
 ORDER BY (계정 = '=== 합계 ==='), 보상액 DESC;


SELECT '[5-2] 백섭으로 잃는 아이템 - 드롭으로 얻은 것만' AS step;

--  골드와 같은 원칙이다.
--    드롭으로 주운 아이템   -> 사라진다. 되돌릴 반대편이 없다  => 보상
--    상점/경매에서 산 아이템 -> 골드가 돌아온다                 => 보상 안 함
--
--  처음에는 "구간에 늘어난 아이템" 을 전부 셌더니 161 개가 나왔는데
--  전부 SHOP_BUY(구매)였다. 돈 주고 산 물건은 백섭이 그 돈을 돌려줬으므로
--  손해가 아니다. 세면 같은 손실을 두 번 계산하는 셈이다.

SELECT actor         AS 계정,
       item_code     AS 아이템,
       SUM(quantity) AS 잃는수량
  FROM game_log
 WHERE created_at >= @incident_at
   AND log_type   = 'DROP_GAIN'
   AND item_code NOT IN (0, 9000)     -- 골드 말고 진짜 아이템만
   AND actor LIKE 'dup%'
   AND actor <> @patient_zero
 GROUP BY actor, item_code
HAVING SUM(quantity) > 0
 ORDER BY 잃는수량 DESC
 LIMIT 20;


SELECT '[5-3] 오염 골드가 전부 회수됐나 (보상과 별개)' AS step;

--  ★ [5-1] 과 헷갈리기 쉬운데 다른 질문이다.
--      [5-1] = 누구에게 얼마를 "돌려줄" 것인가
--      [5-3] = 시장에 풀린 오염 골드가 전부 "걷혔나"
--
--    회수는 이미 백섭이 했다. 지갑에서 사라졌는지를 여기서 확인만 한다.
--    [5-1] 은 보상액이 0 인 사람을 목록에서 빼므로 그 사람의 오염분도
--    집계에서 빠진다. 회수 총액은 반드시 이 쿼리로 따로 봐야 한다.

SELECT COUNT(DISTINCT c.actor) AS 대금받은사람,
       SUM(c.gold)             AS 시장에풀린오염골드
  FROM game_log c
  JOIN game_log s
    ON s.log_type = 'AUCTION_SOLD'
   AND s.target   = @patient_zero
   AND s.actor    = c.actor
   AND s.detail   = c.detail
 WHERE c.log_type   = 'AUCTION_COLLECT'
   AND c.created_at >= @incident_at;


SELECT '[5-4] 검증 - 로그로 계산한 손실이 실제와 맞나' AS step;

--  ★ 백섭 직후에만 돌릴 수 있는 검증이다.
--
--    로그계산 = 구간의 gold 합계        (mmorpg_log 만 보고 계산)
--    DB실측   = 로그 마지막 잔액 - 현재 골드  (실제로 얼마가 사라졌나)
--
--    둘이 같아야 보상액을 믿을 수 있다.
--    단 최초 감염자만 어긋난다 - 그 500,000 은 로그를 안 거치고 생겼으므로
--    로그를 아무리 더해도 나오지 않는다. 어긋나는 게 정상이고,
--    그래서 그 계정만 산정에서 통째로 제외하는 것이다.

SELECT COUNT(*)                        AS 계정수,
       SUM(l.by_log =  d.by_db)        AS 일치,
       SUM(l.by_log <> d.by_db)        AS 불일치_최초감염자만
  FROM (SELECT actor, SUM(gold) AS by_log
          FROM game_log WHERE created_at >= @incident_at AND actor LIKE 'dup%'
         GROUP BY actor) l
  JOIN (SELECT c.account_id AS actor,
               (SELECT g.gold_balance FROM game_log g
                 WHERE g.actor = c.account_id AND g.log_type <> 'AUCTION_SOLD'
                 ORDER BY g.log_id DESC LIMIT 1) - c.gold AS by_db
          FROM mmorpg.`character` c WHERE c.account_id LIKE 'dup%') d
    ON d.actor = l.actor;


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
