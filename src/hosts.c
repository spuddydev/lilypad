#include "hosts.h"
#include "config.h"
#include "integration.h"
#include "state.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

void get_hosts_path(char *out, size_t size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdg && *xdg)
        snprintf(out, size, "%s/lilypad/hosts", xdg);
    else if (home && *home)
        snprintf(out, size, "%s/.config/lilypad/hosts", home);
    else
        snprintf(out, size, "hosts");
}

static int config_base(char *out, size_t size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdg && *xdg) {
        snprintf(out, size, "%s", xdg);
        return 1;
    }
    if (home && *home) {
        snprintf(out, size, "%s/.config", home);
        return 1;
    }
    return 0;
}

void migrate_legacy_config(void) {
    char base[MAX_PATH];
    if (!config_base(base, sizeof(base))) return;

    char old_dir[MAX_PATH], new_dir[MAX_PATH];
    snprintf(old_dir, sizeof(old_dir), "%s/ssh-menu", base);
    snprintf(new_dir, sizeof(new_dir), "%s/lilypad", base);

    struct stat st_old, st_new;
    int has_old = (stat(old_dir, &st_old) == 0 && S_ISDIR(st_old.st_mode));
    int has_new = (stat(new_dir, &st_new) == 0 && S_ISDIR(st_new.st_mode));

    if (!has_old) return;
    if (has_new) {
        fprintf(stderr,
                "lilypad: legacy config at %s remains; %s already exists; not merging\n",
                old_dir, new_dir);
        return;
    }
    if (rename(old_dir, new_dir) != 0) {
        if (errno == EXDEV) {
            fprintf(stderr,
                    "lilypad: cannot move %s to %s across filesystems.\n"
                    "  please move it manually: mv %s %s\n",
                    old_dir, new_dir, old_dir, new_dir);
            exit(1);
        }
        fprintf(stderr, "lilypad: migration failed: %s\n", strerror(errno));
        return;
    }
    fprintf(stderr, "lilypad: migrated config from %s to %s\n", old_dir, new_dir);
}

static void mkdir_p(const char *path) {
    char tmp[MAX_PATH];
    size_t n = strlen(path);
    if (n >= sizeof(tmp)) return;
    memcpy(tmp, path, n + 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0700);
            *p = '/';
        }
    }
    mkdir(tmp, 0700);
}

static void ensure_parent(const char *path) {
    char tmp[MAX_PATH];
    size_t n = strlen(path);
    if (n >= sizeof(tmp)) return;
    memcpy(tmp, path, n + 1);
    char *slash = strrchr(tmp, '/');
    if (!slash || slash == tmp) return;
    *slash = '\0';
    mkdir_p(tmp);
}

/* Parse an in-place, whitespace-delimited hosts line.
 * Format: "nick host [jump] [#markers]"
 * Writes pointers into `line` (which is modified). Returns 1 on success. */
static int parse_line(char *line, char **nick, char **host, char **jump, char **markers) {
    *nick = *host = *jump = *markers = NULL;
    char *save = NULL;
    char *fields[4] = {0};
    int nf = 0;
    for (char *t = strtok_r(line, " \t\r\n", &save);
         t && nf < 4;
         t = strtok_r(NULL, " \t\r\n", &save))
        fields[nf++] = t;
    if (nf < 2) return 0;
    *nick = fields[0];
    *host = fields[1];
    if (nf >= 3 && fields[nf - 1][0] == '#') {
        *markers = fields[nf - 1] + 1;
        nf--;
    }
    if (nf >= 3) *jump = fields[2];
    return 1;
}

static void copy_field(char *dst, size_t dstsz, const char *src) {
    if (dstsz == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= dstsz) n = dstsz - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

int load_hosts(const char *path, Host *hosts, int max) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int count = 0;
    char line[MAX_LINE];
    while (count < max && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char *nick, *host, *jump, *markers;
        if (!parse_line(line, &nick, &host, &jump, &markers)) continue;
        Host *h = &hosts[count];
        copy_field(h->nick, sizeof(h->nick), nick);
        copy_field(h->host, sizeof(h->host), host);
        copy_field(h->jump, sizeof(h->jump), jump);
        copy_field(h->markers, sizeof(h->markers), markers);
        /* Overlay state markers on top of any legacy #markers token. */
        const HostState *st = state_get(h->nick);
        if (st && st->markers[0])
            copy_field(h->markers, sizeof(h->markers), st->markers);
        count++;
    }
    fclose(f);
    return count;
}

