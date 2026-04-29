#include "integration.h"
#include "config.h"
#include "exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int detect_tmux_local(void) {
    /* Local presence is not a hard gate; remote check is what matters. */
    return 1;
}

static int detect_tmuxp_local(void) {
    return 1;
}

static int tmuxp_offer_install(const Host *h) {
    static const char *const remote_cmd =
        REMOTE_PATH_PREFIX
        "if command -v pipx >/dev/null 2>&1; then "
        "  pipx install tmuxp; "
        "elif command -v pip3 >/dev/null 2>&1; then "
        "  pip3 install --user tmuxp || "
        "    pip3 install --user --break-system-packages tmuxp; "
        "elif command -v pip >/dev/null 2>&1; then "
        "  pip install --user tmuxp || "
        "    pip install --user --break-system-packages tmuxp; "
        "else "
        "  echo 'lilypad: no pipx, pip3, or pip on the remote.' >&2; "
        "  if   command -v apt-get >/dev/null 2>&1; then "
        "    echo '  install pipx with: sudo apt install pipx' >&2; "
        "  elif command -v dnf     >/dev/null 2>&1; then "
        "    echo '  install pipx with: sudo dnf install pipx' >&2; "
        "  elif command -v apk     >/dev/null 2>&1; then "
        "    echo '  install pipx with: sudo apk add pipx' >&2; "
        "  elif command -v pacman  >/dev/null 2>&1; then "
        "    echo '  install pipx with: sudo pacman -S python-pipx' >&2; "
        "  elif command -v brew    >/dev/null 2>&1; then "
        "    echo '  install pipx with: brew install pipx' >&2; "
        "  fi; "
        "  exit 1; "
        "fi";
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        const char *args[16];
        int a = 0;
        args[a++] = "ssh";
        args[a++] = "-t";
        if (h->jump[0]) { args[a++] = "-J"; args[a++] = (char *)h->jump; }
        args[a++] = (char *)h->host;
        args[a++] = (char *)remote_cmd;
        args[a++] = NULL;
        execvp("ssh", (char *const *)args);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

static Integration registry[] = {
    {
        .name = "iterm",
        .kind = KIND_LOCAL,
        .marker_letter = 0,
        .detect_local = is_iterm,
        .probe_remote_cmd = NULL,
        .offer_install = NULL,
        .enabled = 0,
    },
    {
        .name = "tmux",
        .kind = KIND_REMOTE,
        .marker_letter = 't',
        .detect_local = detect_tmux_local,
        .probe_remote_cmd = "command -v tmux >/dev/null && echo I_t",
        .offer_install = NULL,
        .enabled = 1,
    },
    {
        .name = "tmuxp",
        .kind = KIND_REMOTE,
        .marker_letter = 'p',
        .detect_local = detect_tmuxp_local,
        .probe_remote_cmd = "command -v tmuxp >/dev/null && echo I_p",
        .offer_install = tmuxp_offer_install,
        .enabled = 1,
    },
};

#define REGISTRY_LEN ((int)(sizeof(registry) / sizeof(registry[0])))

const Integration *integration_at(int index) {
    if (index < 0 || index >= REGISTRY_LEN) return NULL;
    return &registry[index];
}

void integrations_init(void) {
    for (int i = 0; i < REGISTRY_LEN; i++) {
        Integration *it = &registry[i];
        char key[64];
        snprintf(key, sizeof(key), "integration.%s", it->name);
        int detected = it->detect_local ? it->detect_local() : 1;
        if (it->kind == KIND_LOCAL)
            it->enabled = config_get_bool(key, 0) && detected;
        else
            it->enabled = config_get_bool(key, 1);
    }
}
