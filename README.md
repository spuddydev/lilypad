# ssh-menu

Tiny ncurses menu for quickly SSH-ing into saved hosts. macOS only (uses `_NSGetExecutablePath` to locate the hosts file next to the binary).

## Build

```
make
```

Requires a C compiler and `curses`, both of which ship with macOS.

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
- `q` to quit

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
