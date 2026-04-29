# lilypad

An ssh launcher with tmux integration. Opens a small ncurses menu over your saved hosts, probes them in the background, and hands off to ssh (or `tmux new-session -A`, with iTerm `tmux -CC` when you're on iTerm). A direct CLI is available for scripted use and tab completion is built in.

Runs on macOS and Linux.

## Quick start

```
jump add user@addr nick           # save a host
jump                              # open the menu
jump nick session                 # attach or create a tmux session by name
```

## Install

One-liner:

```
curl -fsSL https://raw.githubusercontent.com/spuddydev/lilypad/main/install.sh | bash
```

Two-step (inspect first):

```
curl -fsSL https://raw.githubusercontent.com/spuddydev/lilypad/main/install.sh -o install.sh
less install.sh
bash install.sh
```

From source:

```
make
sudo install -m 755 jump /usr/local/bin/jump
make install-completions
```

Requires a C compiler and ncurses (`libncurses-dev` on Debian/Ubuntu; ships with macOS).

## CLI

| Command | Result |
| --- | --- |
| `jump` | Open the ncurses menu. |
| `jump <nick>` | Open the host page (sessions sub-menu) for `<nick>`. |
| `jump <nick> <session> [--plain]` | Attach the tmux session, or create it if missing. `--plain` forces basic tmux on iTerm. |
| `jump -s <nick>` | Plain ssh, no tmux. |
| `jump add user@addr nick [jump_hosts]` | Save a host. `jump_hosts` is a comma-separated list of ProxyJump hops. |
| `jump config` | Print the current config. |
| `jump config get <key>` | Print one value. |
| `jump config set <key> <value>` | Persist a value. |
| `jump config unset <key>` | Drop back to the default. |
| `jump config undecline <nick> <letter>` | Re-enable an integration prompt that was answered "never". |

Unknown nicknames trigger a single nearest-match suggestion via length-aware Levenshtein.

## Menu keys

Host menu:

| Key | Action |
| --- | --- |
| `j`/`k`, `↑`/`↓` | Move selection. |
| `J`/`K`, shift `↑`/`↓` | Reorder host. |
| `D` | Delete host. |
| `A` | Add host inline (three prompts). |
| `Enter` | Open host page (sessions sub-menu). |
| `s` | Plain ssh. |
| `t` | Default tmux session named `main`. |
| `/` | Filter by nickname or address; `Esc` clears, `Enter` picks. |
| `r` | Reprobe the highlighted host. |
| `R` | Reprobe all visible hosts. |
| `1`–`9` | Jump to that row. |
| `q` | Quit. |

Session sub-menu:

| Key | Action |
| --- | --- |
| `j`/`k` | Move selection. |
| `D` | Kill the selected session on the remote. |
| `R` | Rename the selected session on the remote. |
| `t` | Pick this entry but force basic tmux (skip `-CC`) on iTerm. |
| `Enter` | Pick. |
| `q` | Back to host menu. |

## Markers

Red letters next to a host flag what is missing on the remote:

| Marker | Meaning |
| --- | --- |
| (empty) | Probe succeeded, tmux and tmuxp present. |
| `t` | tmux not installed. |
| `p` | tmuxp not installed. |
| `tp` | Neither installed. |
| `?` | Probe could not connect or authenticate. |

Probing runs automatically on menu open (with a `probe.cache_seconds` cache) and on demand with `r` or `R`.

## Config

`~/.config/lilypad/config` (or `$XDG_CONFIG_HOME/lilypad/config`). Plain `key = value`, `#` comments.

| Key | Default | Notes |
| --- | --- | --- |
| `integration.iterm` | `off` | Auto-set by `setup.sh` when `TERM_PROGRAM` is iTerm. |
| `integration.tmuxp` | `off` | Auto-set by `setup.sh` when `tmuxp` is on `$PATH`. |
| `probe.timeout` | `5` | Seconds, ssh `ConnectTimeout`. |
| `probe.cache_seconds` | `60` | Skip reprobe within this window. |
| `probe.max_parallel` | `8` | Concurrent probes during the menu's initial burst. |

## Integrations

When a remote is missing an integration that has an installer, the connect flow asks once: yes installs and reprobes, no skips this connection, never persists per-host so the prompt never fires again. Use `jump config undecline <nick> <letter>` to revert.

| Name | Marker | Local check | Remote install |
| --- | --- | --- | --- |
| `iterm` | (none) | `TERM_PROGRAM == iTerm.app` | n/a |
| `tmux` | `t` | (none) | n/a |
| `tmuxp` | `p` | (none) | First available of `pipx install tmuxp`, `pip3 install --user tmuxp`, `pip install --user tmuxp`. |

## Files

| Path | Owner | Purpose |
| --- | --- | --- |
| `~/.config/lilypad/hosts` | user | One `nickname user@addr [jump_hosts]` per line. |
| `~/.config/lilypad/state` | program | Markers, last-probe timestamp, cached sessions, declined letters. Percent-encoded values. |
| `~/.config/lilypad/config` | user | See table above. |
| `~/.config/lilypad/tmuxp/` | user | tmuxp template YAMLs (one is seeded automatically). |

## Layout

- `src/menu.c` — entry point and the ncurses menu loop.
- `src/cli.c` — subcommand dispatch (`add`, `config`, `_complete`, plus direct host and session paths).
- `src/exec.c` — ssh handoff, tmuxp template runner, iTerm `tmux -CC` choice.
- `src/hosts.c` — hosts file storage; legacy migration; nickname rules.
- `src/state.c` — runtime state file with percent-encoded values and a write batch API.
- `src/config.c` — `key = value` config parser and store.
- `src/integrations.c` — registry and install hooks for `iterm`, `tmux`, `tmuxp`.
- `src/probe_pool.c` — forked-ssh probe pool with per-child pipes.
- `src/ui.c` — ncurses widgets, host menu, session sub-menu, prompts.

## Contributing

Bug reports and patches welcome. Build with `make`, run with `./jump`. There is no test suite; see CLAUDE.md for project conventions.

## Licence

MIT. See `LICENSE`.