int is_reserved_nick(const char *nick) {
    if (!nick) return 1;
    if (nick[0] == '-') return 1;
    static const char *const reserved[] = { "add", "config", "_complete", NULL };
    for (int i = 0; reserved[i]; i++)
        if (strcmp(nick, reserved[i]) == 0) return 1;
    return 0;
}

int add_host(const char *path, const char *nick, const char *host, const char *jump) {
    if (!strchr(host, '@')) {
        fprintf(stderr, "Error: host must be in name@address format\n");
        return 1;
    }
    if (strpbrk(nick, " \t\r\n")) {
        fprintf(stderr, "Error: nickname must not contain whitespace\n");
        return 1;
    }
    if (is_reserved_nick(nick)) {
        fprintf(stderr, "Error: nickname '%s' is reserved\n", nick);
        return 1;
    }
    if (jump && strpbrk(jump, " \t\r\n")) {
        fprintf(stderr, "Error: jump hosts must not contain whitespace\n");
        return 1;
    }

    FILE *f = fopen(path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char existing[MAX_LINE];
            if (sscanf(line, "%511s", existing) == 1 && strcmp(existing, nick) == 0) {
                fclose(f);
                fprintf(stderr, "Error: nickname '%s' already exists\n", nick);
                return 1;
            }
        }
        fclose(f);
    }

    ensure_parent(path);
    f = fopen(path, "a");
    if (!f) { perror("Error opening hosts file"); return 1; }
    if (jump)
        fprintf(f, "%s %s %s\n", nick, host, jump);
    else
        fprintf(f, "%s %s\n", nick, host);
    fclose(f);
    return 0;
}

int update_markers(const char *path, const char *nick, const char *markers) {
    (void)path;
    return state_set_markers(nick, markers);
}

int save_hosts(const char *path, const Host *hosts, int count) {
    ensure_parent(path);
    char tmp_path[MAX_PATH];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    FILE *w = fopen(tmp_path, "w");
    if (!w) return -1;
    for (int i = 0; i < count; i++) {
        const Host *h = &hosts[i];
        if (h->jump[0]) fprintf(w, "%s %s %s\n", h->nick, h->host, h->jump);
        else            fprintf(w, "%s %s\n",    h->nick, h->host);
    }
    fclose(w);
    if (rename(tmp_path, path) != 0) { unlink(tmp_path); return -1; }
    return 0;
}

void shell_sq_escape(char *dst, size_t dstsz, const char *src) {
    size_t i = 0;
    for (const char *p = src; *p; p++) {
        if (*p == '\'') {
            if (i + 4 >= dstsz) break;
            dst[i++] = '\'';
            dst[i++] = '\\';
            dst[i++] = '\'';
            dst[i++] = '\'';
        } else {
            if (i + 1 >= dstsz) break;
            dst[i++] = *p;
        }
    }
    dst[i < dstsz ? i : dstsz - 1] = '\0';
}

