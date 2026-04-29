# v1.0.0

First tagged release of lilypad (formerly ssh-menu).

## Highlights

- Renamed binary `jump` and config directory `~/.config/lilypad/`. Existing `~/.config/ssh-menu/` is migrated automatically on first run.
- New CLI surface: `jump <nick>`, `jump <nick> <session> [--plain]`, `jump -s <nick>`, `jump add ...`, `jump config ...`. Bare `jump` still opens the menu.
- Live threaded probing: the menu now probes all hosts in parallel on open, with a per-host cache and a clean shutdown on quit.
- Config file at `~/.config/lilypad/config` with integration toggles and probe tuning. `setup.sh` seeds it from local detection.
- Integration registry covering iterm, tmux, tmuxp. tmuxp's missing-on-remote prompt asks once and remembers.
- Per-host runtime data moves to a separate `state` file with percent-encoded values, leaving the user-edited `hosts` file clean.
- Inline shift-A flow to add a host without leaving the menu.
- Tab completion for bash and zsh; install via `make install-completions` or the curl one-liner.
- Doxygen API docs with the doxygen-awesome theme; build via `make docs`.

## Install

```
curl -fsSL https://raw.githubusercontent.com/spuddydev/lilypad/main/install.sh | bash
```

Or two-step:

```
curl -fsSL https://raw.githubusercontent.com/spuddydev/lilypad/main/install.sh -o install.sh
less install.sh
bash install.sh
```

## Notes

- macOS Intel (`darwin-x86_64`) and Apple Silicon (`darwin-arm64`) prebuilds available; Linux x86_64 prebuilt; Linux arm64 builds from source for now.
- Homebrew tap setup follows separately at `spuddydev/homebrew-lilypad`.
