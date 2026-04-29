#!/usr/bin/env bash
# Build and install lilypad.
# Pinned to the latest tag on github.com/spuddydev/lilypad. PREFIX is
# /usr/local when writable; otherwise ~/.local with a PATH warning.

set -euo pipefail

REPO="https://github.com/spuddydev/lilypad"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

# 1. Pick the latest semver tag.
LATEST="$(git ls-remote --tags --refs "$REPO" \
  | awk -F/ '{print $NF}' \
  | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' \
  | sort -V \
  | tail -1)"

if [ -z "${LATEST:-}" ]; then
    echo "lilypad: no tagged release found at $REPO" >&2
    exit 1
fi

echo "lilypad: installing $LATEST"

# 2. Dependency check: ncurses headers and a C compiler.
if ! command -v cc >/dev/null && ! command -v gcc >/dev/null; then
    echo "lilypad: need cc or gcc on PATH" >&2
    exit 1
fi

if ! cc -E -include ncurses.h - </dev/null >/dev/null 2>&1; then
    echo "lilypad: ncurses headers not found." >&2
    case "$(uname -s)" in
        Darwin) echo "  macOS usually has them; if missing, install via 'brew install ncurses'." >&2 ;;
        Linux)
            if   command -v apt-get >/dev/null; then echo "  apt:  sudo apt install libncurses-dev" >&2
            elif command -v dnf     >/dev/null; then echo "  dnf:  sudo dnf install ncurses-devel" >&2
            elif command -v apk     >/dev/null; then echo "  apk:  sudo apk add ncurses-dev" >&2
            fi
            ;;
    esac
    exit 1
fi

# 3. Pick PREFIX.
if [ -w /usr/local/bin ] 2>/dev/null; then
    PREFIX="/usr/local"
else
    PREFIX="$HOME/.local"
    mkdir -p "$PREFIX/bin"
fi

# 4. Clone, build, install.
git clone --depth 1 --branch "$LATEST" "$REPO" "$TMPDIR/lilypad"
make -C "$TMPDIR/lilypad"
install -m 755 "$TMPDIR/lilypad/jump" "$PREFIX/bin/jump"

# 5. Completions.
make -C "$TMPDIR/lilypad" install-completions || true

echo "lilypad: installed $PREFIX/bin/jump"

case ":$PATH:" in
    *":$PREFIX/bin:"*) ;;
    *) echo "lilypad: warning, $PREFIX/bin is not on \$PATH" >&2 ;;
esac