int kill_session(const char *host, const char *jump, const char *session) {
    if (!session || !*session) return -1;

    char esc[256];
    shell_sq_escape(esc, sizeof(esc), session);
    char remote_cmd[512];
    snprintf(remote_cmd, sizeof(remote_cmd),
             REMOTE_PATH_PREFIX "tmux kill-session -t '%s'", esc);

    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        const char *args[16];
        int a = 0;
        args[a++] = "ssh";
        args[a++] = "-o"; args[a++] = "BatchMode=yes";
        args[a++] = "-o"; args[a++] = "ConnectTimeout=5";
        if (jump && *jump) { args[a++] = "-J"; args[a++] = jump; }
        args[a++] = host;
        args[a++] = remote_cmd;
        args[a++] = NULL;
        execvp("ssh", (char *const *)args);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

int rename_session(const char *host, const char *jump,
                   const char *old_name, const char *new_name) {
    if (!old_name || !*old_name || !new_name || !*new_name) return -1;

    char esc_old[256], esc_new[256];
    shell_sq_escape(esc_old, sizeof(esc_old), old_name);
    shell_sq_escape(esc_new, sizeof(esc_new), new_name);
    char remote_cmd[512];
    snprintf(remote_cmd, sizeof(remote_cmd),
             REMOTE_PATH_PREFIX "tmux rename-session -t '%s' '%s'",
             esc_old, esc_new);

    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        const char *args[16];
        int a = 0;
        args[a++] = "ssh";
        args[a++] = "-o"; args[a++] = "BatchMode=yes";
        args[a++] = "-o"; args[a++] = "ConnectTimeout=5";
        if (jump && *jump) { args[a++] = "-J"; args[a++] = jump; }
        args[a++] = host;
        args[a++] = remote_cmd;
        args[a++] = NULL;
        execvp("ssh", (char *const *)args);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

/* Build the combined remote shell snippet from enabled remote integrations.
   Each fragment contributes `<test> && echo I_<letter>;`. A trailing `true`
   ensures exit 0 even when nothing matches. */
static void build_probe_cmd(char *out, size_t size) {
    out[0] = '\0';
    size_t pos = 0;
    pos += (size_t)snprintf(out + pos, size - pos, "%s", REMOTE_PATH_PREFIX);
    for (int i = 0; ; i++) {
        const Integration *it = integration_at(i);
        if (!it) break;
        if (it->kind != KIND_REMOTE || !it->enabled || !it->probe_remote_cmd) continue;
        pos += (size_t)snprintf(out + pos, size - pos, "%s; ", it->probe_remote_cmd);
        if (pos >= size) return;
    }
    snprintf(out + pos, size - pos, "true");
}

int probe_host(const char *host, const char *jump, char *out, size_t outsize) {
    if (outsize == 0) return -1;
    out[0] = '\0';

    char remote_cmd[1024];
    build_probe_cmd(remote_cmd, sizeof(remote_cmd));

    char timeout_str[16];
    snprintf(timeout_str, sizeof(timeout_str),
             "ConnectTimeout=%d", config_get_int("probe.timeout", 5));

    int pipefd[2];
    if (pipe(pipefd) != 0) return -1;

    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return -1; }
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        const char *args[16];
        int a = 0;
        args[a++] = "ssh";
        args[a++] = "-o"; args[a++] = "BatchMode=yes";
        args[a++] = "-o"; args[a++] = timeout_str;
        args[a++] = "-o"; args[a++] = "StrictHostKeyChecking=accept-new";
        if (jump && *jump) { args[a++] = "-J"; args[a++] = jump; }
        args[a++] = host;
        args[a++] = remote_cmd;
        args[a++] = NULL;
        execvp("ssh", (char *const *)args);
        _exit(127);
    }

    close(pipefd[1]);
    char buf[512];
    ssize_t total = 0, n;
    while (total < (ssize_t)sizeof(buf) - 1 &&
           (n = read(pipefd[0], buf + total, sizeof(buf) - 1 - total)) > 0)
        total += n;
    buf[total > 0 ? total : 0] = '\0';
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);
    int ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;

    if (!ok) {
        if (outsize >= 2) { out[0] = '?'; out[1] = '\0'; }
        return 0;
    }

    size_t pos = 0;
    for (int i = 0; ; i++) {
        const Integration *it = integration_at(i);
        if (!it) break;
        if (it->kind != KIND_REMOTE || !it->enabled || !it->marker_letter) continue;
        char token[8];
        snprintf(token, sizeof(token), "I_%c", it->marker_letter);
        if (!strstr(buf, token) && pos + 1 < outsize)
            out[pos++] = it->marker_letter;
    }
    out[pos] = '\0';
    return 0;
}

