#include "config.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_CONFIG_ENTRIES 32
#define CONFIG_KEY_MAX     64
#define CONFIG_VAL_MAX     128

typedef struct {
    char key[CONFIG_KEY_MAX];
    char value[CONFIG_VAL_MAX];
    int  set;
} ConfigEntry;

static const char *const known_keys[] = {
    "integration.iterm",
    "integration.tmuxp",
    "probe.timeout",
    "probe.max_parallel",
    NULL,
};

static ConfigEntry entries[MAX_CONFIG_ENTRIES];
static int entry_count = 0;

void get_config_path(char *out, size_t size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdg && *xdg)        snprintf(out, size, "%s/lilypad/config", xdg);
    else if (home && *home) snprintf(out, size, "%s/.config/lilypad/config", home);
    else                    snprintf(out, size, "config");
}

static ConfigEntry *find_entry(const char *key) {
    for (int i = 0; i < entry_count; i++)
        if (strcmp(entries[i].key, key) == 0) return &entries[i];
    return NULL;
}

static void trim(char *s) {
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' || s[n-1] == '\r'))
        s[--n] = '\0';
}

void config_load(void) {
    entry_count = 0;
    char path[MAX_PATH];
    get_config_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[CONFIG_KEY_MAX + CONFIG_VAL_MAX + 8];
    while (fgets(line, sizeof(line), f)) {
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *k = line;
        char *v = eq + 1;
        trim(k);
        trim(v);
        if (!*k) continue;
        if (entry_count >= MAX_CONFIG_ENTRIES) break;
        ConfigEntry *e = &entries[entry_count++];
        snprintf(e->key, sizeof(e->key), "%s", k);
        snprintf(e->value, sizeof(e->value), "%s", v);
        e->set = 1;
    }
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

int config_save(void) {
    char path[MAX_PATH];
    get_config_path(path, sizeof(path));
    ensure_parent(path);
    char tmp[MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    fputs("# lilypad config\n", f);
    for (int i = 0; i < entry_count; i++)
        if (entries[i].set)
            fprintf(f, "%s = %s\n", entries[i].key, entries[i].value);
    fclose(f);
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

const char *config_get_str(const char *key, const char *defv) {
    ConfigEntry *e = find_entry(key);
    return (e && e->set) ? e->value : defv;
}

int config_get_int(const char *key, int defv) {
    const char *s = config_get_str(key, NULL);
    if (!s) return defv;
    char *end;
    long n = strtol(s, &end, 10);
    if (end == s || *end != '\0') return defv;
    return (int)n;
}

int config_get_bool(const char *key, int defv) {
    const char *s = config_get_str(key, NULL);
    if (!s) return defv;
    if (!strcasecmp(s, "on") || !strcasecmp(s, "true") ||
        !strcasecmp(s, "1") || !strcasecmp(s, "yes")) return 1;
    if (!strcasecmp(s, "off") || !strcasecmp(s, "false") ||
        !strcasecmp(s, "0") || !strcasecmp(s, "no")) return 0;
    return defv;
}

int config_set_str(const char *key, const char *value) {
    ConfigEntry *e = find_entry(key);
    if (!e) {
        if (entry_count >= MAX_CONFIG_ENTRIES) return -1;
        e = &entries[entry_count++];
    }
    snprintf(e->key, sizeof(e->key), "%s", key);
    snprintf(e->value, sizeof(e->value), "%s", value);
    e->set = 1;
    return 0;
}

int config_unset(const char *key) {
    for (int i = 0; i < entry_count; i++) {
        if (strcmp(entries[i].key, key) == 0) {
            entries[i] = entries[entry_count - 1];
            entry_count--;
            return 0;
        }
    }
    return -1;
}

const char *config_known_key(int index) {
    int n = (int)(sizeof(known_keys) / sizeof(known_keys[0])) - 1;
    if (index < 0 || index >= n) return NULL;
    return known_keys[index];
}

void config_print(void) {
    printf("# lilypad config\n");
    for (int i = 0; known_keys[i]; i++) {
        const char *v = config_get_str(known_keys[i], NULL);
        if (v) printf("%s = %s\n", known_keys[i], v);
        else   printf("# %s = (default)\n", known_keys[i]);
    }
}
