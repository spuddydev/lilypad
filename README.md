# ssh-menu

Tiny ncurses menu for quickly SSH-ing into saved hosts. Runs on macOS and Linux — state lives in `~/.config/ssh-menu/` (or `$XDG_CONFIG_HOME/ssh-menu/`).

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

Stored at `~/.config/ssh-menu/hosts` (or `$XDG_CONFIG_HOME/ssh-menu/hosts`). One record per line:

```
nickname user@address [jump_hosts] [#markers]
```

Whitespace-delimited. Lines with fewer than two fields are skipped. The `#markers` column is written by the probe and shouldn't usually be hand-edited.

## Layout

- `src/` — `menu.c` (entry + dispatch), `hosts.c` (storage + remote probe), `ui.c` (ncurses menu)
- `include/` — `common.h` (types + limits), `hosts.h`, `ui.h`
- `build/` — objects and auto-generated deps (gitignored)
