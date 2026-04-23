#ifndef SSH_MENU_HOSTS_H
#define SSH_MENU_HOSTS_H

#include <stddef.h>
#include "common.h"

void get_hosts_path(char *out, size_t size);
int load_hosts(const char *path, Host *hosts, int max);
int add_host(const char *path, const char *nick, const char *host, const char *jump);
int update_markers(const char *path, const char *nick, const char *markers);

/* Probe host for tmux and tmuxp via one ssh round-trip with BatchMode.
 * Writes markers listing MISSING capabilities ("", "t", "p", "tp") on
 * success, or "?" if the probe couldn't connect / authenticate.
 * Returns 0 if the probe ran (success or "?"), -1 on internal error. */
int probe_host(const char *host, const char *jump, char *out, size_t outsize);

#endif
