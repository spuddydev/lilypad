#ifndef SSH_MENU_HOSTS_H
#define SSH_MENU_HOSTS_H

#include <stddef.h>
#include "common.h"

void get_hosts_path(char *out, size_t size);
int load_hosts(const char *path, Host *hosts, int max);
int add_host(const char *path, const char *nick, const char *host, const char *jump);

#endif
