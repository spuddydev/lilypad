# v1.1.2

Two field-reported fixes.

## Changes

- **`jump --help`, `-h`, and `--version` now work.** They previously errored with "Unknown option". Version is read from a single constant in the source, bumped per release.
- **Zsh completion no longer errors on tab.** Every press after `jump` was throwing three "unrecognized modifier C" warnings, caused by an unused line that used bash-style array slicing in the zsh script.

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
