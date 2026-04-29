# v1.1.4

Live state and a smoother first-run setup.

## New

- **Post-disconnect reprobe.** The connect path now forks ssh as a child instead of replacing the menu process. After ssh exits, a detached background probe refreshes markers and session names. Tab completion stays current without the user ever opening the menu again, and an open menu in another terminal sees the new state on its next tick.
- **Install auto-edits your shell rc.** When falling back to user-only completion paths, the install step appends an idempotent marker block to `.zshrc` and `.bashrc` so tab completion just works after a shell restart. Uninstall removes the same block.
- **CLI smoke tests cover the rc edits** and run on every PR.

## Fixes

- **Doxygen mainpage now renders.** Bold-around-inline-code (`**\`hosts\`**`) was rendering literally; switched to bold-only and refreshed the runtime model paragraph that still described the old execlp handoff.
- **Doxygen sidebar shows.** `DISABLE_INDEX = YES` is required by doxygen-awesome's sidebar-only layout.
- **Doxygen search bar removed.** The default search box collided with the sidebar; the repo is small enough that browsing the tree beats it anyway.

## Install

```
curl -fsSL https://raw.githubusercontent.com/spuddydev/lilypad/main/install.sh | bash
```

## Upgrading

```
jump update
```
