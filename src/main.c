#include "stats.h"
#include "walker.h"

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double
block_percent(uint64_t blocks, uint64_t total)
{
	if (total == 0)
		return 0.0;

	return (double)blocks * 100.0 / (double)total;
}

static void
print_json(const struct comphist_stats *stats, const char *target,
    bool snapshot_mode, const struct comphist_options *opts)
{
	fprintf(stdout, "{\n");
	fprintf(stdout, "  \"target\": \"%s\",\n", target);
	fprintf(stdout, "  \"mode\": \"%s\",\n",
	    snapshot_mode ? "snapshot" : "live");
	fprintf(stdout, "  \"best_effort\": %s,\n",
	    opts->best_effort ? "true" : "false");
	fprintf(stdout, "  \"entries\": [\n");

	bool first = true;
	for (int i = 0; i < ZIO_COMPRESS_FUNCTIONS; i++) {
		const struct comphist_entry *entry = &stats->entries[i];

		if (entry->blocks == 0)
			continue;

		if (!first)
			fprintf(stdout, ",\n");
		first = false;

		double ratio = entry->psize == 0 ? 0.0 :
		    (double)entry->lsize / (double)entry->psize;

		fprintf(stdout,
		    "    {\"name\":\"%s\",\"blocks\":%" PRIu64
		    ",\"block_percent\":%.4f,\"logical_bytes\":%" PRIu64
		    ",\"physical_bytes\":%" PRIu64 ",\"allocated_bytes\":%"
		    PRIu64 ",\"ratio\":%.6f}",
		    comphist_comp_name(i), entry->blocks,
		    block_percent(entry->blocks, stats->total_blocks),
		    entry->lsize, entry->psize, entry->asize, ratio);
	}

	fprintf(stdout, "\n  ],\n");
	fprintf(stdout, "  \"total\": {\"blocks\":%" PRIu64
	    ",\"logical_bytes\":%" PRIu64 ",\"physical_bytes\":%" PRIu64
	    ",\"allocated_bytes\":%" PRIu64 ",\"ratio\":%.6f},\n",
	    stats->total_blocks, stats->total_lsize, stats->total_psize,
	    stats->total_asize,
	    stats->total_psize == 0 ? 0.0 :
	    (double)stats->total_lsize / (double)stats->total_psize);
	fprintf(stdout, "  \"holes\": %" PRIu64 ",\n", stats->total_holes);
	fprintf(stdout, "  \"embedded_blocks\": %" PRIu64 ",\n",
	    stats->total_embedded_blocks);
	fprintf(stdout, "  \"embedded_logical_bytes\": %" PRIu64 ",\n",
	    stats->total_embedded_lsize);
	fprintf(stdout, "  \"redacted_blocks\": %" PRIu64 ",\n",
	    stats->total_redacted);
	fprintf(stdout, "  \"unknown_compression_blocks\": %" PRIu64 ",\n",
	    stats->total_unknown);
	fprintf(stdout, "  \"traversal_errors\": %" PRIu64 "\n",
	    stats->traversal_errors);
	fprintf(stdout, "}\n");
}

static bool
dataset_is_snapshot(const char *dsname)
{
	return (strchr(dsname, '@') != NULL);
}

static void
print_dataset_stats(const char *dsname, const struct comphist_stats *stats,
    bool first)
{
	if (!first)
		fprintf(stdout, "\n");

	fprintf(stdout, "Dataset: %s\n", dsname);
	comphist_stats_print(stats, stdout);
	if (stats->traversal_errors > 0) {
		fprintf(stdout, "traversal errors: %" PRIu64 "\n",
		    stats->traversal_errors);
	}
}

struct json_per_dataset_ctx {
	const struct comphist_options *opts;
	bool first;
};

