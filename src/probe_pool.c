#include "probe_pool.h"
#include "config.h"
#include "integration.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    pid_t pid;
    int fd;
    char nick[MAX_LINE];
    char host[MAX_LINE];
    char jump[MAX_LINE];
    char buf[4096];
    size_t buf_len;
    int eof_seen;
} PoolSlot;

typedef struct {
    char nick[MAX_LINE];
    char host[MAX_LINE];
    char jump[MAX_LINE];
} QueueItem;

static PoolSlot slots[POOL_MAX_PARALLEL];
static int max_parallel = 8;

static QueueItem queue[MAX_HOSTS];
static int queue_len = 0;
static int queue_head = 0;

static const char REMOTE_SNIPPET[] =
    "PATH=$HOME/.local/bin:$HOME/bin:/opt/homebrew/bin:/usr/local/bin:$PATH; "
    "command -v tmux  >/dev/null && echo I_t; "
    "command -v tmuxp >/dev/null && echo I_p; "
    "if command -v tmux >/dev/null; then "
    "  echo SESSIONS_BEGIN; "
    "  tmux ls -F '#S' 2>/dev/null; "
    "  echo SESSIONS_END; "
    "fi; "
    "true";

void probe_pool_init(int mp) {
    max_parallel = mp;
    if (max_parallel < 1) max_parallel = 1;
    if (max_parallel > POOL_MAX_PARALLEL) max_parallel = POOL_MAX_PARALLEL;
    for (int i = 0; i < POOL_MAX_PARALLEL; i++) {
        slots[i].pid = 0;
        slots[i].fd = -1;
        slots[i].buf_len = 0;
        slots[i].eof_seen = 0;
    }
    queue_len = 0;
    queue_head = 0;
}

void probe_pool_enqueue(const char *nick, const char *host, const char *jump) {
    if (queue_len >= MAX_HOSTS) return;
    /* Skip duplicate nicks already pending or in-flight. */
    for (int i = queue_head; i < queue_len; i++)
        if (strcmp(queue[i].nick, nick) == 0) return;
    for (int i = 0; i < max_parallel; i++)
        if (slots[i].pid > 0 && strcmp(slots[i].nick, nick) == 0) return;

    QueueItem *q = &queue[queue_len++];
    snprintf(q->nick, sizeof(q->nick), "%s", nick);
    snprintf(q->host, sizeof(q->host), "%s", host);
    snprintf(q->jump, sizeof(q->jump), "%s", jump ? jump : "");
}

static int find_free_slot(void) {
    for (int i = 0; i < max_parallel; i++)
        if (slots[i].pid == 0) return i;
    return -1;
}

static void launch_slot(int idx, const QueueItem *q) {
    PoolSlot *s = &slots[idx];
    int pipefd[2];
    if (pipe(pipefd) != 0) return;

    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return; }
    if (pid == 0) {
        setpgid(0, 0);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        char timeout_str[32];
        snprintf(timeout_str, sizeof(timeout_str), "ConnectTimeout=%d",
                 config_get_int("probe.timeout", 5));
        const char *args[16];
        int a = 0;
        args[a++] = "ssh";
        args[a++] = "-o"; args[a++] = "BatchMode=yes";
        args[a++] = "-o"; args[a++] = timeout_str;
        args[a++] = "-o"; args[a++] = "StrictHostKeyChecking=accept-new";
        if (q->jump[0]) { args[a++] = "-J"; args[a++] = (char *)q->jump; }
        args[a++] = (char *)q->host;
        args[a++] = (char *)REMOTE_SNIPPET;
        args[a++] = NULL;
        execvp("ssh", (char *const *)args);
        _exit(127);
    }
    setpgid(pid, pid);
    close(pipefd[1]);
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    if (flags >= 0) fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    s->pid = pid;
    s->fd = pipefd[0];
    snprintf(s->nick, sizeof(s->nick), "%s", q->nick);
    snprintf(s->host, sizeof(s->host), "%s", q->host);
    snprintf(s->jump, sizeof(s->jump), "%s", q->jump);
    s->buf_len = 0;
    s->eof_seen = 0;
}

void probe_pool_pump(void) {
    while (queue_head < queue_len) {
        int idx = find_free_slot();
        if (idx < 0) return;
        launch_slot(idx, &queue[queue_head]);
        queue_head++;
    }
}

