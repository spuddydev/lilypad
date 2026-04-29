#include "cli.h"
#include "config.h"
#include "exec.h"
#include "hosts.h"
#include "integration.h"
#include "state.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

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

static int cmd_config(int argc, char *argv[]) {
    if (argc == 2) {
        config_print();
        return 0;
    }
    const char *op = argv[2];
    if (strcmp(op, "get") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s config get <key>\n", argv[0]);
            return 1;
        }
        const char *v = config_get_str(argv[3], NULL);
        if (!v) return 1;
        printf("%s\n", v);
        return 0;
    }
    if (strcmp(op, "set") == 0) {
        if (argc != 5) {
            fprintf(stderr, "Usage: %s config set <key> <value>\n", argv[0]);
            return 1;
        }
        int known = 0;
        for (int i = 0; ; i++) {
            const char *k = config_known_key(i);
            if (!k) break;
            if (strcmp(k, argv[3]) == 0) { known = 1; break; }
        }
        if (!known) {
            fprintf(stderr, "config: unknown key '%s'. Run '%s config' to see valid keys.\n",
                    argv[3], argv[0]);
            return 1;
        }
        if (config_set_str(argv[3], argv[4]) != 0) {
            fprintf(stderr, "config: too many keys\n");
            return 1;
        }
        return config_save();
    }
    if (strcmp(op, "unset") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s config unset <key>\n", argv[0]);
            return 1;
        }
        config_unset(argv[3]);
        return config_save();
    }
    if (strcmp(op, "undecline") == 0) {
        if (argc != 5 || strlen(argv[4]) != 1) {
            fprintf(stderr, "Usage: %s config undecline <nick> <letter>\n", argv[0]);
            return 1;
        }
        if (state_remove_decline(argv[3], argv[4][0]) != 0) {
            fprintf(stderr, "no decline for %s/%s\n", argv[3], argv[4]);
            return 1;
        }
        return state_save();
    }
    fprintf(stderr, "Usage: %s config [get|set|unset|undecline] ...\n", argv[0]);
    return 1;
}

static int find_nick(const Host *hosts, int n, const char *nick) {
    for (int i = 0; i < n; i++)
        if (strcmp(hosts[i].nick, nick) == 0) return i;
    return -1;
}

static int lev_distance(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    if (la > 32 || lb > 32) return 999;
    int prev[33], curr[33];
    for (size_t j = 0; j <= lb; j++) prev[j] = (int)j;
    for (size_t i = 1; i <= la; i++) {
        curr[0] = (int)i;
        for (size_t j = 1; j <= lb; j++) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            int del = prev[j] + 1;
            int ins = curr[j-1] + 1;
            int sub = prev[j-1] + cost;
            int m = del < ins ? del : ins;
            curr[j] = sub < m ? sub : m;
        }
        for (size_t j = 0; j <= lb; j++) prev[j] = curr[j];
    }
    return prev[lb];
}

static void suggest_nick(const char *q, const Host *hosts, int n) {
    static const char *const cmds[] = { "add", "config", "update", "uninstall", NULL };
    int qlen = (int)strlen(q);
    int threshold = qlen <= 4 ? 1 : 2;
    int best_d = threshold + 1;
    const char *best = NULL;
    for (int i = 0; i < n; i++) {
        int d = lev_distance(q, hosts[i].nick);
        if (d < best_d) { best_d = d; best = hosts[i].nick; }
    }
    for (int i = 0; cmds[i]; i++) {
        int d = lev_distance(q, cmds[i]);
        if (d < best_d) { best_d = d; best = cmds[i]; }
    }
    if (best) fprintf(stderr, "Did you mean '%s'?\n", best);
}