static void
print_json_dataset_entry(const char *dsname, const struct comphist_stats *stats)
{
	bool snapshot_mode = dataset_is_snapshot(dsname);

	fprintf(stdout, "    {\"name\":\"%s\",\"mode\":\"%s\","
	    "\"entries\":[", dsname, snapshot_mode ? "snapshot" : "live");

	bool first = true;
	for (int i = 0; i < ZIO_COMPRESS_FUNCTIONS; i++) {
		const struct comphist_entry *entry = &stats->entries[i];
		double ratio;

		if (entry->blocks == 0)
			continue;

		if (!first)
			fprintf(stdout, ",");
		first = false;

		ratio = entry->psize == 0 ? 0.0 :
		    (double)entry->lsize / (double)entry->psize;

		fprintf(stdout,
		    "{\"name\":\"%s\",\"blocks\":%" PRIu64
		    ",\"block_percent\":%.4f,\"logical_bytes\":%" PRIu64
		    ",\"physical_bytes\":%" PRIu64 ",\"allocated_bytes\":%"
		    PRIu64 ",\"ratio\":%.6f}",
		    comphist_comp_name(i), entry->blocks,
		    block_percent(entry->blocks, stats->total_blocks),
		    entry->lsize, entry->psize, entry->asize, ratio);
	}

	fprintf(stdout, "],\"total\":{\"blocks\":%" PRIu64
	    ",\"logical_bytes\":%" PRIu64 ",\"physical_bytes\":%" PRIu64
	    ",\"allocated_bytes\":%" PRIu64 ",\"ratio\":%.6f},"
	    "\"holes\":%" PRIu64 ",\"embedded_blocks\":%" PRIu64
	    ",\"embedded_logical_bytes\":%" PRIu64 ",\"redacted_blocks\":%"
	    PRIu64 ",\"unknown_compression_blocks\":%" PRIu64
	    ",\"traversal_errors\":%" PRIu64 "}",
	    stats->total_blocks, stats->total_lsize, stats->total_psize,
	    stats->total_asize,
	    stats->total_psize == 0 ? 0.0 :
	    (double)stats->total_lsize / (double)stats->total_psize,
	    stats->total_holes, stats->total_embedded_blocks,
	    stats->total_embedded_lsize, stats->total_redacted,
	    stats->total_unknown, stats->traversal_errors);
}

static int
print_json_dataset_cb(const char *dsname, const struct comphist_stats *stats,
    void *arg)
{
	struct json_per_dataset_ctx *ctx = arg;

	if (!ctx->first)
		fprintf(stdout, ",\n");
	ctx->first = false;

	print_json_dataset_entry(dsname, stats);
	return 0;
}

struct text_per_dataset_ctx {
	bool first;
};

static int
print_text_dataset_cb(const char *dsname, const struct comphist_stats *stats,
    void *arg)
{
	struct text_per_dataset_ctx *ctx = arg;

	print_dataset_stats(dsname, stats, ctx->first);
	ctx->first = false;
	return 0;
}

static void
usage(FILE *out, const char *prog)
{
	fprintf(out, "Usage: %s [options] <pool|dataset>\n", prog);
	fprintf(out, "\n");
	fprintf(out, "Options:\n");
	fprintf(out, "  -r        recurse datasets (dataset targets only)\n");
	fprintf(out, "  -p, --per-dataset  print a table per dataset\n");
	fprintf(out, "  --allow-live   allow live (non-snapshot) traversal\n");
	fprintf(out, "  --best-effort  continue on I/O/checksum errors\n");
	fprintf(out, "  --json         emit JSON output\n");
	fprintf(out, "  -h        show this help\n");
	fprintf(out, "\n");
	fprintf(out, "Notes:\n");
	fprintf(out, "  Pool targets scan all datasets in the pool.\n");
	fprintf(out, "  Logical_B is BP_GET_LSIZE, Physical_B is BP_GET_PSIZE,\n");
	fprintf(out, "  Allocated_B is BP_GET_ASIZE.\n");
	fprintf(out, "\n");
	fprintf(out, "Version: %s\n", COMPHIST_VERSION);
}