static void parse_result(const PoolSlot *s, ProbeResult *out) {
    snprintf(out->nick, sizeof(out->nick), "%s", s->nick);
    out->markers[0] = '\0';
    out->session_count = 0;

    /* No bytes or no recognisable tokens: treat as ssh failure. */
    int has_t = strstr(s->buf, "I_t") != NULL;
    int has_p = strstr(s->buf, "I_p") != NULL;
    int has_block = strstr(s->buf, "SESSIONS_BEGIN") != NULL;
    if (s->buf_len == 0 || (!has_t && !has_p && !has_block)) {
        out->markers[0] = '?';
        out->markers[1] = '\0';
        return;
    }

    size_t pos = 0;
    for (int i = 0; ; i++) {
        const Integration *it = integration_at(i);
        if (!it) break;
        if (it->kind != KIND_REMOTE || !it->enabled || !it->marker_letter) continue;
        char tok[8];
        snprintf(tok, sizeof(tok), "I_%c", it->marker_letter);
        if (!strstr(s->buf, tok) && pos + 1 < sizeof(out->markers))
            out->markers[pos++] = it->marker_letter;
    }
    out->markers[pos] = '\0';

    const char *begin = strstr(s->buf, "SESSIONS_BEGIN");
    const char *end   = strstr(s->buf, "SESSIONS_END");
    if (begin && end && end > begin) {
        const char *p = strchr(begin, '\n');
        if (!p || p >= end) return;
        p++;
        while (p < end && out->session_count < POOL_MAX_SESSIONS) {
            const char *nl = memchr(p, '\n', (size_t)(end - p));
            size_t len = nl ? (size_t)(nl - p) : 0;
            if (len > 0 && len < sizeof(out->sessions[0])) {
                memcpy(out->sessions[out->session_count], p, len);
                out->sessions[out->session_count][len] = '\0';
                out->session_count++;
            }
            if (!nl) break;
            p = nl + 1;
        }
    }
}

int probe_pool_drain_now(void (*cb)(const ProbeResult *, void *), void *user) {
    int updates = 0;
    for (int i = 0; i < max_parallel; i++) {
        PoolSlot *s = &slots[i];
        if (s->pid == 0) continue;
        if (!s->eof_seen) {
            while (s->buf_len + 1 < sizeof(s->buf)) {
                ssize_t n = read(s->fd, s->buf + s->buf_len,
                                 sizeof(s->buf) - 1 - s->buf_len);
                if (n > 0) {
                    s->buf_len += (size_t)n;
                } else if (n == 0) {
                    s->eof_seen = 1;
                    break;
                } else {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                    s->eof_seen = 1;
                    break;
                }
            }
        }
        if (!s->eof_seen) continue;

        int status;
        pid_t r = waitpid(s->pid, &status, WNOHANG);
        if (r == 0) continue;

        s->buf[s->buf_len < sizeof(s->buf) ? s->buf_len : sizeof(s->buf) - 1] = '\0';

        ProbeResult res;
        parse_result(s, &res);
        if (cb) cb(&res, user);

        close(s->fd);
        s->fd = -1;
        s->pid = 0;
        s->buf_len = 0;
        s->eof_seen = 0;
        updates++;
    }
    return updates;
}

void probe_pool_shutdown(void) {
    for (int i = 0; i < max_parallel; i++)
        if (slots[i].pid > 0) kill(-slots[i].pid, SIGTERM);

    for (int wait_ms = 0; wait_ms < 1000; wait_ms += 50) {
        int any = 0;
        for (int i = 0; i < max_parallel; i++) {
            if (slots[i].pid <= 0) continue;
            int status;
            pid_t r = waitpid(slots[i].pid, &status, WNOHANG);
            if (r > 0) {
                close(slots[i].fd);
                slots[i].fd = -1;
                slots[i].pid = 0;
            } else {
                any = 1;
            }
        }
        if (!any) goto done;
        usleep(50 * 1000);
    }
    for (int i = 0; i < max_parallel; i++) {
        if (slots[i].pid > 0) {
            kill(-slots[i].pid, SIGKILL);
            int status;
            waitpid(slots[i].pid, &status, 0);
            close(slots[i].fd);
            slots[i].fd = -1;
            slots[i].pid = 0;
        }
    }
done:
    queue_len = 0;
    queue_head = 0;
}

int probe_pool_active(void) {
    int n = 0;
    for (int i = 0; i < max_parallel; i++)
        if (slots[i].pid > 0) n++;
    return n;
}

int probe_pool_queued(void) {
    return queue_len - queue_head;
}
