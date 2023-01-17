#include <postgres.h>
#include <fmgr.h>
#include "utils/lsyscache.h"
#include "libpq/pqformat.h"
#include <kll/kll_sketch.h>

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(quantile_transfn);

typedef struct kll_sketch_aggstate {
	KLLSketch *sketch;
	float p;
} kll_sketch_aggstate;

Datum
quantile_transfn(PG_FUNCTION_ARGS)
{
	MemoryContext agg_context;
	kll_sketch_aggstate *state;

	if (!AggCheckCallContext(fcinfo, &agg_context))
		elog(ERROR, "quantile_transfn called in non-aggregate context");

	if (PG_ARGISNULL(0))
	{
		Oid			fntype,
					base_type;

		// third argument (k) has been provided
		int k = PG_ARGISNULL(3) ? 1000 : DatumGetInt32(PG_GETARG_DATUM(3));
		if (k < 100)
			k = 100; // just use some minimal coefficient

		fntype = get_fn_expr_argtype(fcinfo->flinfo, 1);
		base_type = getBaseType(fntype);
		state = MemoryContextAlloc(agg_context, sizeof(kll_sketch_aggstate));
		state->p = DatumGetFloat4(PG_GETARG_DATUM(2));
		state->sketch = kll_sketch_new(k, base_type, agg_context,
								PG_GET_COLLATION());
	}
	else
		state = (kll_sketch_aggstate *) PG_GETARG_POINTER(0);

	if (!PG_ARGISNULL(1))
	{
		Datum		elem = PG_GETARG_DATUM(1);

		kll_sketch_update(state->sketch, elem);
	}

	PG_RETURN_POINTER(state);
}

PG_FUNCTION_INFO_V1(quantile_finalfn);

Datum
quantile_finalfn(PG_FUNCTION_ARGS)
{
	MemoryContext agg_context;
	KLLQuantiles q;
	Datum		result;
	kll_sketch_aggstate *state;

	if (!AggCheckCallContext(fcinfo, &agg_context))
		elog(ERROR, "quantile_finalfn called in non-aggregate context");

	state = PG_ARGISNULL(0) ? NULL : (kll_sketch_aggstate *) PG_GETARG_POINTER(0);
	if (state == NULL)
		PG_RETURN_NULL();		/* returns null iff no input values */

	q = kll_sketch_get_quantiles(state->sketch);
	result = kll_sketch_quantiles_query(q, (double) state->p);

	PG_RETURN_DATUM(result);
}

PG_FUNCTION_INFO_V1(quantile_serialfn);

Datum
quantile_serialfn(PG_FUNCTION_ARGS)
{
	bytea	   *v;
	kll_sketch_aggstate  *state;
	StringInfo	si;

	state = (kll_sketch_aggstate *) PG_GETARG_POINTER(0);
	si = kll_sketch_serialize(state->sketch);
	pq_sendfloat4(si, state->p);

	v = (bytea *) si->data;
	SET_VARSIZE(v, si->len);
	PG_RETURN_POINTER(v);
}

PG_FUNCTION_INFO_V1(quantile_deserialfn);

Datum
quantile_deserialfn(PG_FUNCTION_ARGS)
{
	MemoryContext agg_context;
	kll_sketch_aggstate  *state;
	bytea	   *v = (bytea *) PG_GETARG_POINTER(0);
	char	   *ptr = VARDATA(v);
	int			len = VARSIZE(v) - VARHDRSZ;
	StringInfoData si = {ptr, len, len, 0};

	if (!AggCheckCallContext(fcinfo, &agg_context))
		elog(ERROR, "quantile_deserialfn called in non-aggregate context");

	state = MemoryContextAlloc(agg_context, sizeof(kll_sketch_aggstate));
	state->sketch = kll_sketch_deserialize(&si, agg_context);
	state->p = pq_getmsgfloat4(&si);
	PG_RETURN_POINTER(state);
}

PG_FUNCTION_INFO_V1(quantile_combinefn);

Datum
quantile_combinefn(PG_FUNCTION_ARGS)
{
	MemoryContext agg_context;
	kll_sketch_aggstate  *src,
						 *dst;

	if (!AggCheckCallContext(fcinfo, &agg_context))
		elog(ERROR, "quantile_combinefn called in non-aggregate context");

	/* if no "merged" state yet, try creating it */
	if (PG_ARGISNULL(0))
	{
		/* nope, the second argument is NULL to, so return NULL */
		if (PG_ARGISNULL(1))
			PG_RETURN_NULL();

		/* the second argument is not NULL, so copy it */
		src = (kll_sketch_aggstate *) PG_GETARG_POINTER(1);

		/* copy the sketch into the long-lived memory context */
		dst = MemoryContextAlloc(agg_context, sizeof(kll_sketch_aggstate));
		dst->sketch = kll_sketch_copy(src->sketch, agg_context);
		dst->p = src->p;

		PG_RETURN_POINTER(dst);
	}

	/*
	 * If the second argument is NULL, just return the first one (we know it's
	 * not NULL at this point).
	 */
	if (PG_ARGISNULL(1))
		PG_RETURN_DATUM(PG_GETARG_DATUM(0));

	/* merge */
	src = (kll_sketch_aggstate *) PG_GETARG_POINTER(1);
	dst = (kll_sketch_aggstate *) PG_GETARG_POINTER(0);
	kll_sketch_merge(src->sketch, dst->sketch);

	PG_RETURN_POINTER(dst);
}
