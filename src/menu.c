#include "cli.h"
#include "config.h"
#include "exec.h"
#include "hosts.h"
#include "integration.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int cmd_menu(void) {
    char hosts_path[MAX_PATH];
    get_hosts_path(hosts_path, sizeof(hosts_path));

    Host hosts[MAX_HOSTS];
    int count = load_hosts(hosts_path, hosts, MAX_HOSTS);

    ui_begin();

    const Host *h = NULL;
    Intent intent = INTENT_CANCEL;
    SubChoice sc = {SUB_CANCEL, "", 0};
    TemplatePick tp = {TPL_CANCEL, ""};
    char new_session[64] = "main";

    while (1) {
        HostPick hp = run_host_menu(hosts, &count, hosts_path);
        if (hp.host_index < 0) { ui_end(); return 0; }

        h = &hosts[hp.host_index];
        intent = hp.intent;
        if ((intent == INTENT_TMUX_CHOOSE || intent == INTENT_TMUX_DEFAULT) &&
            !host_has_tmux(h))
            intent = INTENT_SSH;

        if (intent == INTENT_TMUX_DEFAULT) break;
        if (intent != INTENT_TMUX_CHOOSE) break;

        ui_status("Loading sessions...");
        char sessions[16][128];
        int ns = fetch_sessions(h->host, h->jump, sessions, 16);
        if (ns < 0) ns = 0;

        int back_to_main = 0;
        while (1) {
            sc = run_tmux_menu(h->nick, h->host, h->jump, sessions, &ns, host_has_tmuxp(h));
            if (sc.kind == SUB_CANCEL) { back_to_main = 1; break; }

            if (sc.kind != SUB_NEW) break;

            char templates[32][128];
            int nt = list_templates(templates, 32);
            if (nt == 0) { tp.kind = TPL_DEFAULT; }
            else {
                tp = run_template_menu(h->nick, templates, nt);
                if (tp.kind == TPL_CANCEL) continue;
            }

            if (tp.kind == TPL_DEFAULT) {
                new_session[0] = '\0';
                if (ui_prompt("New session name (blank for auto):",
                              new_session, sizeof(new_session), NULL) != 0)
                    continue;
                if (new_session[0] == '\0')
                    auto_session_name(sessions, ns, new_session, sizeof(new_session));
            }
            break;
        }
        if (!back_to_main) break;
    }

    ui_end();

    const char *prefix = (is_iterm() && !sc.force_plain) ? "tmux -CC" : "tmux";
    char remote_cmd[1024];

    if (intent == INTENT_SSH || (intent == INTENT_TMUX_CHOOSE && sc.kind == SUB_PLAIN))
        return exec_ssh_plain(h);

    if (sc.kind == SUB_NEW && tp.kind == TPL_NAMED)
        return exec_template(h, tp.name, sc.force_plain);

    char esc[256];

    if (intent == INTENT_TMUX_DEFAULT) {
        return exec_ssh_tmux(h, "tmux new");
    }

    if (sc.kind == SUB_NEW) {
        shell_sq_escape(esc, sizeof(esc), new_session);
        snprintf(remote_cmd, sizeof(remote_cmd), "%s new -A -s '%s'", prefix, esc);
        return exec_ssh_tmux(h, remote_cmd);
    }

    if (sc.kind == SUB_ATTACH) {
        shell_sq_escape(esc, sizeof(esc), sc.session);
        snprintf(remote_cmd, sizeof(remote_cmd), "%s attach -t '%s'", prefix, esc);
        return exec_ssh_tmux(h, remote_cmd);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    migrate_legacy_config();
    config_load();
    integrations_init();
    install_default_templates();
    return cli_dispatch(argc, argv);
}
