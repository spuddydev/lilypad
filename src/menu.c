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

static int exec_ssh_plain(const Host *h) {
    if (h->jump[0]) execlp("ssh", "ssh", "-J", h->jump, h->host, NULL);
    else            execlp("ssh", "ssh", h->host, NULL);
    perror("ssh");
    return 1;
}

static int exec_ssh_tmux(const Host *h, const char *remote_cmd) {
    if (h->jump[0]) execlp("ssh", "ssh", "-t", "-J", h->jump, h->host, remote_cmd, NULL);
    else            execlp("ssh", "ssh", "-t", h->host, remote_cmd, NULL);
    perror("ssh");
    return 1;
}

static int cmd_menu(void) {
    char hosts_path[MAX_PATH];
    get_hosts_path(hosts_path, sizeof(hosts_path));

    Host hosts[MAX_HOSTS];
    int count = load_hosts(hosts_path, hosts, MAX_HOSTS);

    ui_begin();
    HostPick hp = run_host_menu(hosts, count, hosts_path);
    if (hp.host_index < 0) { ui_end(); return 0; }

    const Host *h = &hosts[hp.host_index];
    Intent intent = hp.intent;
    if ((intent == INTENT_TMUX_CHOOSE || intent == INTENT_TMUX_DEFAULT) &&
        !host_has_tmux(h))
        intent = INTENT_SSH;

    SubChoice sc = {SUB_CANCEL, ""};
    if (intent == INTENT_TMUX_CHOOSE) {
        ui_status("Loading sessions...");
        char sessions[16][128];
        int ns = fetch_sessions(h->host, h->jump, sessions, 16);
        if (ns < 0) ns = 0;
        sc = run_tmux_menu(h->nick, sessions, ns);
        if (sc.kind == SUB_CANCEL) { ui_end(); return 0; }
    }

    ui_end();

    const char *prefix = is_iterm() ? "tmux -CC" : "tmux";
    char remote_cmd[256];

    if (intent == INTENT_SSH || (intent == INTENT_TMUX_CHOOSE && sc.kind == SUB_PLAIN))
        return exec_ssh_plain(h);

    if (intent == INTENT_TMUX_DEFAULT || sc.kind == SUB_NEW) {
        snprintf(remote_cmd, sizeof(remote_cmd), "%s new -A -s main", prefix);
        return exec_ssh_tmux(h, remote_cmd);
    }

    if (sc.kind == SUB_ATTACH) {
        snprintf(remote_cmd, sizeof(remote_cmd), "%s attach -t '%s'", prefix, sc.session);
        return exec_ssh_tmux(h, remote_cmd);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--add") == 0)
        return cmd_add(argc, argv);
    return cmd_menu();
}
