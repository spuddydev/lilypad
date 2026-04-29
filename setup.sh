#!/usr/bin/env bash
# Detect local prerequisites and write an initial lilypad config.
# Re-running is safe: the file is overwritten with the detected state.

set -euo pipefail

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
probe.cache_seconds = 60
probe.max_parallel = 8
EOF

echo "wrote $CONFIG_FILE"
echo "  integration.iterm = $iterm"
echo "  integration.tmuxp = $tmuxp"

# Install shell completions if we're running from a repo checkout.
script_dir="$(cd "$(dirname "$0")" && pwd)"
if [ -f "$script_dir/Makefile" ] && [ -f "$script_dir/completions/jump.bash" ]; then
    echo
    echo "installing shell completions..."
    make -C "$script_dir" install-completions
else
    echo
    echo "note: run 'make install-completions' from the repo to enable tab completion."
fi
