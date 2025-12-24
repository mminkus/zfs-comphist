#ifndef COMPHIST_WALKER_H
#define COMPHIST_WALKER_H

#include "stats.h"

#include <libzfs.h>

#include "zfs-comphist.h"

typedef int (*comphist_dataset_cb_t)(const char *dsname,
    const struct comphist_stats *stats, void *arg);

int comphist_walk(const char *target, const struct comphist_options *opts,
    struct comphist_stats *stats);
int comphist_walk_datasets(const char *target, const struct comphist_options *opts,
    comphist_dataset_cb_t cb, void *arg);

#endif