static int cmd_ssh(const char *nick) {
    char path[MAX_PATH];
    get_hosts_path(path, sizeof(path));
    Host hosts[MAX_HOSTS];
    int n = load_hosts(path, hosts, MAX_HOSTS);
    int idx = find_nick(hosts, n, nick);
    if (idx < 0) {
        fprintf(stderr, "Unknown host: %s\n", nick);
        suggest_nick(nick, hosts, n);
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
        suggest_nick(nick, hosts, n);
        return 1;
    }
    char esc[256];
    shell_sq_escape(esc, sizeof(esc), session);
    const char *prefix = (is_iterm() && !force_plain) ? "tmux -CC" : "tmux";
    char remote_cmd[1024];
    snprintf(remote_cmd, sizeof(remote_cmd), "%s new -A -s '%s'", prefix, esc);
    return exec_ssh_tmux(&hosts[idx], remote_cmd);
}

/* Mirrors the post-pick interaction in cmd_menu (src/menu.c). Cancel from the
   sub-menu exits rather than returning to a host list, since this path was
   entered by nickname. */
static int cmd_direct_host(const char *nick) {
    char path[MAX_PATH];
    get_hosts_path(path, sizeof(path));
    Host hosts[MAX_HOSTS];
    int n = load_hosts(path, hosts, MAX_HOSTS);
    int idx = find_nick(hosts, n, nick);
    if (idx < 0) {
        fprintf(stderr, "Unknown host: %s\n", nick);
        suggest_nick(nick, hosts, n);
        return 1;
    }
    const Host *h = &hosts[idx];

    if (!host_has_tmux(h)) {
        return exec_ssh_plain(h);
    }

    ui_begin();
    ui_status("Loading sessions...");
    char sessions[16][128];
    int ns = fetch_sessions(h->host, h->jump, sessions, 16);
    if (ns < 0) ns = 0;

    SubChoice sc = {SUB_CANCEL, "", 0};
    TemplatePick tp = {TPL_CANCEL, ""};
    char new_session[64] = "main";

    while (1) {
        sc = run_tmux_menu(h->nick, h->host, h->jump, sessions, &ns, host_has_tmuxp(h));
        if (sc.kind == SUB_CANCEL) { ui_end(); return 0; }
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

    char to_install[8] = "";
    if (sc.kind != SUB_PLAIN)
        prompt_install_decisions(h, to_install, sizeof(to_install));

    ui_end();
    apply_install_decisions(h, to_install);

    const char *prefix = (is_iterm() && !sc.force_plain) ? "tmux -CC" : "tmux";
    char remote_cmd[1024];
    char esc[256];

    if (sc.kind == SUB_PLAIN) return exec_ssh_plain(h);
    if (sc.kind == SUB_NEW && tp.kind == TPL_NAMED)
        return exec_template(h, tp.name, sc.force_plain);
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

void prompt_install_decisions(const Host *h, char *out, size_t out_size) {
    if (out_size > 0) out[0] = '\0';
    if (h->markers[0] == '?') return;
    HostState *st = state_get(h->nick);
    size_t pos = 0;
    for (int i = 0; ; i++) {
        const Integration *it = integration_at(i);
        if (!it) break;
        if (it->kind != KIND_REMOTE) continue;
        if (!it->enabled) continue;
        if (!it->offer_install) continue;
        if (!it->marker_letter) continue;
        if (!strchr(h->markers, it->marker_letter)) continue;
        if (st && strchr(st->declined, it->marker_letter)) continue;

        char msg[256];
        snprintf(msg, sizeof(msg), "Install %s on %s?", it->name, h->nick);
        UiConfirm c = ui_confirm3(msg);
        if (c == UI_CONFIRM_YES) {
            if (pos + 1 < out_size) out[pos++] = it->marker_letter;
        } else if (c == UI_CONFIRM_NEVER) {
            state_add_decline(h->nick, it->marker_letter);
        }
    }
    if (out_size > 0) out[pos < out_size ? pos : out_size - 1] = '\0';
}

void apply_install_decisions(const Host *h, const char *letters) {
    if (!letters || !*letters) return;
    int any_failed = 0;
    for (const char *p = letters; *p; p++) {
        const Integration *target = NULL;
        for (int i = 0; ; i++) {
            const Integration *it = integration_at(i);
            if (!it) break;
            if (it->marker_letter == *p) { target = it; break; }
        }
        if (!target || !target->offer_install) continue;
        printf("lilypad: installing %s on %s...\n", target->name, h->nick);
        if (target->offer_install(h) != 0) {
            fprintf(stderr, "lilypad: %s install failed\n", target->name);
            any_failed = 1;
            continue;
        }
    }
    printf("lilypad: re-probing %s...\n", h->nick);
    char markers[8] = "";
    probe_host(h->host, h->jump, markers, sizeof(markers));
    state_set_markers(h->nick, markers);
    if (any_failed) {
        fprintf(stderr, "lilypad: press Enter to continue, or Ctrl-C to abort...");
        fflush(stderr);
        int c = getchar();
        while (c != '\n' && c != EOF) c = getchar();
    }
}

static void emit_subcommands(void) {
    /* Underscore-prefixed subcommands are hidden from completion. */
    static const char *const cmds[] = { "add", "config", "update", "uninstall", NULL };
    for (int i = 0; cmds[i]; i++) printf("%s\n", cmds[i]);
}

static void emit_nicks(void) {
    char path[MAX_PATH];
    get_hosts_path(path, sizeof(path));
    Host hosts[MAX_HOSTS];
    int n = load_hosts(path, hosts, MAX_HOSTS);
    for (int i = 0; i < n; i++) printf("%s\n", hosts[i].nick);
}

static void emit_sessions(const char *nick) {
    HostState *st = state_get(nick);
    if (!st) return;
    for (int i = 0; i < st->session_count; i++) printf("%s\n", st->sessions[i]);
}

static void emit_config_keys(void) {
    for (int i = 0; ; i++) {
        const char *k = config_known_key(i);
        if (!k) break;
        printf("%s\n", k);
    }
}

static int is_known_nick(const char *nick) {
    char path[MAX_PATH];
    get_hosts_path(path, sizeof(path));
    Host hosts[MAX_HOSTS];
    int n = load_hosts(path, hosts, MAX_HOSTS);
    for (int i = 0; i < n; i++)
        if (strcmp(hosts[i].nick, nick) == 0) return 1;
    return 0;
}

static int cmd_complete(int argc, char *argv[]) {
    /* argv[0]=jump, argv[1]=_complete, argv[2..] are the slice of input
       words after the binary name (including the partial under cursor). */
    int n = argc - 2;
    if (n <= 0) {
        emit_subcommands();
        emit_nicks();
        return 0;
    }
    const char *first = argv[2];
    if (n == 1) {
        emit_subcommands();
        emit_nicks();
        return 0;
    }
    /* n >= 2 */
    if (strcmp(first, "config") == 0) {
        if (n == 2) {
            printf("get\nset\nunset\nundecline\n");
            return 0;
        }
        const char *op = argv[3];
        if (strcmp(op, "get") == 0 || strcmp(op, "set") == 0 || strcmp(op, "unset") == 0) {
            if (n == 3) emit_config_keys();
            return 0;
        }
        if (strcmp(op, "undecline") == 0) {
            if (n == 3) emit_nicks();
            else if (n == 4) printf("t\np\n");
            return 0;
        }
        return 0;
    }
    if (strcmp(first, "-s") == 0) {
        if (n == 2) emit_nicks();
        return 0;
    }
    if (strcmp(first, "add") == 0) {
        return 0; /* addresses, nicks and jump hosts are user-typed. */
    }
    /* first is a nick? complete sessions for it. */
    if (is_known_nick(first) && n == 2) {
        emit_sessions(first);
        return 0;
    }
    return 0;
}

static int self_path(char *out, size_t outsize) {
#ifdef __linux__
    ssize_t n = readlink("/proc/self/exe", out, outsize - 1);
    if (n > 0) { out[(size_t)n] = '\0'; return 0; }
#elif defined(__APPLE__)
    uint32_t sz = (uint32_t)outsize;
    if (_NSGetExecutablePath(out, &sz) == 0) return 0;
#endif
    return -1;
}

#define LILYPAD_REPO "https://github.com/spuddydev/lilypad"

static int latest_remote_tag(char *out, size_t outsize) {
    FILE *p = popen(
        "git ls-remote --tags --refs " LILYPAD_REPO " 2>/dev/null"
        " | awk -F/ '{print $NF}'"
        " | grep -E '^v[0-9]+\\.[0-9]+\\.[0-9]+$'"
        " | sort -V | tail -1",
        "r");
    if (!p) return -1;
    out[0] = '\0';
    if (fgets(out, (int)outsize, p)) {
        size_t n = strlen(out);
        while (n > 0 && (out[n-1] == '\n' || out[n-1] == '\r')) out[--n] = '\0';
    }
    int rc = pclose(p);
    if (rc != 0 || out[0] == '\0') return -1;
    return 0;
}

static int cmp_semver(const char *a, const char *b) {
    int aM = 0, am = 0, ap = 0, bM = 0, bm = 0, bp = 0;
    if (*a == 'v') a++;
    if (*b == 'v') b++;
    sscanf(a, "%d.%d.%d", &aM, &am, &ap);
    sscanf(b, "%d.%d.%d", &bM, &bm, &bp);
    if (aM != bM) return aM - bM;
    if (am != bm) return am - bm;
    return ap - bp;
}

static int cmd_update(int argc, char *argv[]) {
    int check_only = (argc >= 3 && strcmp(argv[2], "--check") == 0);

    char latest[64];
    if (latest_remote_tag(latest, sizeof(latest)) != 0) {
        fprintf(stderr, "lilypad: could not fetch latest tag from %s\n", LILYPAD_REPO);
        fprintf(stderr, "  check your network and that git is installed\n");
        return 1;
    }

    int cmp = cmp_semver(LILYPAD_VERSION, latest);
    if (cmp >= 0) {
        printf("lilypad: already on %s (latest is %s)\n", LILYPAD_VERSION, latest);
        return 0;
    }
    printf("lilypad: %s available (current %s)\n", latest, LILYPAD_VERSION);

    if (check_only) return 0;

    printf("running install.sh from %s ...\n", LILYPAD_REPO);
    int rc = system(
        "curl -fsSL https://raw.githubusercontent.com/spuddydev/lilypad/main/install.sh | bash");
    return WIFEXITED(rc) ? WEXITSTATUS(rc) : 1;
}

static int try_remove(const char *path) {
    if (access(path, F_OK) != 0) return 0;
    if (unlink(path) == 0) {
        printf("removed %s\n", path);
        return 1;
    }
    fprintf(stderr, "skip %s (%s)\n", path, strerror(errno));
    return 0;
}

static int rm_rf(const char *path) {
    char cmd[MAX_PATH + 16];
    snprintf(cmd, sizeof(cmd), "rm -rf -- '%s'", path);
    int rc = system(cmd);
    return WIFEXITED(rc) ? WEXITSTATUS(rc) : 1;
}

static int cmd_uninstall(int argc, char *argv[]) {
    (void)argc; (void)argv;
    char self[MAX_PATH] = "";
    if (self_path(self, sizeof(self)) != 0 || self[0] == '\0') {
        fprintf(stderr, "lilypad: could not determine the binary's own path\n");
        return 1;
    }

    printf("lilypad will remove:\n");
    printf("  binary:      %s\n", self);
    printf("  completions: bash and zsh, in standard system and user paths\n");
    printf("  config:      ~/.config/lilypad (with confirmation)\n");
    if (!prompt_yn("Continue?")) {
        printf("aborted\n");
        return 0;
    }

    int removed = 0;
    if (access(self, F_OK) == 0) {
        if (unlink(self) == 0) {
            printf("removed %s\n", self);
            removed++;
        } else {
            fprintf(stderr, "could not remove %s (%s)\n", self, strerror(errno));
            fprintf(stderr, "  try: sudo rm %s\n", self);
        }
    }

    static const char *const completion_paths[] = {
        "/usr/local/etc/bash_completion.d/jump",
        "/etc/bash_completion.d/jump",
        "/opt/homebrew/etc/bash_completion.d/jump",
        "/usr/local/share/zsh/site-functions/_jump",
        "/usr/share/zsh/site-functions/_jump",
        "/opt/homebrew/share/zsh/site-functions/_jump",
        NULL,
    };
    for (int i = 0; completion_paths[i]; i++)
        removed += try_remove(completion_paths[i]);

    const char *xdg_data = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    char p[MAX_PATH];
    if (xdg_data && *xdg_data) {
        snprintf(p, sizeof(p), "%s/bash-completion/completions/jump", xdg_data);
        removed += try_remove(p);
        snprintf(p, sizeof(p), "%s/zsh/site-functions/_jump", xdg_data);
        removed += try_remove(p);
    } else if (home && *home) {
        snprintf(p, sizeof(p), "%s/.local/share/bash-completion/completions/jump", home);
        removed += try_remove(p);
        snprintf(p, sizeof(p), "%s/.local/share/zsh/site-functions/_jump", home);
        removed += try_remove(p);
    }

    char config_dir[MAX_PATH] = "";
    const char *xdg_cfg = getenv("XDG_CONFIG_HOME");
    if (xdg_cfg && *xdg_cfg)
        snprintf(config_dir, sizeof(config_dir), "%s/lilypad", xdg_cfg);
    else if (home && *home)
        snprintf(config_dir, sizeof(config_dir), "%s/.config/lilypad", home);

    if (config_dir[0] && access(config_dir, F_OK) == 0) {
        char msg[MAX_PATH + 64];
        snprintf(msg, sizeof(msg), "Remove %s and all saved hosts?", config_dir);
        if (prompt_yn(msg)) {
            if (rm_rf(config_dir) == 0) {
                printf("removed %s\n", config_dir);
                removed++;
            }
        } else {
            printf("kept %s\n", config_dir);
        }
    }

    printf("\nlilypad: removed %d item(s).\n", removed);
    return 0;
}

static void print_usage(const char *prog) {
    printf(
        "Usage:\n"
        "  %s                              Open the menu\n"
        "  %s <nick>                       Open the host page\n"
        "  %s <nick> <session> [--plain]   Attach or create a tmux session\n"
        "  %s -s <nick>                    Plain ssh, no tmux\n"
        "  %s add user@addr nick [jump]    Save a host\n"
        "  %s config                       Print the current config\n"
        "  %s config get|set|unset <key>   Edit one config value\n"
        "  %s config undecline <nick> <l>  Re-enable an integration prompt\n"
        "  %s update [--check]             Update lilypad to the latest tag\n"
        "  %s uninstall                    Remove lilypad from this machine\n"
        "  %s --help, -h                   Show this help\n"
        "  %s --version                    Show version\n"
        "\n"
        "Hosts file: ~/.config/lilypad/hosts\n"
        "Docs:       https://github.com/spuddydev/lilypad\n",
        prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

int cli_dispatch(int argc, char *argv[]) {
    if (argc <= 1) return cmd_menu();

    const char *a = argv[1];

    if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    if (strcmp(a, "--version") == 0) {
        printf("lilypad %s\n", LILYPAD_VERSION);
        return 0;
    }

    if (strcmp(a, "add") == 0)            return cmd_add(argc, argv);
    if (strcmp(a, "config") == 0)         return cmd_config(argc, argv);
    if (strcmp(a, "update") == 0)         return cmd_update(argc, argv);
    if (strcmp(a, "uninstall") == 0)      return cmd_uninstall(argc, argv);
    if (strcmp(a, "_complete") == 0)      return cmd_complete(argc, argv);
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
