#ifndef SSH_MENU_UI_H
#define SSH_MENU_UI_H

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

HostPick run_host_menu(Host *hosts, int count, const char *hosts_path);
SubChoice run_tmux_menu(const char *host_label, char sessions[][128], int n, int has_tmuxp);
TemplatePick run_template_menu(const char *host_label, char templates[][128], int n);

#endif
