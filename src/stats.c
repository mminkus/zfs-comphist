#include "stats.h"

#include <inttypes.h>
#include <string.h>

void
comphist_stats_init(struct comphist_stats *stats)
{
	memset(stats, 0, sizeof(*stats));
}

static enum zio_compress
comphist_normalize_comp(enum zio_compress comp)
{
	if (comp < 0 || comp >= ZIO_COMPRESS_FUNCTIONS) {
		return ZIO_COMPRESS_INHERIT;
	}

	return comp;
}

void
comphist_stats_add_block(struct comphist_stats *stats, enum zio_compress comp,
    uint64_t lsize, uint64_t psize, uint64_t asize, bool embedded)
{
	enum zio_compress idx = comphist_normalize_comp(comp);
	struct comphist_entry *entry = &stats->entries[idx];

	entry->blocks++;
	entry->lsize += lsize;
	entry->psize += psize;
	entry->asize += asize;

	stats->total_blocks++;
	stats->total_lsize += lsize;
	stats->total_psize += psize;
	stats->total_asize += asize;

	if (embedded) {
		entry->embedded_blocks++;
		entry->embedded_lsize += lsize;
		stats->total_embedded_blocks++;
		stats->total_embedded_lsize += lsize;
	}

	if (idx == ZIO_COMPRESS_INHERIT && comp != ZIO_COMPRESS_INHERIT) {
		stats->total_unknown++;
	}
}

void
comphist_stats_note_hole(struct comphist_stats *stats)
{
	stats->total_holes++;
}

void
comphist_stats_note_redacted(struct comphist_stats *stats)
{
	stats->total_redacted++;
}

void
comphist_stats_note_traversal_error(struct comphist_stats *stats)
{
	stats->traversal_errors++;
}

const char *
comphist_comp_name(enum zio_compress comp)
{
	switch (comp) {
	case ZIO_COMPRESS_INHERIT:
		return "inherit";
	case ZIO_COMPRESS_ON:
		return "on";
	case ZIO_COMPRESS_OFF:
		return "off";
	case ZIO_COMPRESS_LZJB:
		return "lzjb";
	case ZIO_COMPRESS_EMPTY:
		return "empty";
	case ZIO_COMPRESS_GZIP_1:
		return "gzip-1";
	case ZIO_COMPRESS_GZIP_2:
		return "gzip-2";
	case ZIO_COMPRESS_GZIP_3:
		return "gzip-3";
	case ZIO_COMPRESS_GZIP_4:
		return "gzip-4";
	case ZIO_COMPRESS_GZIP_5:
		return "gzip-5";
	case ZIO_COMPRESS_GZIP_6:
		return "gzip-6";
	case ZIO_COMPRESS_GZIP_7:
		return "gzip-7";
	case ZIO_COMPRESS_GZIP_8:
		return "gzip-8";
	case ZIO_COMPRESS_GZIP_9:
		return "gzip-9";
	case ZIO_COMPRESS_ZLE:
		return "zle";
	case ZIO_COMPRESS_LZ4:
		return "lz4";
	case ZIO_COMPRESS_ZSTD:
		return "zstd";
	default:
		return "unknown";
	}
}

static void
comphist_print_row(FILE *out, const char *name, uint64_t blocks,
    double block_percent, uint64_t lsize, uint64_t psize, uint64_t asize,
    double ratio)
{
	fprintf(out, "%-12s %12" PRIu64 " %8.2f %14" PRIu64 " %14" PRIu64
	    " %14" PRIu64 " %7.2f\n",
	    name, blocks, block_percent, lsize, psize, asize, ratio);
}

void
comphist_stats_print(const struct comphist_stats *stats, FILE *out)
{
	fprintf(out, "Compression   Blocks     Block_%%   Logical_B     Physical_B"
	    "     Allocated_B  Ratio\n");
	fprintf(out, "------------------------------------------------------------"
	    "------------------------\n");

	for (int i = 0; i < ZIO_COMPRESS_FUNCTIONS; i++) {
		const struct comphist_entry *entry = &stats->entries[i];
		double block_percent = 0.0;
		double ratio = 0.0;

		if (entry->blocks == 0) {
			continue;
		}

		if (stats->total_blocks > 0) {
			block_percent =
			    (double)entry->blocks * 100.0 /
			    (double)stats->total_blocks;
		}

		if (entry->psize > 0) {
			ratio = (double)entry->lsize / (double)entry->psize;
		}

		comphist_print_row(out, comphist_comp_name(i),
		    entry->blocks, block_percent, entry->lsize, entry->psize,
		    entry->asize, ratio);
	}

	fprintf(out, "------------------------------------------------------------"
	    "------------------------\n");
	comphist_print_row(out, "total", stats->total_blocks,
	    stats->total_blocks ? 100.0 : 0.0, stats->total_lsize,
	    stats->total_psize, stats->total_asize,
	    stats->total_psize == 0 ? 0.0 :
	    (double)stats->total_lsize / (double)stats->total_psize);

	if (stats->total_holes > 0) {
		fprintf(out, "holes: %" PRIu64 "\n", stats->total_holes);
	}
	if (stats->total_redacted > 0) {
		fprintf(out, "redacted blocks: %" PRIu64 "\n",
		    stats->total_redacted);
	}
	if (stats->total_embedded_blocks > 0) {
		fprintf(out, "embedded blocks: %" PRIu64 " (logical bytes: %" PRIu64
		    ")\n", stats->total_embedded_blocks,
		    stats->total_embedded_lsize);
	}
	if (stats->total_unknown > 0) {
		fprintf(out, "unknown compression blocks: %" PRIu64 "\n",
		    stats->total_unknown);
	}
}
