#!/usr/bin/env bash
# Detect local prerequisites and write an initial lilypad config.
# Re-running is safe: the file is overwritten with the detected state.

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"

# If we're in a repo and lilypad is already installed, rebuild and update it.
if [ -f "$script_dir/Makefile" ]; then
    existing="$(command -v jump || true)"
    if [ -n "$existing" ]; then
        echo "found existing install at $existing — rebuilding..."
        make -C "$script_dir"
        if [ -w "$existing" ]; then
            install -m 755 "$script_dir/jump" "$existing"
            echo "updated $existing"
        else
            echo
            echo "$existing is not writable. To finish updating, run:"
            echo "  sudo install -m 755 $script_dir/jump $existing"
        fi
        echo
    fi
fi

CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/lilypad"
CONFIG_FILE="$CONFIG_DIR/config"

mkdir -p "$CONFIG_DIR"

iterm="off"
if [ "${TERM_PROGRAM:-}" = "iTerm.app" ]; then
    iterm="on"
fi

tmuxp="off"
if command -v tmuxp >/dev/null 2>&1; then
    tmuxp="on"
fi

cat >"$CONFIG_FILE" <<EOF
# lilypad config
integration.iterm = $iterm
integration.tmuxp = $tmuxp
probe.timeout = 5
probe.max_parallel = 8
EOF

echo "wrote $CONFIG_FILE"
echo "  integration.iterm = $iterm"
echo "  integration.tmuxp = $tmuxp"

# Install shell completions if we're running from a repo checkout.
if [ -f "$script_dir/Makefile" ] && [ -f "$script_dir/completions/jump.bash" ]; then
    echo
    echo "installing shell completions..."
    make -C "$script_dir" install-completions
else
    echo
    echo "note: run 'make install-completions' from the repo to enable tab completion."
fi
