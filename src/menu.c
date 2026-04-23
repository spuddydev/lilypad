#include "hosts.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int prompt_yn(const char *msg) {
    printf("%s [y/N] ", msg);
    fflush(stdout);
    int c = getchar();
    int yes = (c == 'y' || c == 'Y');
    while (c != '\n' && c != EOF) c = getchar();
    return yes;
}

static int do_copy_id(const char *host, const char *jump) {
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        char proxy_opt[MAX_LINE + 16];
        const char *args[8];
        int a = 0;
        args[a++] = "ssh-copy-id";
        if (jump && *jump) {
            snprintf(proxy_opt, sizeof(proxy_opt), "ProxyJump=%s", jump);
            args[a++] = "-o";
            args[a++] = proxy_opt;
        }
        args[a++] = host;
        args[a++] = NULL;
        execvp("ssh-copy-id", (char *const *)args);
        perror("ssh-copy-id");
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

static int cmd_add(int argc, char *argv[]) {
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "Usage: %s --add name@address nickname [jump_hosts]\n", argv[0]);
        return 1;
    }
    const char *host = argv[2];
    const char *nick = argv[3];
    const char *jump = argc == 5 ? argv[4] : NULL;

    char hosts_path[MAX_PATH];
    get_hosts_path(hosts_path, sizeof(hosts_path));

    int rc = add_host(hosts_path, nick, host, jump);
    if (rc != 0) return rc;

    if (jump)
        printf("Added: %s -> %s (via %s)\n", nick, host, jump);
    else
        printf("Added: %s -> %s\n", nick, host);

    printf("Probing %s... ", host);
    fflush(stdout);
    char markers[8] = "";
    probe_host(host, jump, markers, sizeof(markers));

    if (markers[0] == '?') {
        printf("unreachable or key not installed.\n");
        if (prompt_yn("Install ssh key now?")) {
            if (do_copy_id(host, jump) == 0) {
                printf("Re-probing... ");
                fflush(stdout);
                probe_host(host, jump, markers, sizeof(markers));
                printf("%s\n", markers[0] ? markers : "ok");
            }
        }
    } else if (markers[0]) {
        printf("reachable, missing: %s\n", markers);
    } else {
        printf("ok (tmux and tmuxp present)\n");
    }

    update_markers(hosts_path, nick, markers);
    return 0;
}

static int is_iterm(void) {
    const char *tp = getenv("TERM_PROGRAM");
    return tp && strcmp(tp, "iTerm.app") == 0;
}

static int host_has_tmux(const Host *h) {
    return h->markers[0] != '?' && strchr(h->markers, 't') == NULL;
}

static int host_has_tmuxp(const Host *h) {
    return h->markers[0] != '?' && strchr(h->markers, 'p') == NULL;
}

static int exec_ssh_plain(const Host *h) {
    if (h->jump[0]) execlp("ssh", "ssh", "-J", h->jump, h->host, NULL);
    else            execlp("ssh", "ssh", h->host, NULL);
    perror("ssh");
    return 1;
}

static int exec_ssh_tmux(const Host *h, const char *remote_cmd) {
    char wrapped[1024];
    snprintf(wrapped, sizeof(wrapped), REMOTE_PATH_PREFIX "%s", remote_cmd);
    if (h->jump[0]) execlp("ssh", "ssh", "-t", "-J", h->jump, h->host, wrapped, NULL);
    else            execlp("ssh", "ssh", "-t", h->host, wrapped, NULL);
    perror("ssh");
    return 1;
}

