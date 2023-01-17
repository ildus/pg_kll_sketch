#ifndef KLL_SKETCH_H
#define KLL_SKETCH_H

#include <postgres.h>
#include "lib/stringinfo.h"
#include "fmgr.h"

typedef struct
{
	void	   *sketch;

	size_t		allocated;
	size_t		len;
	Datum	   *items;
} KLLSketchCompactor;

typedef struct
{
	//k controls the maximum memory used by the stream, which is 3 * k + lg(n).
	size_t		k;
	size_t      max_size;

    //height of sketch(length of compactors)
	size_t		H;
	size_t		size;
	Oid			valtype;
	KLLSketchCompactor **compactors;

	FmgrInfo	flinfo;
	MemoryContext memcxt;
	Oid			collation;		/* collation for compare function */
	int16		typlen;
	bool		typbyval;

	/* debug */
	int			copied;
	bool		deserialized;
} KLLSketch;

typedef struct
{
	double		w;
	Datum		v;
} KLLQuantile;

typedef struct
{
	size_t		len;
	KLLQuantile *quantiles;
} KLLQuantiles;

KLLSketch  *kll_sketch_new(int k, Oid cmp_proc, MemoryContext memcxt,
						   Oid collation);
void		kll_sketch_update(KLLSketch * sketch, Datum val);

StringInfo	kll_sketch_serialize(KLLSketch * sketch);
KLLSketch  *kll_sketch_deserialize(StringInfo si, MemoryContext mcxt);
KLLSketch  *kll_sketch_copy(KLLSketch * src, MemoryContext mcxt);
void		kll_sketch_merge(KLLSketch * src, KLLSketch * dst);

void		kll_compactor_add(KLLSketchCompactor * c, Datum val);
void		kll_compactor_compact(KLLSketch * sketch, KLLSketchCompactor * c,
								  KLLSketchCompactor * dst);
void		kll_compactor_extend(KLLSketchCompactor * c, size_t extra);

KLLQuantiles kll_sketch_get_quantiles(KLLSketch * sketch);
double		kll_sketch_quantiles_query(KLLQuantiles q, double p);

#endif	/* //KLL_SKETCH_H */
