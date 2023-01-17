#include "postgres.h"
#include <assert.h>
#include "utils/typcache.h"
#include "utils/lsyscache.h"
#include "utils/datum.h"
#include "utils/builtins.h"
#include "libpq/pqformat.h"
#include <math.h>
#include "kll_sketch.h"
#include "height.h"

inline static size_t
kll_sketch_capacity(KLLSketch * s, size_t h)
{
	double		k = s->k;
	double		cap = ceil(k * compute_height(s->H - h - 1));

	return (size_t) cap + 1;
}

static void
kll_sketch_grow(KLLSketch * sketch)
{
	KLLSketchCompactor *c;
	MemoryContext oldcxt;

	oldcxt = MemoryContextSwitchTo(sketch->memcxt);

	c = palloc0(sizeof(KLLSketchCompactor));
	c->sketch = sketch;

	sketch->H += 1;
	if (sketch->compactors == NULL)
		sketch->compactors = palloc(sizeof(KLLSketchCompactor *) * sketch->H);
	else
		sketch->compactors = repalloc(sketch->compactors, sizeof(KLLSketchCompactor *) * sketch->H);

	MemoryContextSwitchTo(oldcxt);

	sketch->compactors[sketch->H - 1] = c;
	sketch->max_size = 0;

	for (size_t h = 0; h < sketch->H; h++)
	{
		sketch->max_size += kll_sketch_capacity(sketch, h);
	}
}

static void
kll_sketch_compact(KLLSketch * sketch)
{
	while (sketch->size >= sketch->max_size)
	{
		for (size_t h = 0; h < sketch->H; h++)
		{
			size_t		prev_h;
			size_t		prev_h1;

			if (sketch->compactors[h]->len >= kll_sketch_capacity(sketch, h))
			{
				if (h + 1 >= sketch->H)
				{
					/* add a new KLLSketchCompactor */
					kll_sketch_grow(sketch);
				}

				prev_h = sketch->compactors[h]->len;
				prev_h1 = sketch->compactors[h + 1]->len;

				kll_compactor_compact(sketch, sketch->compactors[h], sketch->compactors[h + 1]);

				sketch->size += sketch->compactors[h]->len - prev_h;
				sketch->size += sketch->compactors[h + 1]->len - prev_h1;

				if (sketch->size < sketch->max_size)
					break;
			}
		}
	}
}

/*  Add a new value to the sketch */
void
kll_sketch_update(KLLSketch * sketch, Datum val)
{
	MemoryContext oldcxt = MemoryContextSwitchTo(sketch->memcxt);
	KLLSketchCompactor *c = sketch->compactors[0];

	val = datumCopy(val, sketch->typbyval, sketch->typlen);
	kll_compactor_add(c, val);
	sketch->size += 1;
	kll_sketch_compact(sketch);
	MemoryContextSwitchTo(oldcxt);
}

/*  Create a new sketch */
KLLSketch *
kll_sketch_new(int k, Oid valtype, MemoryContext memcxt,
			   Oid collation)
{
	TypeCacheEntry *typentry;
	MemoryContext oldcxt = MemoryContextSwitchTo(memcxt);
	KLLSketch  *sketch = palloc0(sizeof(KLLSketch));

	if (sketch == NULL)
		elog(ERROR, "out of memory");

	sketch->memcxt = memcxt;
	sketch->collation = collation;
	sketch->k = k;
	sketch->valtype = valtype;

	typentry = lookup_type_cache(valtype, TYPECACHE_CMP_PROC);
	if (!OidIsValid(typentry->cmp_proc))
		elog(ERROR, "could not find compare function for the argument type");
	fmgr_info(typentry->cmp_proc, &sketch->flinfo);
	get_typlenbyval(valtype, &sketch->typlen, &sketch->typbyval);

	/* start adding compactors */
	kll_sketch_grow(sketch);

	MemoryContextSwitchTo(oldcxt);
	return sketch;
}

/*  Serialize the sketch to bytes */
StringInfo
kll_sketch_serialize(KLLSketch * sketch)
{
	StringInfo	si = makeStringInfo();

	/* TODO: support 32 bit machines */
	static_assert(sizeof(size_t) == 8, "size_t should be 64 bits");
	static_assert(sizeof(Oid) == 4, "Oid should be 32 bits");
	static_assert(sizeof(Datum) == 8, "Datum should be 64 bits");

	pq_sendint32(si, 0);
	//for VARHDRSZ
		pq_sendint64(si, sketch->k);
	pq_sendint32(si, sketch->collation);
	pq_sendint32(si, sketch->valtype);
	pq_sendint64(si, sketch->max_size);
	pq_sendint64(si, sketch->H);
	pq_sendint64(si, sketch->size);
	pq_sendint16(si, sketch->typlen);
	pq_sendbyte(si, sketch->typbyval);

	for (size_t i = 0; i < sketch->H; i++)
	{
		KLLSketchCompactor *c = sketch->compactors[i];

		pq_sendint64(si, c->len);
		if (sketch->typbyval)
			for (size_t j = 0; j < c->len; j++)
				pq_sendint64(si, c->items[j]);
		else
		{
			for (size_t j = 0; j < c->len; j++)
			{
				char	   *addr,
						   *prev;

				size_t		estimated = datumEstimateSpace(c->items[j], false,
														   sketch->typbyval, sketch->typlen);

				enlargeStringInfo(si, estimated + 10);

				/* save the pointers */
				addr = si->data + si->len;
				prev = addr;

				datumSerialize(c->items[j], false, sketch->typbyval, sketch->typlen,
							   &addr);
				si->len += (addr - prev);
			}
		}
	}

	return si;
}

