CREATE TABLE intvals(val int, color text);

-- Test empty table
SELECT kll_sketch_quantile(val, 0.5) FROM intvals;

-- Integers with odd number of values
INSERT INTO intvals VALUES
       (1, 'a'),
       (2, 'c'),
       (9, 'b'),
       (7, 'c'),
       (2, 'd'),
       (-3, 'd'),
       (2, 'e');

SELECT * FROM intvals ORDER BY val;
SELECT kll_sketch_quantile(val, 0.5) FROM intvals;

-- Integers with NULLs and even number of values
INSERT INTO intvals VALUES
       (99, 'a'),
       (NULL, 'a'),
       (NULL, 'e'),
       (NULL, 'b'),
       (7, 'c'),
       (0, 'd');

SELECT * FROM intvals ORDER BY val;
SELECT kll_sketch_quantile(val, 0.5) FROM intvals;

-- Text values
CREATE TABLE textvals(val text, color int);

INSERT INTO textvals VALUES
       ('erik', 1),
       ('mat', 3),
       ('rob', 8),
       ('david', 9),
       ('lee', 2);

SELECT * FROM textvals ORDER BY val;
SELECT kll_sketch_quantile(val, 0.5) FROM textvals;
SELECT kll_sketch_quantile(val, 0.5, 100) FROM textvals;

-- Test large table with timestamps
CREATE TABLE timestampvals (val timestamptz);

INSERT INTO timestampvals(val)
SELECT TIMESTAMP 'epoch' + (i * INTERVAL '1 second')
FROM generate_series(0, 100000) as T(i);

SELECT m::date as date, date_part('hour', m) as hour,
    (date_part('minute', m) between 52 and 54) as betw52and54
FROM
    (SELECT kll_sketch_quantile(val, 0.5) as m FROM timestampvals) t;

set parallel_setup_cost = 1;
set parallel_tuple_cost = 0.0001;
set max_parallel_workers = 1;
set force_parallel_mode = on;

-- Test large table with texts
CREATE TABLE textvals2 (val text);
INSERT INTO textvals2 select md5(i::text) from generate_series(1, 100000) i;
EXPLAIN (COSTS OFF) SELECT kll_sketch_quantile(val, 0.5) FROM textvals2;
SELECT (v ^@ '7f' or v ^@ '80') as true FROM
	(SELECT kll_sketch_quantile(val, 0.5) as v FROM textvals2) t;
