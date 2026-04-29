@mainpage Lilypad

Internal API documentation for [lilypad](https://github.com/spuddydev/lilypad), the ssh launcher with tmux integration.

The user-facing docs (install, CLI, keybindings) live in the [README](https://github.com/spuddydev/lilypad#readme). This site is for contributors and the curious — it covers the C API surface, the on-disk file formats, and the runtime model.

@section sec_layout Layout at a glance

The codebase is small and split into a handful of translation units. Each owns one concern:

| Unit | Concern |
| --- | --- |
| `src/menu.c` | `main` and the menu-mode entry point |
| `src/cli.c` | Subcommand dispatch (`add`, `config`, direct host and session paths, `_complete`) |
| `src/exec.c` | ssh handoff, tmuxp template runner, iTerm `tmux -CC` choice |
| `src/hosts.c` | Hosts file storage, legacy migration, nickname rules, remote probe |
| `src/state.c` | Runtime state file with percent-encoded values and a write batch API |
| `src/config.c` | `key = value` config parser and store |
| `src/integrations.c` | Registry and install hooks for `iterm`, `tmux`, `tmuxp` |
| `src/probe_pool.c` | Forked-ssh probe pool with per-child pipes |
| `src/ui.c` | ncurses widgets, host menu, session sub-menu, prompts |

@section sec_files On-disk files

Three files under `~/.config/lilypad/`:

- **`hosts`** — user-edited. One `nickname user@addr [jump_hosts]` per line.
- **`state`** — program-managed. Percent-encoded `key=value` pairs per host: markers, last probe time, cached session list, declined integration letters.
- **`config`** — user-edited. Plain `key = value`, `#` comments. See @ref config.h.

@section sec_runtime Runtime model

- **No dynamic allocation.** All buffers are fixed-size stack arrays sized from `common.h`.
- **`execlp` handoff.** When the user picks a host the menu process is replaced by ssh. There is no parent watching the connection.
- **Probe pool.** On menu open every host is probed in parallel through a forked ssh pool. Results are streamed back via per-child pipes and written to state in batched commits.
- **Integration registry.** Static array in `src/integrations.c`. Each entry is local-only (`iterm`) or remote-probed (`tmux`, `tmuxp`) with a single-letter marker.

@section sec_modules Where to start reading

- The menu loop: @ref ui.h "ui.h" and `src/ui.c`'s `run_host_menu`.
- The probe model: @ref probe_pool.h "probe_pool.h".
- The state file format: @ref state.h "state.h".
- The CLI dispatch table: `src/cli.c`'s `cli_dispatch`.
