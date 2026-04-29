#include "cli.h"
#include "exec.h"
#include "hosts.h"

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
        fprintf(stderr, "Usage: %s add user@address nickname [jump_hosts]\n", argv[0]);
        return 1;
    }
    const char *host = argv[2];
    const char *nick = argv[3];
    const char *jump = argc == 5 ? argv[4] : NULL;

    char hosts_path[MAX_PATH];
    get_hosts_path(hosts_path, sizeof(hosts_path));

    int rc = add_host(hosts_path, nick, host, jump);
    if (rc != 0) return rc;

    if (jump) printf("Added: %s -> %s (via %s)\n", nick, host, jump);
    else      printf("Added: %s -> %s\n", nick, host);

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

static int cmd_template_add(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s --template-add path/to/file.yaml\n", argv[0]);
        return 1;
    }
    return install_template_from_path(argv[2]) == 0 ? 0 : 1;
}

static int cmd_config_placeholder(void) {
    fprintf(stderr, "jump config: not implemented yet\n");
    return 1;
}

static int find_nick(const Host *hosts, int n, const char *nick) {
    for (int i = 0; i < n; i++)
        if (strcmp(hosts[i].nick, nick) == 0) return i;
    return -1;
}

static int cmd_ssh(const char *nick) {
    char path[MAX_PATH];
    get_hosts_path(path, sizeof(path));
    Host hosts[MAX_HOSTS];
    int n = load_hosts(path, hosts, MAX_HOSTS);
    int idx = find_nick(hosts, n, nick);
    if (idx < 0) {
        fprintf(stderr, "Unknown host: %s\n", nick);
        return 1;
    }
    return exec_ssh_plain(&hosts[idx]);
}

static int cmd_direct_session(const char *nick, const char *session, int force_plain) {
    char path[MAX_PATH];
    get_hosts_path(path, sizeof(path));
    Host hosts[MAX_HOSTS];
    int n = load_hosts(path, hosts, MAX_HOSTS);
    int idx = find_nick(hosts, n, nick);
    if (idx < 0) {
        fprintf(stderr, "Unknown host: %s\n", nick);
        return 1;
    }
    char esc[256];
    shell_sq_escape(esc, sizeof(esc), session);
    const char *prefix = (is_iterm() && !force_plain) ? "tmux -CC" : "tmux";
    char remote_cmd[1024];
    snprintf(remote_cmd, sizeof(remote_cmd), "%s new -A -s '%s'", prefix, esc);
    return exec_ssh_tmux(&hosts[idx], remote_cmd);
}

static int cmd_direct_host(const char *nick) {
    /* Host page UI lands in a follow-up commit. For now, suggest the menu. */
    fprintf(stderr,
            "jump %s: open the menu (`jump`) and select '%s' to pick a session.\n",
            nick, nick);
    return 1;
}

int cli_dispatch(int argc, char *argv[]) {
    if (argc <= 1) return cmd_menu();

    const char *a = argv[1];

    if (strcmp(a, "add") == 0)            return cmd_add(argc, argv);
    if (strcmp(a, "config") == 0)         return cmd_config_placeholder();
    if (strcmp(a, "--template-add") == 0) return cmd_template_add(argc, argv);

    if (strcmp(a, "-s") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s -s <nick>\n", argv[0]);
            return 1;
        }
        return cmd_ssh(argv[2]);
    }

    if (a[0] == '-') {
        fprintf(stderr, "Unknown option: %s\n", a);
        return 1;
    }

    /* `jump <nick>` or `jump <nick> <session> [--plain]` */
    const char *nick = a;
    if (argc == 2) return cmd_direct_host(nick);

    const char *session = argv[2];
    int force_plain = (argc >= 4 && strcmp(argv[3], "--plain") == 0);
    return cmd_direct_session(nick, session, force_plain);
}
