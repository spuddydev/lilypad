#!/usr/bin/env bash
# CLI smoke tests. Run from repo root via `make test-cli`.

set -uo pipefail

JUMP="${JUMP:-./jump}"
fails=0

assert_eq() {
    if [ "$1" != "$2" ]; then
        echo "FAIL: $3"
        echo "  expected: $2"
        echo "  got:      $1"
        fails=$((fails + 1))
    fi
}

assert_contains() {
    if ! echo "$1" | grep -qF "$2"; then
        echo "FAIL: $3"
        echo "  output did not contain: $2"
        fails=$((fails + 1))
    fi
}

# --help exits 0 with Usage banner
out=$("$JUMP" --help 2>&1); rc=$?
assert_eq "$rc" "0" "jump --help exits 0"
assert_contains "$out" "Usage:" "jump --help prints Usage"
assert_contains "$out" "uninstall" "jump --help mentions uninstall"

# -h exits 0
out=$("$JUMP" -h 2>&1); rc=$?
assert_eq "$rc" "0" "jump -h exits 0"
assert_contains "$out" "Usage:" "jump -h prints Usage"

# --version exits 0
out=$("$JUMP" --version 2>&1); rc=$?
assert_eq "$rc" "0" "jump --version exits 0"
assert_contains "$out" "lilypad " "jump --version prints product"

# config set rejects unknown keys
out=$("$JUMP" config set definitely_not_a_key x 2>&1); rc=$?
[ "$rc" -ne 0 ] || { echo "FAIL: config set bogus key did not exit non-zero"; fails=$((fails + 1)); }
assert_contains "$out" "unknown key" "config set names the key"

# config set accepts schema keys
out=$("$JUMP" config set probe.timeout 7 2>&1); rc=$?
assert_eq "$rc" "0" "config set on valid key exits 0"

# config get round-trips
out=$("$JUMP" config get probe.timeout 2>&1); rc=$?
assert_eq "$rc" "0" "config get exits 0"
assert_eq "$out" "7" "config get returns the persisted value"

# Reset for cleanliness
"$JUMP" config unset probe.timeout >/dev/null 2>&1

# Unknown subcommand suggestion
out=$("$JUMP" cnfig 2>&1); rc=$?
[ "$rc" -ne 0 ] || { echo "FAIL: unknown command did not exit non-zero"; fails=$((fails + 1)); }
assert_contains "$out" "Did you mean" "unknown command suggests one"

# install-completions auto-edits .zshrc and .bashrc idempotently
fixture=$(mktemp -d)
touch "$fixture/.zshrc" "$fixture/.bashrc"
HOME="$fixture" XDG_DATA_HOME="$fixture/data" LILYPAD_USER_ONLY=1 make install-completions >/dev/null 2>&1
HOME="$fixture" XDG_DATA_HOME="$fixture/data" LILYPAD_USER_ONLY=1 make install-completions >/dev/null 2>&1
zsh_blocks=$(grep -c '>>> lilypad completions >>>' "$fixture/.zshrc" 2>/dev/null || echo 0)
bash_blocks=$(grep -c '>>> lilypad completions >>>' "$fixture/.bashrc" 2>/dev/null || echo 0)
assert_eq "$zsh_blocks" "1" "zshrc lilypad block written exactly once"
assert_eq "$bash_blocks" "1" "bashrc lilypad block written exactly once"
rm -rf "$fixture"

if [ "$fails" -gt 0 ]; then
    echo
    echo "$fails CLI test(s) failed"
    exit 1
fi
echo "all CLI tests passed"
