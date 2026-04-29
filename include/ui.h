#ifndef LILYPAD_UI_H
#define LILYPAD_UI_H

#include <stddef.h>
#include "common.h"

/**
 * @defgroup ui Menu and Prompts
 * @brief ncurses widgets, host menu, session sub-menu, and small prompts.
 * @{
 */

/** What the user wants to do with a picked host. */
typedef enum {
    INTENT_CANCEL = 0,        /**< Quit without picking. */
    INTENT_SSH,               /**< Plain ssh, no tmux. */
    INTENT_TMUX_DEFAULT,      /**< Default tmux session named "main". */
    INTENT_TMUX_CHOOSE,       /**< Open the session sub-menu. */
} Intent;

/** Result of the top-level host menu. */
typedef struct {
    int host_index;           /**< Index into the hosts array. */
    Intent intent;            /**< Connect intent or INTENT_CANCEL. */
} HostPick;

/** Branch of the session sub-menu the user picked. */
typedef enum {
    SUB_CANCEL = 0,           /**< Back / quit without picking. */
    SUB_PLAIN,                /**< Plain ssh. */
    SUB_NEW,                  /**< Start a fresh session (template flow). */
    SUB_ATTACH,               /**< Attach to an existing named session. */
} SubChoiceKind;

/** Result of the session sub-menu. */
typedef struct {
    SubChoiceKind kind;       /**< Picked branch or SUB_CANCEL. */
    char session[128];        /**< Session name when kind == SUB_ATTACH. */
    int force_plain;          /**< When 1, skip iTerm's `tmux -CC`. */
} SubChoice;

/** Branch of the template menu the user picked. */
typedef enum {
    TPL_CANCEL = 0,           /**< Back without picking. */
    TPL_DEFAULT,              /**< Default tmux session, no template. */
    TPL_NAMED,                /**< Run a named tmuxp template. */
} TemplateKind;

/** Result of the template menu. */
typedef struct {
    TemplateKind kind;        /**< Picked branch or TPL_CANCEL. */
    char name[128];           /**< Template basename when kind == TPL_NAMED. */
} TemplatePick;

/** Initialise ncurses (initscr, colours, raw mode). */
void ui_begin(void);
/** Tear ncurses down (endwin). */
void ui_end(void);
/** Show a transient status line at the bottom of the screen. */
void ui_status(const char *msg);

/** Inline text input. Pre-fills `out` with `default_value` (may be NULL).
 *  Returns 0 when the user accepts with Enter, -1 on Esc/cancel. */
int ui_prompt(const char *label, char *out, size_t size, const char *default_value);

typedef enum {
    UI_CONFIRM_NO = 0,
    UI_CONFIRM_YES = 1,
    UI_CONFIRM_NEVER = 2,
} UiConfirm;

/** Three-way prompt: y, n, v (or !) for never. Esc maps to NO. */
UiConfirm ui_confirm3(const char *msg);

/** Run the top-level host menu loop. May mutate `hosts` and `*count`
 *  (add / delete / reorder) and rewrite the hosts file at `hosts_path`. */
HostPick run_host_menu(Host *hosts, int *count, const char *hosts_path);

/** Run the per-host session sub-menu. `sessions` and `*n` may be mutated
 *  by kill or rename actions. `has_tmuxp` toggles the template flow. */
SubChoice run_tmux_menu(const char *host_label, const char *host, const char *jump,
                        char sessions[][128], int *n, int has_tmuxp);

/** Pick from the available tmuxp templates for a host. */
TemplatePick run_template_menu(const char *host_label, char templates[][128], int n);

/** @} */

#endif
