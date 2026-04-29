CC      = cc
CFLAGS  = -Wall -Wextra -O2 -Iinclude -MMD -MP

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  LDLIBS = -lncurses
else
  LDLIBS = -lcurses
endif

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:src/%.c=build/%.o)
DEPS = $(OBJS:.o=.d)
BIN  = jump

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build $(BIN)

-include $(DEPS)

.PHONY: clean
