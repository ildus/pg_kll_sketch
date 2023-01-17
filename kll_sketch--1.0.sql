CREATE OR REPLACE FUNCTION _kll_sketch_transfn(state internal, val anyelement, p real)
RETURNS internal
AS 'MODULE_PATHNAME', 'quantile_transfn'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION _kll_sketch_transfn(state internal, val anyelement, p real, k integer)
RETURNS internal
AS 'MODULE_PATHNAME', 'quantile_transfn'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION _kll_sketch_finalfn(state internal, val anyelement, p real)
RETURNS anyelement
AS 'MODULE_PATHNAME', 'quantile_finalfn'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION _kll_sketch_finalfn(state internal, val anyelement, p real, k integer)
RETURNS anyelement
AS 'MODULE_PATHNAME', 'quantile_finalfn'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION _kll_sketch_combinefn(a internal, b internal)
RETURNS internal
AS 'MODULE_PATHNAME', 'quantile_combinefn'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION _kll_sketch_serialfn(a internal)
RETURNS bytea
AS 'MODULE_PATHNAME', 'quantile_serialfn'
LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION _kll_sketch_deserialfn(a bytea, b internal)
RETURNS internal
AS 'MODULE_PATHNAME', 'quantile_deserialfn'
LANGUAGE C IMMUTABLE STRICT;

DROP AGGREGATE IF EXISTS kll_sketch_quantile (ANYELEMENT, REAL);
DROP AGGREGATE IF EXISTS kll_sketch_quantile (ANYELEMENT, REAL, INT);
CREATE AGGREGATE kll_sketch_quantile(ANYELEMENT, REAL)
(
    sfunc = _kll_sketch_transfn,
    stype = internal,
    serialfunc = _kll_sketch_serialfn,
    deserialfunc = _kll_sketch_deserialfn,
    combinefunc = _kll_sketch_combinefn,
    finalfunc = _kll_sketch_finalfn,
    finalfunc_extra,
    parallel = safe
);

CREATE AGGREGATE kll_sketch_quantile(ANYELEMENT, REAL, INT)
(
    sfunc = _kll_sketch_transfn,
    stype = internal,
    serialfunc = _kll_sketch_serialfn,
    deserialfunc = _kll_sketch_deserialfn,
    combinefunc = _kll_sketch_combinefn,
    finalfunc = _kll_sketch_finalfn,
    finalfunc_extra,
    parallel = safe
);
