#include "integration.h"
#include "config.h"
#include "exec.h"

#include <stdio.h>
#include <string.h>

static int detect_tmux_local(void) {
    /* Local presence is not a hard gate; remote check is what matters. */
    return 1;
}

static int detect_tmuxp_local(void) {
    return 1;
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
        .offer_install = NULL,
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
