-- ============================================================
--  탐지 쿼리 검증용 합성 데이터 생성 (테스트 픽스처)
--
--  ★ 이 파일은 무엇인가
--    "골드가 비정상적으로 새어나가는 유저를 찾아내는 쿼리"를 만들려면
--    먼저 정상 유저가 잔뜩 있는 데이터가 있어야 한다. 이상치라는 건
--    정상 분포와 비교해야만 나오는 개념이기 때문이다.
--    실플레이 로그는 8건뿐이라 비교 대상이 없어서, 계정 30개를 만들어 넣는다.
--    ★ 악용자는 별도 계정이 아니다. user17 은 그 30 개 안에 들어 있다.
--      (= 순수 정상 29 명 + 악용자 1 명). user17 도 사고 구간 밖에서는
--      남들과 똑같은 정상 로그를 남기며, 그게 3 번 대조 쿼리의 근거가 된다.
--
--  ★ 왜 봇이 아니라 SQL 인가
--    MMO_StressTest 봇은 로그인/이동/공격만 한다. 줍기/상점/경매 패킷을
--    아예 보내지 않아서 거래 로그가 한 건도 안 쌓인다. 봇으로 하려면
--    봇에 경제 행동을 새로 구현해야 하는데, 지금 목적(탐지 쿼리 검증)에는
--    "정답을 아는 데이터"를 직접 만드는 편이 낫다.
--
--  ★ 정답 (이 데이터가 숨기고 있는 것)
--    user17 이 2026-08-03 밤에 버그 몬스터를 반복 사냥해 골드를 쓸어담았다.
--    중요한 점: user17 의 로그는 완벽하게 정상이다.
--      - 드롭은 정상 경로라 로그가 다 남아 있고
--      - gold_balance 연속성도 하나도 안 깨진다
--    그래서 연속성 검사로는 절대 안 잡힌다. 집계로만 잡힌다.
--    이것이 시나리오 1 과 시나리오 2 의 탐지 기법이 다른 이유다.
--
--  ★ 실행
--    root 로 전체 실행. 재실행 안전(user% 계정과 그 로그만 지우고 다시 만든다).
--    실플레이 로그(test1)와 test1/test2 계정은 건드리지 않는다.
--
--  ★ 운영 DB 에는 절대 실행하지 말 것. 로그를 위조하는 스크립트다.
-- ============================================================

USE mmorpg;

SET @old_safe_updates := @@SQL_SAFE_UPDATES;
SET SQL_SAFE_UPDATES = 0;

-- 재현 가능한 데이터를 만들기 위해 난수를 쓰지 않고 CRC32 해시를 쓴다.
-- 같은 입력이면 언제나 같은 결과가 나오므로, 탐지 쿼리를 고치면서
-- 데이터가 흔들리지 않는다.


-- ============================================================
--  1. 이전 실행분 정리
-- ============================================================
DELETE FROM mmorpg_log.game_log WHERE actor LIKE 'user%';
DELETE FROM account            WHERE account_id LIKE 'user%';   -- character/inventory 는 CASCADE


