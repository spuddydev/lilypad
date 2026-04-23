#include "hosts.h"

#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <stdint.h>
#endif

static int get_exe_path(char *out, size_t size) {
#ifdef __APPLE__
    uint32_t len = (uint32_t)size;
    return _NSGetExecutablePath(out, &len) == 0 ? 0 : -1;
#else
    ssize_t n = readlink("/proc/self/exe", out, size - 1);
    if (n < 0) return -1;
    out[n] = '\0';
    return 0;
#endif
}

void get_hosts_path(char *out, size_t size) {
    char exe[MAX_PATH];
    if (get_exe_path(exe, sizeof(exe)) != 0) {
        snprintf(out, size, "hosts");
        return;
    }
    snprintf(out, size, "%s/hosts", dirname(exe));
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
        count++;
    }
    fclose(f);
    return count;
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
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char tmp_path[MAX_PATH];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    FILE *w = fopen(tmp_path, "w");
    if (!w) { fclose(f); return -1; }

    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char copy[MAX_LINE];
        copy_field(copy, sizeof(copy), line);
        copy[strcspn(copy, "\n")] = '\0';
        char *n, *h, *j, *m;
        if (parse_line(copy, &n, &h, &j, &m) && strcmp(n, nick) == 0) {
            found = 1;
            if (j && *j) {
                if (markers && *markers) fprintf(w, "%s %s %s #%s\n", n, h, j, markers);
                else                     fprintf(w, "%s %s %s\n", n, h, j);
            } else {
                if (markers && *markers) fprintf(w, "%s %s #%s\n", n, h, markers);
                else                     fprintf(w, "%s %s\n", n, h);
            }
        } else {
            fputs(line, w);
        }
    }
    fclose(f);
    fclose(w);

    if (!found) { unlink(tmp_path); return -1; }
    if (rename(tmp_path, path) != 0) { unlink(tmp_path); return -1; }
    return 0;
}
