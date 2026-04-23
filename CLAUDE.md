# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & run

- `make` — build the `menu` binary (links `-lcurses`).
- `make clean` — remove binary and object files.
- `./menu` — open the ncurses picker.
- `./menu --add user@address nickname [jump_hosts]` — append a host.

No test suite, linter, or formatter is configured.

## Platforms

macOS and Linux. `hosts.c` resolves the `hosts` file path next to the binary using `_NSGetExecutablePath` on macOS and `readlink("/proc/self/exe")` on Linux, guarded by `#ifdef __APPLE__`. The `Makefile` picks `-lcurses` on macOS and `-lncurses` on Linux via `uname -s`.

## Layout

- `src/` — translation units (`menu.c`, `hosts.c`, `ui.c`).
- `include/` — public headers (`common.h`, `hosts.h`, `ui.h`).
- `build/` — object files and `.d` dep files (gitignored; created by make).
- `Makefile` at repo root. Uses `-MMD -MP` for automatic header-dep tracking; no need to list `.o: foo.h` manually.

## Architecture

Three translation units plus shared types:

- `src/menu.c` — `main`, argv dispatch (`--add` vs. interactive), ssh-copy-id orchestration, and the `execlp("ssh", ...)` handoff. After `run_menu` returns a chosen index, this process is replaced by `ssh` (or `ssh -J <jump> <host>` when a jump is set).
- `src/hosts.c` / `include/hosts.h` — hosts-file I/O and the remote probe. `get_hosts_path` resolves `<binary_dir>/hosts`. `load_hosts` parses whitespace-delimited `nickname user@address [jump] [#markers]` lines via a `strtok_r` tokenizer; `add_host` appends with validation + duplicate check; `update_markers` rewrites a single row atomically via tmpfile + rename. `probe_host` forks `ssh -o BatchMode=yes` to check for tmux/tmuxp in one round-trip and returns a marker string (missing letters, or `?` on failure).
- `src/ui.c` / `include/ui.h` — ncurses menu. `run_menu` owns the full curses lifecycle (`initscr`/`endwin`). Supports ↑↓/jk nav, Enter/digits to pick, `s`/`/` to filter, `r` to re-probe the highlighted host. `display_width` counts UTF-8 code points so centering works with multibyte glyphs. Markers are drawn in red (`init_pair(2, COLOR_RED, COLOR_BLACK)`) to the right of each label; empty = healthy.
- `include/common.h` — `Host` struct (including `markers[8]`) and the size constants (`MAX_PATH=4096`, `MAX_LINE=512`, `MAX_HOSTS=256`).

## Marker semantics

The 4th column of the hosts file stores what is **missing** on the remote, not what is present. Possible values:

- empty — probe succeeded, tmux + tmuxp + ssh key all present
- `t` / `p` / `tp` — probe succeeded, listed letters are missing
- `?` — probe could not connect or authenticate

`s` is reserved for "ssh key missing" but the current probe cannot distinguish auth failure from network failure, so it always emits `?` for either. If you add stderr parsing to tell them apart, `s` becomes emittable.

All buffers are fixed-size stack arrays sized from `common.h`; there is no dynamic allocation anywhere in the codebase. Keep it that way unless introducing a feature that genuinely needs it.
