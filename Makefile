CC ?= cc
CFLAGS ?= -std=c11 -O2 -g
CPPFLAGS += -Iinclude
CPPFLAGS += -I/usr/include/libzfs
CPPFLAGS += -I/usr/include/libspl
ZFS_SRC ?= /home/martin/opensource/zfs
ifneq ($(wildcard $(ZFS_SRC)/include),)
CPPFLAGS += -I$(ZFS_SRC)/include -I$(ZFS_SRC)/lib/libzpool/include
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
