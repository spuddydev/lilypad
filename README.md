# ssh-menu

Tiny ncurses menu for quickly SSH-ing into saved hosts. Runs on macOS and Linux — the `hosts` file is resolved next to the binary.

## Build

```
make
```

Requires a C compiler and ncurses. macOS ships this out of the box; on Debian/Ubuntu, `sudo apt install libncurses-dev`.

## Usage

### Add a host

```
./menu --add user@address nickname [jump_hosts]
```

- `nickname` must not contain whitespace and must be unique.
- `jump_hosts` is optional and passed to `ssh -J`. Use commas for multiple hops (e.g. `u1@a,u2@b`).

### Open the menu

```
./menu
```

- `↑`/`↓` or `j`/`k` to move
- `Enter`, or digits `1`–`9`, to pick
- `s` (or `/`) to filter by nickname/address — Esc clears, Enter picks
- `r` to re-probe the highlighted host (refreshes markers)
- `q` to quit

Red markers next to a host flag what is **missing**: `t` = no tmux, `p` = no tmuxp, `?` = probe failed. No markers means the host is fully set up. Probing happens automatically on `--add` and on demand with `r`.

The selected host is connected via `ssh`, with `ssh -J <jump_hosts>` when one is set.

## Hosts file

Stored as `hosts` next to the binary, one record per line:

```
nickname user@address [jump_hosts]
```

Whitespace-delimited. Lines with fewer than two fields are skipped.

## Layout

- `menu.c` — entry point and command dispatch
- `hosts.c` / `hosts.h` — host-file storage (load, add, path resolution)
- `ui.c` / `ui.h` — ncurses menu
- `common.h` — shared types and limits
