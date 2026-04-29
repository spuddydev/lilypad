#include "exec.h"
#include "hosts.h"
#include "state.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int is_iterm(void) {
    const char *tp = getenv("TERM_PROGRAM");
    return tp && strcmp(tp, "iTerm.app") == 0;
}

int host_has_tmux(const Host *h) {
    return h->markers[0] != '?' && strchr(h->markers, 't') == NULL;
}

int host_has_tmuxp(const Host *h) {
    return h->markers[0] != '?' && strchr(h->markers, 'p') == NULL;
}

/* Run a probe of `h` and rewrite the state file. Detached so the user's
   shell prompt comes back immediately after disconnect. */
static void spawn_background_reprobe(const Host *h) {
    fflush(stdout);
    fflush(stderr);
    pid_t bg = fork();
    if (bg < 0) return;
    if (bg == 0) {
        /* First child: setsid and double-fork so the grandchild is reparented
           to init/launchd and the parent doesn't have to wait on it. */
        setsid();
        pid_t bg2 = fork();
        if (bg2 != 0) _exit(0);

        /* Grandchild: detach stdio so nothing prints over the user's shell. */
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, 0);
            dup2(devnull, 1);
            dup2(devnull, 2);
            if (devnull > 2) close(devnull);
        }

        char markers[8] = "";
        probe_host(h->host, h->jump, markers, sizeof(markers));
        char sessions[16][128];
        int ns = 0;
        if (markers[0] != '?')
            ns = fetch_sessions(h->host, h->jump, sessions, 16);
        if (ns < 0) ns = 0;

        state_begin_batch();
        state_set_markers(h->nick, markers);
        if (ns > 0) state_set_sessions(h->nick, sessions, ns);
        state_commit_batch();
        _exit(0);
    }
    /* Reap the first child immediately; the grandchild is detached. */
    int status;
    waitpid(bg, &status, 0);
}

/* Fork ssh as a child, wait for it, then kick off the post-disconnect
   reprobe in the background. SIGINT and SIGQUIT are ignored in the
   parent so they reach ssh; the child resets them via the inherited
   default after fork. */
static int run_ssh_with_reprobe(const Host *h, const char *const *argv) {
    fflush(stdout);
    fflush(stderr);

    struct sigaction old_int, old_quit, ign = {0};
    ign.sa_handler = SIG_IGN;
    sigaction(SIGINT,  &ign, &old_int);
    sigaction(SIGQUIT, &ign, &old_quit);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        sigaction(SIGINT,  &old_int,  NULL);
        sigaction(SIGQUIT, &old_quit, NULL);
        return 1;
    }
    if (pid == 0) {
        /* Child: restore default signal handling so ssh can be Ctrl-C'd. */
        sigaction(SIGINT,  &old_int,  NULL);
        sigaction(SIGQUIT, &old_quit, NULL);
        execvp("ssh", (char *const *)argv);
        perror("ssh");
        _exit(127);
    }

    int status;
    while (waitpid(pid, &status, 0) == -1 && errno == EINTR) {}

    sigaction(SIGINT,  &old_int,  NULL);
    sigaction(SIGQUIT, &old_quit, NULL);

    spawn_background_reprobe(h);

    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

int exec_ssh_plain(const Host *h) {
    const char *argv[8];
    int a = 0;
    argv[a++] = "ssh";
    if (h->jump[0]) { argv[a++] = "-J"; argv[a++] = h->jump; }
    argv[a++] = h->host;
    argv[a++] = NULL;
    return run_ssh_with_reprobe(h, argv);
}

int exec_ssh_tmux(const Host *h, const char *remote_cmd) {
    char wrapped[1024];
    snprintf(wrapped, sizeof(wrapped), REMOTE_PATH_PREFIX "%s", remote_cmd);
    const char *argv[8];
    int a = 0;
    argv[a++] = "ssh";
    argv[a++] = "-t";
    if (h->jump[0]) { argv[a++] = "-J"; argv[a++] = h->jump; }
    argv[a++] = h->host;
    argv[a++] = wrapped;
    argv[a++] = NULL;
    return run_ssh_with_reprobe(h, argv);
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

static int parse_session_name(const char *yaml_path, char *out, size_t size) {
    FILE *f = fopen(yaml_path, "r");
    if (!f) return -1;
    char line[512];
    int found = -1;
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "session_name:", 13) != 0) continue;
        p += 13;
        while (*p == ' ' || *p == '\t') p++;
        char *e = p + strlen(p);
        while (e > p && (e[-1] == '\n' || e[-1] == '\r' ||
                         e[-1] == ' ' || e[-1] == '\t'))
            e--;
        *e = '\0';
        if ((*p == '"' || *p == '\'') && e > p + 1 && e[-1] == *p) {
            p++; *(e - 1) = '\0';
        }
        size_t n = strlen(p);
        if (n == 0 || n >= size) break;
        memcpy(out, p, n);
        out[n] = '\0';
        found = 0;
        break;
    }
    fclose(f);
    return found;
}

int exec_template(const Host *h, const char *template_name, int force_plain) {
    char dir[MAX_PATH], src[MAX_PATH];
    get_templates_dir(dir, sizeof(dir));
    snprintf(src, sizeof(src), "%s/%s.yaml", dir, template_name);
    if (access(src, R_OK) != 0)
        snprintf(src, sizeof(src), "%s/%s.yml", dir, template_name);
    if (access(src, R_OK) != 0) {
        fprintf(stderr, "Template not found: %s\n", template_name);
        return 1;
    }
    char session[128] = "";
    int have_session = parse_session_name(src, session, sizeof(session)) == 0;

    char remote_path[MAX_PATH];
    snprintf(remote_path, sizeof(remote_path), "/tmp/lilypad-%s.yaml", template_name);
    if (scp_template(h, src, remote_path) != 0) {
        fprintf(stderr, "scp failed\n");
        return 1;
    }

    const char *tmux_prefix = (is_iterm() && !force_plain) ? "tmux -CC" : "tmux";
    char remote_cmd[1024];
    char esc_path[MAX_PATH + 16];
    shell_sq_escape(esc_path, sizeof(esc_path), remote_path);
    if (have_session) {
        char esc_session[256];
        shell_sq_escape(esc_session, sizeof(esc_session), session);
        snprintf(remote_cmd, sizeof(remote_cmd),
                 "tmuxp load -d -y '%s'; rm -f '%s'; %s attach -t '%s'",
                 esc_path, esc_path, tmux_prefix, esc_session);
    } else {
        snprintf(remote_cmd, sizeof(remote_cmd),
                 "tmuxp load -y '%s'; rm -f '%s'", esc_path, esc_path);
    }
    return exec_ssh_tmux(h, remote_cmd);
}

void auto_session_name(char sessions[][128], int ns, char *out, size_t outsize) {
    for (int suffix = 0; suffix < 1000; suffix++) {
        char candidate[128];
        if (suffix == 0) snprintf(candidate, sizeof(candidate), "untitled");
        else             snprintf(candidate, sizeof(candidate), "untitled-%d", suffix);
        int taken = 0;
        for (int i = 0; i < ns; i++)
            if (strcmp(sessions[i], candidate) == 0) { taken = 1; break; }
        if (!taken) {
            size_t n = strlen(candidate);
            if (n >= outsize) n = outsize - 1;
            memcpy(out, candidate, n);
            out[n] = '\0';
            return;
        }
    }
    snprintf(out, outsize, "untitled");
}
