# v1.1.1

Small ergonomic fixes.

## Changes

- **Setup script updates an existing install.** When run from a repo checkout, if a jump binary is already on PATH, setup now rebuilds and reinstalls into the same location. If the install directory is not writable, prints the exact sudo command to finish.
- **Doxygen landing page is no longer empty.** The generated index now uses README.md as the main page, so the GitHub Pages site at https://spuddydev.github.io/lilypad/ actually shows content.

## Install

```
curl -fsSL https://raw.githubusercontent.com/spuddydev/lilypad/main/install.sh | bash
```

## Upgrading

```
cd path/to/lilypad
git pull
bash setup.sh
```