-- ============================================================
--  2. 계정 30 개 만들기 (user01 ~ user30)
-- ============================================================
--  숫자 1~200 을 담은 표. 계정 번호와 사건 번호를 만드는 데 쓴다.
--  ★ 같은 임시 테이블을 한 쿼리에서 두 번 참조하면 MySQL 이 거부한다
--    (Error 1137 Can't reopen table). 아래에서 계정 x 사건으로 조인해야 하므로
--    내용이 같은 표를 두 개 만든다.
DROP TEMPORARY TABLE IF EXISTS tmp_num;
DROP TEMPORARY TABLE IF EXISTS tmp_seq;
CREATE TEMPORARY TABLE tmp_num (n INT PRIMARY KEY);
CREATE TEMPORARY TABLE tmp_seq (n INT PRIMARY KEY);

INSERT INTO tmp_num (n)
WITH RECURSIVE seq AS (
    SELECT 1 AS n
    UNION ALL
    SELECT n + 1 FROM seq WHERE n < 200
)
SELECT n FROM seq;

INSERT INTO tmp_seq (n) SELECT n FROM tmp_num;

INSERT INTO account (account_id, password)
SELECT CONCAT('user', LPAD(n, 2, '0')), '1234'
  FROM tmp_num WHERE n <= 30;

INSERT INTO `character` (account_id, zone_id, spawn_x, spawn_z, gold, level, exp)
SELECT CONCAT('user', LPAD(n, 2, '0')), 0, 12, 20, 0, 1 + (n % 8), 0
  FROM tmp_num WHERE n <= 30;


-- ============================================================
--  3. 사건 목록 만들기
--
--   계정마다 30 ~ 69 건. 종류는 해시값의 나머지로 정한다.
--     45% 골드 드롭    (item_code 9000, 골드 +)
--     15% 아이템 드롭  (아이템 +)
--     15% 상점 구매    (아이템 +, 골드 -)
--     10% 상점 판매    (아이템 -, 골드 +)
--     10% 아이템 사용  (아이템 -)
--      5% 경매 등록    (아이템 -)
--
--   쓰는 아이템 코드는 겹쳐 쌓이는 것들만 골랐다.
--   장비(3xxx)는 한 칸에 하나씩이라 슬롯 계산이 복잡해지기 때문이다.
-- ============================================================
DROP TEMPORARY TABLE IF EXISTS tmp_ev;
CREATE TEMPORARY TABLE tmp_ev (
    actor      VARCHAR(20)  NOT NULL,
    seq        INT          NOT NULL,
    log_type   VARCHAR(24)  NOT NULL,
    item_code  INT          NOT NULL,
    quantity   INT          NOT NULL,
    gold       INT          NOT NULL,
    detail     VARCHAR(128) NULL,
    created_at DATETIME(3)  NOT NULL,
    INDEX idx_actor_time (actor, created_at, seq)
);

-- ------------------------------------------------------------
--  3-1. 계정 30 개 전원의 정상 활동
--
--   user17 도 여기 포함된다. 악용자에게도 평소의 정상 로그가 있어야
--   "계정을 통째로 날리는 게 아니라 사고 구간만 되감는다" 가 성립한다.
-- ------------------------------------------------------------
INSERT INTO tmp_ev (actor, seq, log_type, item_code, quantity, gold, detail, created_at)
SELECT
    e.actor,
    e.seq,
    CASE
        WHEN e.t < 45 THEN 'DROP_GAIN'
        WHEN e.t < 60 THEN 'DROP_GAIN'
        WHEN e.t < 75 THEN 'SHOP_BUY'
        WHEN e.t < 85 THEN 'SHOP_SELL'
        WHEN e.t < 95 THEN 'ITEM_USE'
        ELSE               'AUCTION_LIST'
    END AS log_type,
    CASE WHEN e.t < 45 THEN 9000 ELSE e.code END AS item_code,
    CASE
        WHEN e.t < 45 THEN 0                 -- 골드 드롭은 아이템 수량이 없다
        WHEN e.t < 60 THEN 1                 -- 아이템 드롭
        WHEN e.t < 75 THEN 1 + (e.m % 3)     -- 상점 구매 1~3 개
        WHEN e.t < 85 THEN -1                -- 상점 판매
        WHEN e.t < 95 THEN -1                -- 아이템 사용
        ELSE               -1                -- 경매 등록
    END AS quantity,
    CASE
        WHEN e.t < 45 THEN 20 + (e.m % 61)          -- 드롭 골드 20~80
        WHEN e.t < 60 THEN 0
        WHEN e.t < 75 THEN -30 * (1 + (e.m % 3))    -- 구매 대금
        WHEN e.t < 85 THEN 15                       -- 판매 대금
        ELSE               0
    END AS gold,
    -- 드롭 로그에는 출처 몬스터를 남긴다(서버 Zone.cpp 가 남기는 detail 과 같은 형식).
    -- 이게 있어야 탐지가 "이 유저가 이상하다" 에서 멈추지 않고
    -- "어느 몬스터가 원인인가" 까지 갈 수 있다.
    CASE WHEN e.t <  60 THEN CONCAT('mon=', ELT(1 + (e.m % 2), 'orc', 'wing'))
         WHEN e.t >= 95 THEN CONCAT('unit=', 100 + (e.m % 200))
         ELSE NULL END AS detail,
    -- 유저마다 시작 시각을 이틀 범위로 흩어 놓는다.
    -- 전원이 같은 시각에 시작하면 하루에 활동이 몰려서
    -- "일자별 평균" 이라는 비교 기준 자체가 얇아진다.
    TIMESTAMPADD(SECOND,
                 (CRC32(e.actor) % 172800) + e.seq * (600 + (e.m % 3400)),
                 '2026-08-01 00:00:00') AS created_at
FROM (
    SELECT
        CONCAT('user', LPAD(a.n, 2, '0')) AS actor,
        s.n                               AS seq,
        -- CRC32 는 UNSIGNED 를 돌려준다. 그대로 두면 아래에서 음수를 곱할 때
        -- 부호 없는 뺄셈으로 계산돼 Error 1690(out of range) 이 난다.
        CAST(CRC32(CONCAT('t', a.n, '-', s.n)) % 100 AS SIGNED) AS t,
        CAST(CRC32(CONCAT('m', a.n, '-', s.n)) % 997 AS SIGNED) AS m,
        ELT(1 + (CRC32(CONCAT('c', a.n, '-', s.n)) % 5),
            1000, 1001, 1002, 1004, 4004)  AS code
    FROM tmp_num a
    JOIN tmp_seq s
      ON s.n <= 30 + (CRC32(CONCAT('cnt', a.n)) % 40)
    WHERE a.n <= 30
) e;

-- ------------------------------------------------------------
--  3-2. 악용자 user17 - 사고 구간
--
--   2026-08-03 20:00 ~ 23:00 사이에 골드 드롭만 180 건.
--   건당 금액도 정상(20~80)보다 훨씬 크다(300~500).
--
--   ★ 원인 몬스터는 wing 이다.
--     정상 유저들도 wing 을 잡지만 그쪽은 20~80 이 나온다.
--     즉 "wing 이라서" 가 아니라 "이 구간의 wing 이" 잘못된 것이고,
--     대조군이 있어야 그 구분이 데이터로 증명된다.
--
--   ★ 로그 자체는 완벽하게 정상이라는 점이 핵심이다.
--     드롭은 원래 골드가 새로 생기는 정상 경로이고, 서버는 지급에
--     성공한 뒤 로그를 남기므로 잔액도 한 번도 안 어긋난다.
--     연속성 검사로는 잡을 수 없고, 집계로만 드러난다.
-- ------------------------------------------------------------
INSERT INTO tmp_ev (actor, seq, log_type, item_code, quantity, gold, detail, created_at)
SELECT
    'user17',
    1000 + s.n,
    'DROP_GAIN',
    9000,
    0,
    300 + (CRC32(CONCAT('abuse', s.n)) % 201),
    'mon=wing',
    TIMESTAMPADD(SECOND, s.n * 60, '2026-08-03 20:00:00')
FROM tmp_num s
WHERE s.n <= 180;


-- ============================================================
--  4. game_log 에 넣기
--
--   ★ gold_balance 는 직접 지어내면 안 된다.
--     시간순 누적합으로 계산해야 연속성이 성립한다.
--     지어냈다면 정상 유저 전원이 연속성 검사에 걸려버린다.
--
--   모든 계정의 시작 골드는 5000 으로 둔다.
-- ============================================================
INSERT INTO mmorpg_log.game_log
    (log_type, actor, target, item_code, quantity, gold, gold_balance, detail, created_at)
SELECT
    log_type, actor, NULL, item_code, quantity, gold,
    5000 + SUM(gold) OVER (PARTITION BY actor ORDER BY created_at, seq
                           ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW),
    detail, created_at
FROM tmp_ev
ORDER BY created_at, actor, seq;


-- ============================================================
--  5. 게임 DB 를 로그와 맞추기
--
--   로그와 실제 상태가 어긋나 있으면 환수 도구가 엉뚱한 값을 낸다.
--   최종 골드 = 마지막 로그의 잔액.
-- ============================================================
UPDATE `character` c
   SET gold = (
        SELECT g.gold_balance
          FROM mmorpg_log.game_log g
         WHERE g.actor = c.account_id
         ORDER BY g.created_at DESC, g.log_id DESC
         LIMIT 1)
 WHERE c.account_id LIKE 'user%';

-- 인벤토리: 시작 재고 + 로그의 수량 증감.
-- 시작 재고를 넉넉히(각 20개) 둬서 판매/사용으로 음수가 되지 않게 했다.
INSERT INTO inventory (account_id, slot, item_code, count)
SELECT
    x.actor,
    ROW_NUMBER() OVER (PARTITION BY x.actor ORDER BY x.item_code) - 1 AS slot,
    x.item_code,
    x.cnt
FROM (
    SELECT c.account_id AS actor,
           codes.item_code,
           20 + IFNULL((SELECT SUM(g.quantity)
                          FROM mmorpg_log.game_log g
                         WHERE g.actor = c.account_id
                           AND g.item_code = codes.item_code), 0) AS cnt
      FROM `character` c
      JOIN (SELECT 1000 AS item_code UNION ALL SELECT 1001 UNION ALL
            SELECT 1002 UNION ALL SELECT 1004 UNION ALL SELECT 4004) codes
     WHERE c.account_id LIKE 'user%'
) x
WHERE x.cnt > 0;


-- ============================================================
--  6. 확인
-- ============================================================
SELECT '=== 만들어진 계정 수 (30 이어야 정상) ===' AS check_step;
SELECT COUNT(*) AS accounts FROM account WHERE account_id LIKE 'user%';

SELECT '=== 로그 건수 (합성분만) ===' AS check_step;
SELECT COUNT(*) AS synthetic_logs FROM mmorpg_log.game_log WHERE actor LIKE 'user%';

SELECT '=== 연속성 검사 - 0 행이어야 정상 ===' AS check_step;
SELECT * FROM (
    SELECT log_id, actor, gold, gold_balance,
           LAG(gold_balance) OVER (PARTITION BY actor ORDER BY created_at, log_id) AS prev
      FROM mmorpg_log.game_log
     WHERE log_type <> 'AUCTION_SOLD'
) t
WHERE prev IS NOT NULL AND prev + gold <> gold_balance;

SELECT '=== 골드가 음수인 로그 - 0 행이어야 정상 ===' AS check_step;
SELECT COUNT(*) AS negative_balance FROM mmorpg_log.game_log WHERE gold_balance < 0;

SELECT '=== DB 골드와 마지막 로그 잔액이 어긋난 계정 - 0 행이어야 정상 ===' AS check_step;
SELECT c.account_id, c.gold AS db_gold, l.gold_balance AS log_gold
  FROM `character` c
  JOIN mmorpg_log.game_log l
    ON l.log_id = (SELECT g.log_id FROM mmorpg_log.game_log g
                    WHERE g.actor = c.account_id
                    ORDER BY g.created_at DESC, g.log_id DESC LIMIT 1)
 WHERE c.account_id LIKE 'user%' AND c.gold <> l.gold_balance;

SET SQL_SAFE_UPDATES = @old_safe_updates;
