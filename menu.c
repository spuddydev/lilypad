#include "hosts.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
    return 0;
}

static int cmd_menu(void) {
    char hosts_path[MAX_PATH];
    get_hosts_path(hosts_path, sizeof(hosts_path));

    Host hosts[MAX_HOSTS];
    int count = load_hosts(hosts_path, hosts, MAX_HOSTS);

    int chosen = run_menu(hosts, count);
    if (chosen < 0) return 0;

    const Host *h = &hosts[chosen];
    if (h->jump[0])
        execlp("ssh", "ssh", "-J", h->jump, h->host, NULL);
    else
        execlp("ssh", "ssh", h->host, NULL);
    perror("ssh");
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--add") == 0)
        return cmd_add(argc, argv);
    return cmd_menu();
}
