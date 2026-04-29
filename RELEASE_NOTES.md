# v1.1.0

Bug fixes and ergonomic improvements based on first-week feedback.

## Fixes

- **Install prompt actually installs.** The remote tmuxp install command now runs with the same PATH prefix the probe uses, so pipx at `~/.local/bin` is found in non-interactive ssh shells. Falls back to `pip install --break-system-packages` on PEP 668 distros (Debian 12+, Ubuntu 23.04+, Fedora 38+). When pipx, pip3, and pip are all missing, prints the right install command for the detected package manager (apt, dnf, apk, pacman, brew). On install failure, pauses for Enter so the error doesn't scroll past behind the new ssh session.
- **Config set rejects unknown keys.** Previously `jump config set tmuxp false` silently accepted the mistyped key (it should be `integration.tmuxp`); the rogue value persisted but was hidden from `jump config` because the printer only shows known keys.

## New

- **Setup script installs completions.** Running `setup.sh` from a repo checkout now also runs `make install-completions`, so tab completion is wired up alongside the config file in one go.
- **Doxygen API docs published to GitHub Pages.** Auto-deploys on every push to main.
- **Continuous integration.** Every pull request and push to main now builds and runs the unit-test harness on Linux and macOS.
- **Branch protection.** Main now requires green CI before merge, linear history, and resolved conversations.

## Install

```
curl -fsSL https://raw.githubusercontent.com/spuddydev/lilypad/main/install.sh | bash
```

## Upgrading from 1.0.0

The hosts file, state file, and config format are unchanged. Just rebuild and reinstall.
