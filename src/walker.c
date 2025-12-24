#include "walker.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <sys/dmu.h>
#include <sys/dmu_traverse.h>
#include <sys/spa.h>
#include <sys/zfs_context.h>
#include <sys/zio.h>

static const char *const comphist_tag = "zfs-comphist";

struct comphist_find_ctx {
	const struct comphist_options *opts;
	struct comphist_stats *stats;
	int error;
};

struct comphist_iter_ctx {
	const struct comphist_options *opts;
	comphist_dataset_cb_t cb;
	void *arg;
	int error;
};

static int
comphist_blkptr_cb(spa_t *spa, zilog_t *zilog, const blkptr_t *bp,
    const zbookmark_phys_t *zb, const struct dnode_phys *dnp, void *arg)
{
	struct comphist_stats *stats = arg;

	(void)spa;
	(void)zilog;
	(void)dnp;

	if (zb->zb_level == ZB_DNODE_LEVEL)
		return (0);

	if (BP_IS_HOLE(bp)) {
		comphist_stats_note_hole(stats);
		return (0);
	}

	if (BP_IS_REDACTED(bp)) {
		comphist_stats_note_redacted(stats);
		return (0);
	}

	comphist_stats_add_block(stats, BP_GET_COMPRESS(bp),
	    BP_GET_LSIZE(bp), BP_GET_PSIZE(bp), BP_GET_ASIZE(bp),
	    BP_IS_EMBEDDED(bp));

	return (0);
}

static int
comphist_traverse_dataset(struct dsl_dataset *ds,
    const struct comphist_options *opts, struct comphist_stats *stats)
{
	int flags = TRAVERSE_PRE | TRAVERSE_PREFETCH_METADATA |
	    TRAVERSE_NO_DECRYPT;
	zbookmark_phys_t resume = {0};
	zbookmark_phys_t *resume_ptr = NULL;

	if (opts->best_effort) {
		flags |= TRAVERSE_HARD;
		resume_ptr = &resume;
	}

	for (;;) {
		int err = traverse_dataset_resume(ds, 0, resume_ptr, flags,
		    comphist_blkptr_cb, stats);
		if (err == 0)
			return (0);
		if (!opts->best_effort)
			return (err);

		comphist_stats_note_traversal_error(stats);
		if (err == EIO || err == ECKSUM || err == ENXIO) {
			if (resume.zb_blkid == UINT64_MAX)
				return (err);
			resume.zb_blkid++;
			continue;
		}
		return (err);
	}
}

static int
comphist_walk_dataset(const char *dsname, const struct comphist_options *opts,
    struct comphist_stats *stats)
{
	objset_t *os = NULL;
	int err;

	err = dmu_objset_hold(dsname, comphist_tag, &os);
	if (err != 0)
		return (err);

	err = comphist_traverse_dataset(dmu_objset_ds(os), opts, stats);

	dmu_objset_rele(os, comphist_tag);
	return (err);
}

static int
comphist_find_cb(const char *dsname, void *arg)
{
	struct comphist_find_ctx *ctx = arg;
	int err = comphist_walk_dataset(dsname, ctx->opts, ctx->stats);

	if (err != 0) {
		ctx->error = err;
		return (err);
	}

	return (0);
}

static int
comphist_iter_cb(const char *dsname, void *arg)
{
	struct comphist_iter_ctx *ctx = arg;
	struct comphist_stats stats;
	int err;

	comphist_stats_init(&stats);
	err = comphist_walk_dataset(dsname, ctx->opts, &stats);
	if (err != 0) {
		ctx->error = err;
		return (err);
	}

	err = ctx->cb(dsname, &stats, ctx->arg);
	if (err != 0) {
		ctx->error = err;
		return (err);
	}

	return (0);
}

static bool
comphist_target_is_pool(const char *target)
{
	return (strpbrk(target, "/@#") == NULL);
}

int
comphist_walk(const char *target, const struct comphist_options *opts,
    struct comphist_stats *stats)
{
	struct comphist_find_ctx ctx = {
		.opts = opts,
		.stats = stats,
		.error = 0,
	};
	bool kernel_ready = false;
	int err = 0;

	kernel_init(SPA_MODE_READ);
	kernel_ready = true;

	if (comphist_target_is_pool(target)) {
		err = dmu_objset_find(target, comphist_find_cb, &ctx,
		    DS_FIND_CHILDREN);
		if (err == 0)
			err = ctx.error;
	} else if (opts->recursive) {
		if (strpbrk(target, "@#") != NULL) {
			err = EINVAL;
		} else {
			err = dmu_objset_find(target, comphist_find_cb, &ctx,
			    DS_FIND_CHILDREN);
			if (err == 0)
				err = ctx.error;
		}
	} else {
		err = comphist_walk_dataset(target, opts, stats);
	}

	if (kernel_ready)
		kernel_fini();

	if (err != 0) {
		errno = err;
		return (-1);
	}

	return (0);
}

int
comphist_walk_datasets(const char *target, const struct comphist_options *opts,
    comphist_dataset_cb_t cb, void *arg)
{
	struct comphist_iter_ctx ctx = {
		.opts = opts,
		.cb = cb,
		.arg = arg,
		.error = 0,
	};
	struct comphist_stats stats;
	bool kernel_ready = false;
	int err = 0;

	if (cb == NULL) {
		errno = EINVAL;
		return (-1);
	}

	kernel_init(SPA_MODE_READ);
	kernel_ready = true;

	if (comphist_target_is_pool(target)) {
		err = dmu_objset_find(target, comphist_iter_cb, &ctx,
		    DS_FIND_CHILDREN);
		if (err == 0)
			err = ctx.error;
	} else if (opts->recursive) {
		if (strpbrk(target, "@#") != NULL) {
			err = EINVAL;
		} else {
			err = dmu_objset_find(target, comphist_iter_cb, &ctx,
			    DS_FIND_CHILDREN);
			if (err == 0)
				err = ctx.error;
		}
	} else {
		comphist_stats_init(&stats);
		err = comphist_walk_dataset(target, opts, &stats);
		if (err == 0)
			err = cb(target, &stats, arg);
	}

	if (kernel_ready)
		kernel_fini();

	if (err != 0) {
		errno = err;
		return (-1);
	}

	return (0);
}
