#include "state.h"
#include "hosts.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define STATE_MAX_HOSTS MAX_HOSTS

static HostState entries[STATE_MAX_HOSTS];
static int entry_count = 0;
static int batching = 0;
static int loaded = 0;

static void get_state_path(char *out, size_t size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdg && *xdg)        snprintf(out, size, "%s/lilypad/state", xdg);
    else if (home && *home) snprintf(out, size, "%s/.config/lilypad/state", home);
    else                    snprintf(out, size, "state");
}

void state_encode(char *dst, size_t dstsz, const char *src) {
    size_t i = 0;
    for (const char *p = src; *p; p++) {
        unsigned char c = (unsigned char)*p;
        int needs = (c == ' ' || c == ',' || c == '=' || c == '#' || c == '%' ||
                     c == '\t' || c == '\n' || c == '\r');
        if (needs) {
            if (i + 4 >= dstsz) break;
            i += (size_t)snprintf(dst + i, dstsz - i, "%%%02X", c);
        } else {
            if (i + 1 >= dstsz) break;
            dst[i++] = (char)c;
        }
    }
    dst[i < dstsz ? i : dstsz - 1] = '\0';
}

static int hex(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void state_decode(char *dst, size_t dstsz, const char *src) {
    size_t i = 0;
    for (const char *p = src; *p && i + 1 < dstsz; p++) {
        if (*p == '%' && p[1] && p[2]) {
            int hi = hex((unsigned char)p[1]);
            int lo = hex((unsigned char)p[2]);
            if (hi >= 0 && lo >= 0) {
                dst[i++] = (char)((hi << 4) | lo);
                p += 2;
                continue;
            }
        }
        dst[i++] = *p;
    }
    dst[i] = '\0';
}

HostState *state_get(const char *nick) {
    for (int i = 0; i < entry_count; i++)
        if (entries[i].set && strcmp(entries[i].nick, nick) == 0)
            return &entries[i];
    return NULL;
}

static HostState *get_or_create(const char *nick) {
    HostState *e = state_get(nick);
    if (e) return e;
    if (entry_count >= STATE_MAX_HOSTS) return NULL;
    e = &entries[entry_count++];
    memset(e, 0, sizeof(*e));
    snprintf(e->nick, sizeof(e->nick), "%s", nick);
    e->set = 1;
    return e;
}

int state_file_exists(void) {
    char path[MAX_PATH];
    get_state_path(path, sizeof(path));
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void parse_line(char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (!*line || *line == '#') return;
    char *space = strchr(line, ' ');
    if (!space) return;
    *space = '\0';
    HostState *e = get_or_create(line);
    if (!e) return;
    char *p = space + 1;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        char *eq = strchr(p, '=');
        if (!eq) break;
        *eq = '\0';
        char *key = p;
        char *val = eq + 1;
        char *end = val;
        while (*end && *end != ' ' && *end != '\t' && *end != '\n' && *end != '\r') end++;
        char saved = *end;
        *end = '\0';
        char decoded[1024];
        state_decode(decoded, sizeof(decoded), val);

        if (strcmp(key, "markers") == 0) {
            snprintf(e->markers, sizeof(e->markers), "%s", decoded);
        } else if (strcmp(key, "last_probe") == 0) {
            e->last_probe = (time_t)atoll(decoded);
        } else if (strcmp(key, "declined") == 0) {
            snprintf(e->declined, sizeof(e->declined), "%s", decoded);
        } else if (strcmp(key, "sessions") == 0) {
            e->session_count = 0;
            char *save = NULL;
            for (char *tok = strtok_r(decoded, ",", &save);
                 tok && e->session_count < STATE_MAX_SESSIONS;
                 tok = strtok_r(NULL, ",", &save)) {
                snprintf(e->sessions[e->session_count],
                         sizeof(e->sessions[e->session_count]), "%s", tok);
                e->session_count++;
            }
        }

        *end = saved;
        p = end;
    }
}

void state_load(void) {
    entry_count = 0;
    loaded = 1;
    char path[MAX_PATH];
    get_state_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[2048];
    while (fgets(line, sizeof(line), f))
        parse_line(line);
    fclose(f);
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

int state_save(void) {
    char path[MAX_PATH];
    get_state_path(path, sizeof(path));
    ensure_parent(path);
    char tmp[MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    fputs("# lilypad state\n", f);
    for (int i = 0; i < entry_count; i++) {
        const HostState *e = &entries[i];
        if (!e->set) continue;
        fprintf(f, "%s", e->nick);
        if (e->markers[0]) {
            char enc[32];
            state_encode(enc, sizeof(enc), e->markers);
            fprintf(f, " markers=%s", enc);
        }
        if (e->last_probe) fprintf(f, " last_probe=%lld", (long long)e->last_probe);
        if (e->session_count > 0) {
            char joined[2048] = "";
            size_t pos = 0;
            for (int j = 0; j < e->session_count && pos + 1 < sizeof(joined); j++) {
                if (j > 0 && pos + 1 < sizeof(joined)) joined[pos++] = ',';
                char enc[256];
                state_encode(enc, sizeof(enc), e->sessions[j]);
                pos += (size_t)snprintf(joined + pos, sizeof(joined) - pos, "%s", enc);
            }
            joined[pos < sizeof(joined) ? pos : sizeof(joined) - 1] = '\0';
            fprintf(f, " sessions=%s", joined);
        }
        if (e->declined[0]) {
            char enc[16];
            state_encode(enc, sizeof(enc), e->declined);
            fprintf(f, " declined=%s", enc);
        }
        fputc('\n', f);
    }
    fclose(f);
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

static int maybe_save(void) {
    return batching ? 0 : state_save();
}

int state_put(const HostState *st) {
    HostState *e = get_or_create(st->nick);
    if (!e) return -1;
    /* Preserve nick already set */
    char nick[MAX_LINE];
    snprintf(nick, sizeof(nick), "%s", e->nick);
    *e = *st;
    snprintf(e->nick, sizeof(e->nick), "%s", nick);
    e->set = 1;
    return maybe_save();
}

int state_set_markers(const char *nick, const char *markers) {
    HostState *e = get_or_create(nick);
    if (!e) return -1;
    snprintf(e->markers, sizeof(e->markers), "%s", markers ? markers : "");
    e->last_probe = time(NULL);
    return maybe_save();
}

int state_set_sessions(const char *nick, char sessions[][128], int n) {
    HostState *e = get_or_create(nick);
    if (!e) return -1;
    if (n > STATE_MAX_SESSIONS) n = STATE_MAX_SESSIONS;
    for (int i = 0; i < n; i++)
        snprintf(e->sessions[i], sizeof(e->sessions[i]), "%s", sessions[i]);
    e->session_count = n;
    return maybe_save();
}

int state_add_decline(const char *nick, char letter) {
    HostState *e = get_or_create(nick);
    if (!e) return -1;
    if (strchr(e->declined, letter)) return 0;
    size_t n = strlen(e->declined);
    if (n + 1 >= sizeof(e->declined)) return -1;
    e->declined[n] = letter;
    e->declined[n + 1] = '\0';
    return maybe_save();
}

int state_remove_decline(const char *nick, char letter) {
    HostState *e = state_get(nick);
    if (!e) return -1;
    char *pos = strchr(e->declined, letter);
    if (!pos) return -1;
    memmove(pos, pos + 1, strlen(pos + 1) + 1);
    return maybe_save();
}

void state_begin_batch(void) {
    batching = 1;
}

int state_commit_batch(void) {
    batching = 0;
    return state_save();
}

void state_abort_batch(void) {
    batching = 0;
}

void migrate_hosts_to_state(void) {
    if (state_file_exists()) return;

    char hosts_path[MAX_PATH];
    get_hosts_path(hosts_path, sizeof(hosts_path));
    FILE *f = fopen(hosts_path, "r");
    if (!f) return;

    int found_marker = 0;
    Host hosts[MAX_HOSTS];
    int count = load_hosts(hosts_path, hosts, MAX_HOSTS);
    fclose(f);

    for (int i = 0; i < count; i++) {
        if (hosts[i].markers[0]) { found_marker = 1; break; }
    }
    if (!found_marker) return;

    state_begin_batch();
    for (int i = 0; i < count; i++) {
        if (hosts[i].markers[0])
            state_set_markers(hosts[i].nick, hosts[i].markers);
    }
    state_commit_batch();

    for (int i = 0; i < count; i++)
        hosts[i].markers[0] = '\0';
    save_hosts(hosts_path, hosts, count);

    fprintf(stderr,
            "lilypad: migrated host markers into %s/lilypad/state\n",
            getenv("XDG_CONFIG_HOME") ? getenv("XDG_CONFIG_HOME")
                                      : (getenv("HOME") ? "$HOME/.config" : "."));
    (void)loaded;
}
