# v1.1.3

Self-management subcommands and a polished docs site.

## New

- **`jump update [--check]`** fetches the latest tag via `git ls-remote`, compares it to the compiled-in version, and runs the install script when newer. `--check` only prints the comparison.
- **`jump uninstall`** removes the binary, both bash and zsh completion files in standard system and user paths, and (with a y/N confirm) the config directory. Detects its own binary path through `/proc/self/exe` on Linux and `_NSGetExecutablePath` on macOS.
- **CLI smoke tests.** A `tests/cli.sh` harness runs alongside the unit tests on every PR. Catches the kind of regressions that have been hitting in the field: missing flags, config set accepting bogus keys, suggestion behaviour.

## Changes

- **Doxygen site polish.** Real curated landing page with a layout overview and pointers to where to start reading, lilypad green colour theme with dark-mode equivalents, eight `@defgroup` sections so the public API is grouped in the sidebar and Topics page, source browser with caller and callee cross-links, plus heading anchors, an interactive table of contents, and tabs.

## Install

```
curl -fsSL https://raw.githubusercontent.com/spuddydev/lilypad/main/install.sh | bash
```

## Upgrading

```
jump update
```

Or if you cloned the repo:

```
cd path/to/lilypad
git pull
bash setup.sh
```
