#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "kll_sketch.h"

/*  coin is a simple struct to let us get random bools and make minimum calls */
/*  to the random number generator. */
typedef struct
{
	uint64_t	st;
	uint64_t	mask;
}			coin;


/*  64-bit xorshift multiply rng from http://vigna.di.unimi.it/ftp/papers/xorshift.pdf */
static uint64_t
xor_shift_mult64(uint64_t x)
{
	x ^= x >> 12;
	//a
		x ^= x << 25;
	//b
		x ^= x >> 27;
	//c
		return x * 2685821657736338717;
}

/*  returns either 0 or 1 */
static int
toss()
{
	int			res = 0;
	static coin c =
	{
		0, 0
	};

	if (c.mask == 0)
	{
		if (c.st == 0)
			c.st = rand();

		c.st = xor_shift_mult64(c.st);
		c.mask = 1;
	}

	if ((c.st & c.mask) > 0)
		res = 1;

	c.mask <<= 1;
	return res;
}

static int
cmp_datum(const void *a, const void *b, void *arg)
{
	Datum		da = *((const Datum *) a);
	Datum		db = *((const Datum *) b);
	KLLSketch  *sketch = (KLLSketch *) arg;
	int32		compare;

	compare = DatumGetInt32(FunctionCall2Coll(&sketch->flinfo,
											  sketch->collation,
											  da, db));
	return compare;
}

void
kll_compactor_extend(KLLSketchCompactor * c, size_t extra)
{
	MemoryContext oldcxt = MemoryContextSwitchTo(((KLLSketch *) c->sketch)->memcxt);

	c->allocated += extra;
	if (c->items == NULL)
		c->items = palloc(sizeof(Datum) * c->allocated);
	else
		c->items = repalloc(c->items, sizeof(Datum) * c->allocated);

	MemoryContextSwitchTo(oldcxt);
}

void
kll_compactor_add(KLLSketchCompactor * c, Datum val)
{
	if (c->allocated <= c->len)
		kll_compactor_extend(c, 10);

	c->items[c->len] = val;
	c->len += 1;
}

void
kll_compactor_compact(KLLSketch * sketch, KLLSketchCompactor * c,
					  KLLSketchCompactor * dst)
{
	size_t		freelen;
	size_t		odd;

	if (c->len > 1)
		qsort_arg(c->items, c->len, sizeof(Datum), cmp_datum, sketch);

	freelen = dst->allocated - dst->len;
	if (freelen < c->len / 2)
		kll_compactor_extend(dst, c->len / 2 - freelen);

	odd = toss();
	for (size_t i = 0; i < c->len; i++)
	{
		bool		skip = odd ^ (i % 2 == 0);

		if (skip)
		{
			if (!sketch->typbyval)
				pfree(DatumGetPointer(c->items[i]));
			continue;
		}

		kll_compactor_add(dst, c->items[i]);
	}

	c->len = 0;
}
