# shellcheck shell=bash
# Bash completion for jump.
# Compatible with bash 3.2 (macOS default).

_jump_complete() {
    local cur
    cur="${COMP_WORDS[COMP_CWORD]}"

    # Slice from index 1 to and including the partial word at the cursor.
    local words=( "${COMP_WORDS[@]:1:COMP_CWORD}" )

    local IFS=$'\n'
    # Pull completions from `jump _complete` and let compgen filter by prefix.
    COMPREPLY=( $(compgen -W "$(jump _complete "${words[@]}" 2>/dev/null)" -- "$cur") )
}

complete -F _jump_complete jump
