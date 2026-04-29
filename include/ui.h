#ifndef LILYPAD_UI_H
#define LILYPAD_UI_H

#include <stddef.h>
#include "common.h"

typedef enum {
    INTENT_CANCEL = 0,
    INTENT_SSH,
    INTENT_TMUX_DEFAULT,
    INTENT_TMUX_CHOOSE,
} Intent;

typedef struct {
    int host_index;
    Intent intent;
} HostPick;

typedef enum {
    SUB_CANCEL = 0,
    SUB_PLAIN,
    SUB_NEW,
    SUB_ATTACH,
} SubChoiceKind;

typedef struct {
    SubChoiceKind kind;
    char session[128];
    int force_plain;
} SubChoice;

typedef enum {
    TPL_CANCEL = 0,
    TPL_DEFAULT,
    TPL_NAMED,
} TemplateKind;

typedef struct {
    TemplateKind kind;
    char name[128];
} TemplatePick;

void ui_begin(void);
void ui_end(void);
void ui_status(const char *msg);

/* Inline text input. Pre-fills `out` with `default_value` (may be NULL).
 * Returns 0 when the user accepts with Enter, -1 on Esc/cancel. */
int ui_prompt(const char *label, char *out, size_t size, const char *default_value);

typedef enum {
    UI_CONFIRM_NO = 0,
    UI_CONFIRM_YES = 1,
    UI_CONFIRM_NEVER = 2,
} UiConfirm;

/** Three-way prompt: y, n, v (or !) for never. Esc maps to NO. */
UiConfirm ui_confirm3(const char *msg);

HostPick run_host_menu(Host *hosts, int *count, const char *hosts_path);
SubChoice run_tmux_menu(const char *host_label, const char *host, const char *jump,
                        char sessions[][128], int *n, int has_tmuxp);
TemplatePick run_template_menu(const char *host_label, char templates[][128], int n);

#endif