int fetch_sessions(const char *host, const char *jump, char sessions[][128], int max) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return -1;

    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return -1; }
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        const char *args[16];
        int a = 0;
        args[a++] = "ssh";
        args[a++] = "-o"; args[a++] = "BatchMode=yes";
        args[a++] = "-o"; args[a++] = "ConnectTimeout=5";
        if (jump && *jump) { args[a++] = "-J"; args[a++] = jump; }
        args[a++] = host;
        args[a++] = REMOTE_PATH_PREFIX "tmux ls 2>/dev/null; true";
        args[a++] = NULL;
        execvp("ssh", (char *const *)args);
        _exit(127);
    }

    close(pipefd[1]);
    char buf[4096];
    ssize_t total = 0, n;
    while (total < (ssize_t)sizeof(buf) - 1 &&
           (n = read(pipefd[0], buf + total, sizeof(buf) - 1 - total)) > 0)
        total += n;
    buf[total > 0 ? total : 0] = '\0';
    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return -1;

    int count = 0;
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save);
         line && count < max;
         line = strtok_r(NULL, "\n", &save)) {
        char *colon = strchr(line, ':');
        size_t len = colon ? (size_t)(colon - line) : strlen(line);
        if (len == 0 || len >= 128) continue;
        memcpy(sessions[count], line, len);
        sessions[count][len] = '\0';
        count++;
    }
    return count;
}

static const char DEV_SPLIT_YAML[] =
    "session_name: dev\n"
    "windows:\n"
    "  - window_name: dev\n"
    "    layout: even-horizontal\n"
    "    panes:\n"
    "      - null\n"
    "      - null\n";

void get_templates_dir(char *out, size_t size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdg && *xdg)
        snprintf(out, size, "%s/lilypad/tmuxp", xdg);
    else if (home && *home)
        snprintf(out, size, "%s/.config/lilypad/tmuxp", home);
    else
        snprintf(out, size, "tmuxp");
}

static int has_yaml_ext(const char *name) {
    size_t n = strlen(name);
    if (n >= 5 && strcmp(name + n - 5, ".yaml") == 0) return 5;
    if (n >= 4 && strcmp(name + n - 4, ".yml") == 0) return 4;
    return 0;
}

static int is_safe_basename(const char *s, size_t len) {
    if (len == 0) return 0;
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'))
            return 0;
    }
    return 1;
}

int list_templates(char names[][128], int max) {
    char dir[MAX_PATH];
    get_templates_dir(dir, sizeof(dir));
    DIR *d = opendir(dir);
    if (!d) return 0;
    int count = 0;
    struct dirent *e;
    while ((e = readdir(d)) && count < max) {
        if (e->d_name[0] == '.') continue;
        int ext = has_yaml_ext(e->d_name);
        if (!ext) continue;
        size_t base_len = strlen(e->d_name) - ext;
        if (base_len >= 128) base_len = 127;
        if (!is_safe_basename(e->d_name, base_len)) continue;
        memcpy(names[count], e->d_name, base_len);
        names[count][base_len] = '\0';
        count++;
    }
    closedir(d);
    return count;
}

int install_default_templates(void) {
    char dir[MAX_PATH];
    get_templates_dir(dir, sizeof(dir));
    char names[1][128];
    if (list_templates(names, 1) > 0) return 0;
    mkdir_p(dir);
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/dev-split.yaml", dir);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fputs(DEV_SPLIT_YAML, f);
    fclose(f);
    return 1;
}

int install_template_from_path(const char *src) {
    if (!has_yaml_ext(src)) {
        fprintf(stderr, "Template must end in .yaml or .yml\n");
        return -1;
    }
    FILE *in = fopen(src, "r");
    if (!in) { perror(src); return -1; }
    char dir[MAX_PATH];
    get_templates_dir(dir, sizeof(dir));
    mkdir_p(dir);
    const char *base = strrchr(src, '/');
    base = base ? base + 1 : src;
    char dest[MAX_PATH];
    snprintf(dest, sizeof(dest), "%s/%s", dir, base);
    FILE *out = fopen(dest, "w");
    if (!out) { perror(dest); fclose(in); return -1; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { perror(dest); fclose(in); fclose(out); return -1; }
    fclose(in);
    fclose(out);
    fprintf(stderr, "Installed template at %s\n", dest);
    return 0;
}
