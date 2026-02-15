CC ?= cc
CFLAGS ?= -std=c11 -O2 -g
CPPFLAGS += -Iinclude
CPPFLAGS += -I/usr/include/libzfs
CPPFLAGS += -I/usr/include/libspl
UNAME_S := $(shell uname -s 2>/dev/null || echo Linux)
ifeq ($(UNAME_S),FreeBSD)
ZFS_OS ?= freebsd
CPPFLAGS += -I/usr/local/include/libzfs
CPPFLAGS += -I/usr/local/include/libspl
else
ZFS_OS ?= linux
endif
#
# OpenZFS source tree used for supplemental headers (e.g. sys/zfs_ioctl.h).
# Default to repo submodule; set empty to disable.
#
ZFS_SRC ?= zfs
ifneq ($(strip $(ZFS_SRC)),)
ifneq ($(wildcard $(ZFS_SRC)/include),)
CPPFLAGS += -I$(ZFS_SRC)/include
CPPFLAGS += -I$(ZFS_SRC)/lib/libzpool/include
ifeq ($(ZFS_OS),freebsd)
CPPFLAGS += -I$(ZFS_SRC)/lib/libspl/include
CPPFLAGS += -I$(ZFS_SRC)/lib/libspl/include/os/$(ZFS_OS)
else
CPPFLAGS += -idirafter $(ZFS_SRC)/lib/libspl/include
endif
endif
endif
CPPFLAGS += -D_GNU_SOURCE
WARNFLAGS = -Wall -Wextra -Wshadow -Wformat=2 -Wstrict-prototypes -Wno-cast-qual
LDFLAGS ?=
LDLIBS += -lzfs -lzpool -luutil -lnvpair

TARGET = zfs-comphist
OBJS = \
	src/main.o \
	src/walker.o \
	src/stats.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ -c $<

clean:
	rm -f $(TARGET) $(OBJS)
