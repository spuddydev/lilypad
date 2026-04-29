# lilypad

Tiny ncurses menu and CLI for quickly ssh-ing into saved hosts. Runs on macOS and Linux. State lives in `~/.config/lilypad/` (or `$XDG_CONFIG_HOME/lilypad/`).

The full release-quality README lands in a later PR. This is the working summary.

## Build

```
make
```

Requires a C compiler and ncurses. macOS ships this out of the box; on Debian/Ubuntu, `sudo apt install libncurses-dev`.

## Usage

### Open the menu

```
./jump
```

- `↑`/`↓` or `j`/`k` to move; digits `1`–`9` jump to that row
- `Enter` opens (uses the tmux submenu when the host has tmux)
- `s` opens a plain ssh shell
- `t` opens a default tmux session named `main`
- `A` adds a host inline (three prompts: nickname, user@addr, optional jump)
- `D` deletes the highlighted host; `J`/`K` reorder
- `/` to filter; `r` reprobes the highlighted host; `R` reprobes all visible
- `q` to quit

Red markers next to a host flag what is **missing**: `t` = no tmux, `p` = no tmuxp, `?` = probe failed.

### Direct CLI

```
jump <nick>                  # open the host page for <nick>
jump <nick> <session> [--plain]  # attach or create the named tmux session
jump -s <nick>               # plain ssh, no tmux
jump add user@addr nick [jump_hosts]
jump config [get|set|unset|undecline] ...
```

Tab completion ships in `completions/`. Install via `make install-completions`.

### Add a tmuxp template

```
./jump --template-add path/to/file.yaml
```

Copies the YAML into `~/.config/lilypad/tmuxp/`. The template picker lists anything in that directory.

## Files

- `~/.config/lilypad/hosts` — list of `nickname user@addr [jump_hosts]`
- `~/.config/lilypad/state` — markers, last-probe timestamp, cached sessions, declined integrations (program-managed)
- `~/.config/lilypad/config` — user preferences (`integration.iterm`, `integration.tmuxp`, `probe.timeout`, `probe.cache_seconds`, `probe.max_parallel`)

`setup.sh` writes an initial config based on detected local prerequisites.

## Layout

- `src/` — `menu.c` (entry), `cli.c` (subcommand dispatch), `exec.c` (ssh handoff), `hosts.c` (host storage), `state.c` (runtime state), `config.c`, `integrations.c`, `probe_pool.c` (live probes), `ui.c` (ncurses)
- `include/` — public headers
- `completions/` — bash and zsh completion scripts
- `build/` — objects and auto-generated deps (gitignored)
