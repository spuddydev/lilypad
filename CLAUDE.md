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

## Architecture

Three translation units plus shared types:

- `menu.c` — `main`, argv dispatch (`--add` vs. interactive), and the `execlp("ssh", ...)` handoff. After `run_menu` returns a chosen index, this process is replaced by `ssh` (or `ssh -J <jump> <host>` when a jump is set).
- `hosts.c` / `hosts.h` — hosts-file I/O. `get_hosts_path` resolves `<binary_dir>/hosts`. `load_hosts` parses whitespace-delimited `nickname user@address [jump]` lines with `sscanf("%511s %511s %511[^\n]")`, skipping lines with <2 fields. `add_host` validates (`@` required, no whitespace in nickname/jump), scans the file to reject duplicate nicknames, then appends.
- `ui.c` / `ui.h` — ncurses menu. `run_menu` owns the full curses lifecycle (`initscr`/`endwin`) and returns either a selected index or `-1`. Supports ↑↓/jk, Enter, `q`, and digit shortcuts `1`–`9`. `display_width` counts UTF-8 code points by skipping continuation bytes (`0x80` prefix) so centering works with multibyte glyphs like the `↑↓` hint.
- `common.h` — `Host` struct and the size constants (`MAX_PATH=4096`, `MAX_LINE=512`, `MAX_HOSTS=256`) every module depends on.

All buffers are fixed-size stack arrays sized from `common.h`; there is no dynamic allocation anywhere in the codebase. Keep it that way unless introducing a feature that genuinely needs it.

The Makefile declares per-object header dependencies explicitly — update those lines when a `.c` file starts depending on a new header.
