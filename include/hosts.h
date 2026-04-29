#ifndef LILYPAD_HOSTS_H
#define LILYPAD_HOSTS_H

#include <stddef.h>
#include "common.h"

/**
 * @defgroup hosts Hosts and Probing
 * @brief Hosts file storage, the remote probe, and the templates directory.
 * @{
 */

/** Resolve the hosts file path under `$XDG_CONFIG_HOME/lilypad` or `~/.config/lilypad`. */
void get_hosts_path(char *out, size_t size);

/** Move a legacy `~/.config/ssh-menu` directory in place on first run.
 *  No-op if the new dir already exists or the legacy dir is absent. */
void migrate_legacy_config(void);

/** True when `nick` clashes with a CLI subcommand or starts with '-'. */
int is_reserved_nick(const char *nick);

/** Parse the hosts file into the `hosts` array. Returns the count, or -1 on error. */
int load_hosts(const char *path, Host *hosts, int max);

/** Append a host. Validates nickname uniqueness and `user@addr` shape. */
int add_host(const char *path, const char *nick, const char *host, const char *jump);

/** Persist updated markers for a single nick (writes to state file). */
int update_markers(const char *path, const char *nick, const char *markers);

/** Rewrite the hosts file from the in-memory array in its current order. */
int save_hosts(const char *path, const Host *hosts, int count);

/** Kill a remote tmux session. Returns 0 on success, -1 on failure. */
int kill_session(const char *host, const char *jump, const char *session);

/** Rename a remote tmux session. Returns 0 on success, -1 on failure. */
int rename_session(const char *host, const char *jump,
                   const char *old_name, const char *new_name);

/** Escape single quotes for safe embedding inside a single-quoted shell
 *  string: `'` becomes `'\''`. Truncates safely if dst is too small. */
void shell_sq_escape(char *dst, size_t dstsz, const char *src);

/** Probe host for tmux and tmuxp via one ssh round-trip with BatchMode.
 *  Writes markers listing MISSING capabilities ("", "t", "p", "tp") on
 *  success, or "?" if the probe couldn't connect or authenticate.
 *  Returns 0 if the probe ran (success or "?"), -1 on internal error. */
int probe_host(const char *host, const char *jump, char *out, size_t outsize);

/** Fetch tmux session names from the remote via `tmux ls`. Each entry is
 *  the bare session name (text before the colon). Returns the count, 0 if
 *  none, or -1 on ssh error. */
int fetch_sessions(const char *host, const char *jump, char sessions[][128], int max);

/** Resolve the tmuxp template directory under the lilypad config dir. */
void get_templates_dir(char *out, size_t size);

/** List template basenames (sans `.yaml` suffix). Returns the count. */
int  list_templates(char names[][128], int max);

/** Seed the templates dir with the bundled default template. Idempotent. */
int  install_default_templates(void);

/** Copy a template file from `src` into the templates dir. */
int  install_template_from_path(const char *src);

/** @} */

#endif
