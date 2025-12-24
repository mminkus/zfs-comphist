#ifndef COMPHIST_STATS_H
#define COMPHIST_STATS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <sys/zio_compress.h>

struct comphist_entry {
	uint64_t blocks;
	uint64_t lsize;
	uint64_t psize;
	uint64_t asize;
	uint64_t embedded_blocks;
	uint64_t embedded_lsize;
};

struct comphist_stats {
	struct comphist_entry entries[ZIO_COMPRESS_FUNCTIONS];
	uint64_t total_blocks;
	uint64_t total_lsize;
	uint64_t total_psize;
	uint64_t total_asize;
	uint64_t total_embedded_blocks;
	uint64_t total_embedded_lsize;
	uint64_t total_holes;
	uint64_t total_redacted;
	uint64_t total_unknown;
	uint64_t traversal_errors;
};

void comphist_stats_init(struct comphist_stats *stats);
void comphist_stats_add_block(struct comphist_stats *stats,
    enum zio_compress comp, uint64_t lsize, uint64_t psize, uint64_t asize,
    bool embedded);
void comphist_stats_note_hole(struct comphist_stats *stats);
void comphist_stats_note_redacted(struct comphist_stats *stats);
void comphist_stats_note_traversal_error(struct comphist_stats *stats);

const char *comphist_comp_name(enum zio_compress comp);
void comphist_stats_print(const struct comphist_stats *stats, FILE *out);

#endif
