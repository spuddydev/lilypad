#ifndef LILYPAD_EXEC_H
#define LILYPAD_EXEC_H

#include <stddef.h>
#include "common.h"

/** True when the local terminal is iTerm (TERM_PROGRAM == iTerm.app). */
int is_iterm(void);

/** True when the host's marker string indicates tmux is present remotely. */
int host_has_tmux(const Host *h);

/** True when the host's marker string indicates tmuxp is present remotely. */
int host_has_tmuxp(const Host *h);

/** Replace the current process with a plain ssh session to `h`. Returns 1 on failure. */
int exec_ssh_plain(const Host *h);

/** Replace the current process with `ssh -t ... <remote_cmd>`. Returns 1 on failure. */
int exec_ssh_tmux(const Host *h, const char *remote_cmd);

/** scp a tmuxp template to the remote and start a session from it.
 *  `force_plain` skips iTerm's tmux -CC. Returns 1 on failure. */
int exec_template(const Host *h, const char *template_name, int force_plain);

/** Pick "untitled" or "untitled-<n>" not already in `sessions`. */
void auto_session_name(char sessions[][128], int ns, char *out, size_t outsize);

#endif
