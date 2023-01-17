#include "assert.h"
#include "stdlib.h"
#include "math.h"
#include "kll_sketch.h"
#include "utils/builtins.h"

static int
cmp_quantiles(const void *a, const void *b, void *arg)
{
	KLLQuantile *qa = (KLLQuantile *) a;
	KLLQuantile *qb = (KLLQuantile *) b;
	KLLSketch  *sketch = (KLLSketch *) arg;
	int32		compare;

	compare = DatumGetInt32(FunctionCall2Coll(&sketch->flinfo,
											  sketch->collation,
											  qa->v, qb->v));
	return compare;
}

KLLQuantiles
kll_sketch_get_quantiles(KLLSketch * sketch)
{
	double		total_weight = 0;
	double		cur_weight = 0;
	size_t		idx = 0;

	KLLQuantiles q = {sketch->size, NULL};

	q.quantiles = MemoryContextAlloc(sketch->memcxt,
									 sizeof(KLLQuantile) * sketch->size);

	for (size_t i = 0; i < sketch->H; i++)
	{
		double		weight = 1 << i;
		KLLSketchCompactor *c = sketch->compactors[i];

		for (size_t j = 0; j < c->len; j++)
		{
			assert(idx < sketch->size);
			q.quantiles[idx].w = weight;
			q.quantiles[idx].v = c->items[j];

			idx += 1;
		}
		total_weight += weight * c->len;
	}

	qsort_arg(q.quantiles, q.len, sizeof(KLLQuantile), cmp_quantiles, sketch);

	for (size_t i = 0; i < q.len; i++)
	{
		cur_weight += q.quantiles[i].w;
		q.quantiles[i].w = cur_weight / total_weight;
	}

	return q;
}

/*  get a quantile value */
/*  TODO: use binary search */
double
kll_sketch_quantiles_query(KLLQuantiles q, double p)
{
	if (q.len == 0)
		return 0;

	for (size_t j = 0; j < q.len; j++)
	{
		if (q.quantiles[j].w >= p)
			return q.quantiles[j].v;
	}
	return q.quantiles[q.len - 1].v;
}