int
main(int argc, char **argv)
{
	struct comphist_options opts = {0};
	struct comphist_stats stats;
	const char *target = NULL;
	bool has_snap = false;
	bool has_bookmark = false;
	bool is_pool = false;
	int c;
	int long_index = 0;
	static const struct option long_opts[] = {
		{"allow-live", no_argument, NULL, 'L'},
		{"best-effort", no_argument, NULL, 'B'},
		{"json", no_argument, NULL, 'J'},
		{"per-dataset", no_argument, NULL, 'p'},
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, argv, "rhp", long_opts,
	    &long_index)) != -1) {
		switch (c) {
		case 'B':
			opts.best_effort = true;
			break;
		case 'L':
			opts.allow_live = true;
			break;
		case 'J':
			opts.json = true;
			break;
		case 'p':
			opts.per_dataset = true;
			break;
		case 'r':
			opts.recursive = true;
			break;
		case 'h':
			usage(stdout, argv[0]);
			return 0;
		default:
			usage(stderr, argv[0]);
			return 2;
		}
	}

	if (optind >= argc) {
		usage(stderr, argv[0]);
		return 2;
	}

	target = argv[optind];
	has_snap = (strchr(target, '@') != NULL);
	has_bookmark = (strchr(target, '#') != NULL);
	is_pool = (strpbrk(target, "/@#") == NULL);

	if (has_bookmark) {
		fprintf(stderr, "comphist: bookmarks are not supported: %s\n",
		    target);
		return 2;
	}

	if (is_pool) {
		if (!opts.allow_live) {
			fprintf(stderr, "comphist: pool traversal requires "
			    "--allow-live\n");
			return 2;
		}
	} else {
		if (opts.recursive && has_snap) {
			fprintf(stderr, "comphist: -r does not apply to "
			    "dataset snapshots\n");
			return 2;
		}
		if (!has_snap && !opts.allow_live) {
			fprintf(stderr, "comphist: dataset traversal requires "
			    "@snapshot or --allow-live\n");
			return 2;
		}
	}

	if (opts.per_dataset) {
		if (opts.json) {
			struct json_per_dataset_ctx ctx = {
				.opts = &opts,
				.first = true,
			};

			fprintf(stdout, "{\n");
			fprintf(stdout, "  \"target\": \"%s\",\n", target);
			fprintf(stdout, "  \"allow_live\": %s,\n",
			    opts.allow_live ? "true" : "false");
			fprintf(stdout, "  \"best_effort\": %s,\n",
			    opts.best_effort ? "true" : "false");
			fprintf(stdout, "  \"datasets\": [\n");

			if (comphist_walk_datasets(target, &opts,
			    print_json_dataset_cb, &ctx) != 0) {
				fprintf(stderr, "comphist: failed to walk '%s': %s\n",
				    target, strerror(errno));
				return 1;
			}

			fprintf(stdout, "\n  ]\n");
			fprintf(stdout, "}\n");
		} else {
			struct text_per_dataset_ctx ctx = {
				.first = true,
			};

			if (comphist_walk_datasets(target, &opts,
			    print_text_dataset_cb, &ctx) != 0) {
				fprintf(stderr, "comphist: failed to walk '%s': %s\n",
				    target, strerror(errno));
				return 1;
			}

			if (has_snap) {
				fprintf(stdout, "\nsnapshot mode\n");
			} else if (opts.allow_live) {
				fprintf(stdout, "\nlive mode enabled\n");
			}
		}
		return 0;
	}

	comphist_stats_init(&stats);

	if (comphist_walk(target, &opts, &stats) != 0) {
		fprintf(stderr, "comphist: failed to walk '%s': %s\n",
		    target, strerror(errno));
		return 1;
	}

	if (opts.json) {
		print_json(&stats, target, has_snap, &opts);
	} else {
		comphist_stats_print(&stats, stdout);
		if (stats.traversal_errors > 0) {
			fprintf(stdout, "traversal errors: %" PRIu64 "\n",
			    stats.traversal_errors);
		}
		if (has_snap) {
			fprintf(stdout, "snapshot mode\n");
		} else if (opts.allow_live) {
			fprintf(stdout, "live mode enabled\n");
		}
	}
	return 0;
}
