#ifndef SSH_MENU_HOSTS_H
#define SSH_MENU_HOSTS_H

#include <stddef.h>
#include "common.h"

void get_hosts_path(char *out, size_t size);
void migrate_legacy_config(void);
int load_hosts(const char *path, Host *hosts, int max);
int add_host(const char *path, const char *nick, const char *host, const char *jump);
int update_markers(const char *path, const char *nick, const char *markers);

/* Rewrite the hosts file from the in-memory array in its current order. */
int save_hosts(const char *path, const Host *hosts, int count);

/* Kill a remote tmux session. Returns 0 on success, -1 on failure. */
int kill_session(const char *host, const char *jump, const char *session);

/* Rename a remote tmux session. Returns 0 on success, -1 on failure. */
int rename_session(const char *host, const char *jump,
                   const char *old_name, const char *new_name);

/* Escape single quotes for safe embedding inside a single-quoted shell
 * string: ' becomes '\''. Truncates safely if dst is too small. */
void shell_sq_escape(char *dst, size_t dstsz, const char *src);

/* Probe host for tmux and tmuxp via one ssh round-trip with BatchMode.
 * Writes markers listing MISSING capabilities ("", "t", "p", "tp") on
 * success, or "?" if the probe couldn't connect / authenticate.
 * Returns 0 if the probe ran (success or "?"), -1 on internal error. */
int probe_host(const char *host, const char *jump, char *out, size_t outsize);

/* Fetch tmux session names from the remote via `tmux ls`. Each entry is
 * the bare session name (text before the colon). Returns the count, 0 if
 * none, or -1 on ssh error. */
int fetch_sessions(const char *host, const char *jump, char sessions[][128], int max);

void get_templates_dir(char *out, size_t size);
int  list_templates(char names[][128], int max);
int  install_default_templates(void);
int  install_template_from_path(const char *src);

#endif
