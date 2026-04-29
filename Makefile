CC      = cc
# -Wno-format-truncation: snprintf into fixed-size buffers is the project's
# style and the truncation is intentional. Other warnings stay enabled.
CFLAGS  = -Wall -Wextra -Wpedantic -Wno-format-truncation -O2 -Iinclude -MMD -MP

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

PREFIX ?= /usr/local

docs:
	@if [ ! -f vendor/doxygen-awesome-css/doxygen-awesome.css ]; then \
	  git submodule update --init --recursive vendor/doxygen-awesome-css; \
	fi
	doxygen Doxyfile

# Completion install: prefer system paths if writable, otherwise fall
# back to per-user XDG locations and print where the files landed.
install-completions:
	@bash_dir=""; zsh_dir=""; \
	for d in /usr/local/etc/bash_completion.d /etc/bash_completion.d; do \
	  if [ -w "$$d" ]; then bash_dir="$$d"; break; fi; \
	done; \
	for d in /usr/local/share/zsh/site-functions /usr/share/zsh/site-functions; do \
	  if [ -w "$$d" ]; then zsh_dir="$$d"; break; fi; \
	done; \
	if [ -z "$$bash_dir" ]; then \
	  bash_dir="$${XDG_DATA_HOME:-$$HOME/.local/share}/bash-completion/completions"; \
	  mkdir -p "$$bash_dir"; \
	fi; \
	if [ -z "$$zsh_dir" ]; then \
	  zsh_dir="$${XDG_DATA_HOME:-$$HOME/.local/share}/zsh/site-functions"; \
	  mkdir -p "$$zsh_dir"; \
	fi; \
	cp completions/jump.bash "$$bash_dir/jump"; \
	cp completions/_jump "$$zsh_dir/_jump"; \
	echo "bash: $$bash_dir/jump"; \
	echo "zsh:  $$zsh_dir/_jump"; \
	if [ "$$zsh_dir" = "$${XDG_DATA_HOME:-$$HOME/.local/share}/zsh/site-functions" ]; then \
	  echo "  add to ~/.zshrc:  fpath+=(\"$$zsh_dir\") && autoload -U compinit && compinit"; \
	fi

clean:
	rm -rf build $(BIN)

-include $(DEPS)

.PHONY: clean install-completions docs
