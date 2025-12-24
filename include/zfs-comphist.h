#ifndef ZFS_COMPHIST_H
#define ZFS_COMPHIST_H

#include <stdbool.h>

#define COMPHIST_VERSION "0.1.0-dev"

struct comphist_options {
	bool recursive;
	bool allow_live;
	bool best_effort;
	bool json;
	bool per_dataset;
};

#endif
