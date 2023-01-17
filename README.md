# PostgreSQL extension for calculating quantiles using KLL sketch data structure [1]

The extension provided as is, and could be not ready for the production.

## Usage

Get the median:

```sql
SELECT kll_sketch_quantile(col, 0.5) FROM events; -- default k (1000)
SELECT kll_sketch_quantile(col, 0.5, 2000) FROM events; -- with k as 2000
```

## Installation

To compile and install the extension:

```bash
> make && make install
```

## Testing

Tests can be run with

```bash
> make installcheck
```

[1] http://arxiv.org/pdf/1603.05346v1.pdf
