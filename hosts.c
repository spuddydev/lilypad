#include "hosts.h"

#include <libgen.h>
#include <stdio.h>
#include <string.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <stdint.h>
#else
#include <unistd.h>
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

int load_hosts(const char *path, Host *hosts, int max) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int count = 0;
    char line[MAX_LINE];
    while (count < max && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        hosts[count].jump[0] = '\0';
        int fields = sscanf(line, "%511s %511s %511[^\n]",
                            hosts[count].nick, hosts[count].host, hosts[count].jump);
        if (fields >= 2)
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