/*  Serialize the sketch to bytes */
KLLSketch *
kll_sketch_deserialize(StringInfo si, MemoryContext mcxt)
{
	MemoryContext oldcxt;
	size_t		k;
	Oid			collation,
				valtype;
	KLLSketch  *sketch;

	k = pq_getmsgint64(si);
	collation = pq_getmsgint(si, sizeof(Oid));
	valtype = pq_getmsgint(si, sizeof(Oid));

	sketch = kll_sketch_new(k, valtype, mcxt, collation);
	sketch->max_size = pq_getmsgint64(si);
	sketch->H = pq_getmsgint64(si);
	sketch->size = pq_getmsgint64(si);
	sketch->typlen = pq_getmsgint(si, 2);
	sketch->typbyval = (bool) pq_getmsgint(si, 1);
	sketch->deserialized = true;

	oldcxt = MemoryContextSwitchTo(mcxt);
	sketch->compactors = palloc(sizeof(KLLSketchCompactor *) * sketch->H);

	for (size_t i = 0; i < sketch->H; i++)
	{
		KLLSketchCompactor *c = palloc0(sizeof(KLLSketchCompactor));

		sketch->compactors[i] = c;

		c->sketch = sketch;
		c->len = pq_getmsgint64(si);
		kll_compactor_extend(c, c->len);

		if (sketch->typbyval)
		{
			for (size_t j = 0; j < c->len; j++)
				c->items[j] = pq_getmsgint64(si);
		}
		else
		{
			for (size_t j = 0; j < c->len; j++)
			{
				char	   *addr = &si->data[si->cursor];
				char	   *prev = addr;
				bool		isnull;

				c->items[j] = datumRestore(&addr, &isnull);
				assert(!isnull);
				si->cursor += (addr - prev);
			}
		}
	}

	MemoryContextSwitchTo(oldcxt);
	return sketch;
}

KLLSketch *
kll_sketch_copy(KLLSketch * src, MemoryContext mcxt)
{
	MemoryContext oldcxt;

	size_t		k = src->k;
	Oid			collation = src->collation;
	Oid			valtype = src->valtype;

	KLLSketch  *sketch = kll_sketch_new(k, valtype, mcxt, collation);

	sketch->max_size = src->max_size;
	sketch->H = src->H;
	sketch->size = src->size;
	sketch->typlen = src->typlen;
	sketch->typbyval = src->typbyval;
	sketch->copied = src->copied + 1;

	oldcxt = MemoryContextSwitchTo(mcxt);
	sketch->compactors = palloc(sizeof(KLLSketchCompactor *) * src->H);

	for (size_t i = 0; i < src->H; i++)
	{
		KLLSketchCompactor *c = palloc0(sizeof(KLLSketchCompactor));

		c->sketch = sketch;
		c->len = src->compactors[i]->len;
		kll_compactor_extend(c, c->len);
		sketch->compactors[i] = c;

		if (sketch->typbyval)
		{
			for (size_t j = 0; j < c->len; j++)
				c->items[j] = src->compactors[i]->items[j];
		}
		else
		{
			for (size_t j = 0; j < c->len; j++)
				c->items[j] = datumCopy(src->compactors[i]->items[j],
										src->typbyval, src->typlen);
		}
	}
	MemoryContextSwitchTo(oldcxt);
	return sketch;
}

void
kll_sketch_merge(KLLSketch * src, KLLSketch * dst)
{
	MemoryContext oldcxt;

	while (dst->H < src->H)
		kll_sketch_grow(dst);

	oldcxt = MemoryContextSwitchTo(dst->memcxt);
	for (size_t i = 0; i < src->H; i++)
	{
		KLLSketchCompactor *c = src->compactors[i];
		KLLSketchCompactor *d = dst->compactors[i];

		for (size_t j = 0; j < c->len; j++)
		{
			Datum		val = datumCopy(c->items[j], src->typbyval, src->typlen);

			kll_compactor_add(d, val);
			dst->size += 1;
		}
	}

	kll_sketch_compact(dst);
	MemoryContextSwitchTo(oldcxt);
}