static int scp_template(const Host *h, const char *src, const char *remote_path) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }
    if (pid == 0) {
        char dest[MAX_PATH];
        snprintf(dest, sizeof(dest), "%s:%s", h->host, remote_path);
        if (h->jump[0])
            execlp("scp", "scp", "-q", "-J", h->jump, src, dest, NULL);
        else
            execlp("scp", "scp", "-q", src, dest, NULL);
        perror("scp");
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

static int exec_template(const Host *h, const char *template_name) {
    char dir[MAX_PATH], src[MAX_PATH];
    get_templates_dir(dir, sizeof(dir));
    snprintf(src, sizeof(src), "%s/%s.yaml", dir, template_name);
    if (access(src, R_OK) != 0)
        snprintf(src, sizeof(src), "%s/%s.yml", dir, template_name);
    if (access(src, R_OK) != 0) {
        fprintf(stderr, "Template not found: %s\n", template_name);
        return 1;
    }
    char remote_path[MAX_PATH];
    snprintf(remote_path, sizeof(remote_path), "/tmp/ssh-menu-%s.yaml", template_name);
    if (scp_template(h, src, remote_path) != 0) {
        fprintf(stderr, "scp failed\n");
        return 1;
    }
    char remote_cmd[512];
    snprintf(remote_cmd, sizeof(remote_cmd),
             "tmuxp load -y %s; rm -f %s", remote_path, remote_path);
    return exec_ssh_tmux(h, remote_cmd);
}

static int cmd_template_add(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s --template-add path/to/file.yaml\n", argv[0]);
        return 1;
    }
    return install_template_from_path(argv[2]) == 0 ? 0 : 1;
}

static int cmd_menu(void) {
    char hosts_path[MAX_PATH];
    get_hosts_path(hosts_path, sizeof(hosts_path));

    Host hosts[MAX_HOSTS];
    int count = load_hosts(hosts_path, hosts, MAX_HOSTS);

    ui_begin();

    const Host *h = NULL;
    Intent intent = INTENT_CANCEL;
    SubChoice sc = {SUB_CANCEL, ""};
    TemplatePick tp = {TPL_CANCEL, ""};
    char new_session[64] = "main";

    while (1) {
        HostPick hp = run_host_menu(hosts, count, hosts_path);
        if (hp.host_index < 0) { ui_end(); return 0; }

        h = &hosts[hp.host_index];
        intent = hp.intent;
        if ((intent == INTENT_TMUX_CHOOSE || intent == INTENT_TMUX_DEFAULT) &&
            !host_has_tmux(h))
            intent = INTENT_SSH;

        if (intent == INTENT_TMUX_DEFAULT) {
            if (ui_prompt("New tmux session name:", new_session, sizeof(new_session), "main") != 0)
                continue;
            break;
        }
        if (intent != INTENT_TMUX_CHOOSE) break;

        ui_status("Loading sessions...");
        char sessions[16][128];
        int ns = fetch_sessions(h->host, h->jump, sessions, 16);
        if (ns < 0) ns = 0;

        int back_to_main = 0;
        while (1) {
            sc = run_tmux_menu(h->nick, sessions, ns, host_has_tmuxp(h));
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
                if (ui_prompt("New tmux session name:", new_session, sizeof(new_session), "main") != 0)
                    continue;
            }
            break;
        }
        if (!back_to_main) break;
    }

    ui_end();

    const char *prefix = is_iterm() ? "tmux -CC" : "tmux";
    char remote_cmd[256];

    if (intent == INTENT_SSH || (intent == INTENT_TMUX_CHOOSE && sc.kind == SUB_PLAIN))
        return exec_ssh_plain(h);

    if (sc.kind == SUB_NEW && tp.kind == TPL_NAMED)
        return exec_template(h, tp.name);

    if (intent == INTENT_TMUX_DEFAULT || sc.kind == SUB_NEW) {
        snprintf(remote_cmd, sizeof(remote_cmd), "%s new -A -s '%s'", prefix, new_session);
        return exec_ssh_tmux(h, remote_cmd);
    }

    if (sc.kind == SUB_ATTACH) {
        snprintf(remote_cmd, sizeof(remote_cmd), "%s attach -t '%s'", prefix, sc.session);
        return exec_ssh_tmux(h, remote_cmd);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    install_default_templates();
    if (argc > 1 && strcmp(argv[1], "--add") == 0)
        return cmd_add(argc, argv);
    if (argc > 1 && strcmp(argv[1], "--template-add") == 0)
        return cmd_template_add(argc, argv);
    return cmd_menu();
}
