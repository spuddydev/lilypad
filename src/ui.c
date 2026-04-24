#include "ui.h"
#include "hosts.h"

#include <curses.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int display_width(const char *s) {
    int n = 0;
    for (; *s; s++)
        if (((unsigned char)*s & 0xc0) != 0x80) n++;
    return n;
}

static int clamp_top(int top, int selected, int visible) {
    if (selected < top) top = selected;
    if (selected >= top + visible) top = selected - visible + 1;
    if (top < 0) top = 0;
    return top;
}

static int ci_contains(const char *hay, const char *needle) {
    if (!*needle) return 1;
    size_t nlen = strlen(needle);
    for (; *hay; hay++)
        if (strncasecmp(hay, needle, nlen) == 0) return 1;
    return 0;
}

static int apply_filter(const Host *hosts, int count, const char *q, int *out) {
    int n = 0;
    for (int i = 0; i < count; i++)
        if (ci_contains(hosts[i].nick, q) || ci_contains(hosts[i].host, q))
            out[n++] = i;
    return n;
}

void ui_begin(void) {
    setlocale(LC_ALL, "");
    initscr();
    noecho();
    cbreak();
    curs_set(0);
    keypad(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_BLACK, COLOR_CYAN);
    init_pair(2, COLOR_RED, COLOR_BLACK);
}

void ui_end(void) {
    endwin();
}

void ui_status(const char *msg) {
    erase();
    int h, w;
    getmaxyx(stdscr, h, w);
    attron(A_DIM);
    mvaddstr(h / 2, (w - display_width(msg)) / 2, msg);
    attroff(A_DIM);
    refresh();
}

static int ui_confirm(const char *msg) {
    curs_set(0);
    while (1) {
        erase();
        int h, w;
        getmaxyx(stdscr, h, w);
        attron(A_BOLD);
        mvaddstr(h / 2 - 1, (w - display_width(msg)) / 2, msg);
        attroff(A_BOLD);
        const char *hint = "[y] yes  [n] no";
        attron(A_DIM);
        mvaddstr(h - 2, (w - display_width(hint)) / 2, hint);
        attroff(A_DIM);
        refresh();
        int k = getch();
        if (k == 'y' || k == 'Y') return 1;
        if (k == 'n' || k == 'N' || k == 27 || k == 'q') return 0;
    }
}

int ui_prompt(const char *label, char *out, size_t size, const char *default_value) {
    if (size == 0) return -1;
    if (default_value) {
        size_t n = strlen(default_value);
        if (n >= size) n = size - 1;
        memcpy(out, default_value, n);
        out[n] = '\0';
    } else {
        out[0] = '\0';
    }
    size_t pos = strlen(out);

    curs_set(1);
    while (1) {
        erase();
        int h, w;
        getmaxyx(stdscr, h, w);

        int lx = (w - display_width(label)) / 2;
        mvaddstr(h / 2 - 1, lx, label);

        char line[256];
        snprintf(line, sizeof(line), "> %s", out);
        int line_w = display_width(line);
        int bx = (w - line_w) / 2;
        mvaddstr(h / 2 + 1, bx, line);

        const char *hint = "[enter] accept  [esc] cancel";
        attron(A_DIM);
        mvaddstr(h - 2, (w - display_width(hint)) / 2, hint);
        attroff(A_DIM);

        move(h / 2 + 1, bx + 2 + (int)pos);
        refresh();

        int key = getch();
        if (key == 27) { curs_set(0); return -1; }
        if (key == '\n' || key == KEY_ENTER) {
            if (pos > 0) { curs_set(0); return 0; }
        } else if (key == KEY_BACKSPACE || key == 127 || key == 8) {
            if (pos > 0) { pos--; out[pos] = '\0'; }
        } else if (key >= 32 && key < 127 && pos + 1 < size) {
            out[pos++] = (char)key;
            out[pos] = '\0';
        }
    }
}

static void draw_menu(const Host *hosts, const int *filtered, int fcount,
                      int selected, int top, int visible,
                      const char *query, int in_search) {
    int h, w;
    getmaxyx(stdscr, h, w);
    erase();

    const char *title = "SSH Menu";
    int tw = display_width(title);
    int tx = (w - tw) / 2;
    mvaddstr(1, tx, title);
    mvchgat(1, tx, tw, A_BOLD | A_UNDERLINE, 0, NULL);

    if (fcount == 0) {
        const char *msg = "(no matches)";
        mvaddstr(4, (w - display_width(msg)) / 2, msg);
    }

    int end = top + visible;
    if (end > fcount) end = fcount;
    for (int i = top; i < end; i++) {
        const Host *hh = &hosts[filtered[i]];
        char label[MAX_LINE * 2 + 16];
        snprintf(label, sizeof(label), "  %d. %s: %s  ", i + 1, hh->nick, hh->host);
        int lw = display_width(label);
        int mw = hh->markers[0] ? (int)strlen(hh->markers) + 1 : 0;
        int x = (w - lw - mw) / 2;
        int row = 4 + (i - top);
        if (i == selected) {
            attron(COLOR_PAIR(1));
            mvaddstr(row, x, label);
            attroff(COLOR_PAIR(1));
        } else {
            mvaddstr(row, x, label);
        }
        if (hh->markers[0]) {
            attron(COLOR_PAIR(2) | A_BOLD);
            mvaddstr(row, x + lw + 1, hh->markers);
            attroff(COLOR_PAIR(2) | A_BOLD);
        }
    }

    const char *more = "...";
    int more_w = display_width(more);
    if (top > 0)
        mvaddstr(3, (w - more_w) / 2, more);
    if (end < fcount)
        mvaddstr(4 + visible, (w - more_w) / 2, more);

    if (in_search || query[0]) {
        char sline[128];
        snprintf(sline, sizeof(sline), "/%s%s", query, in_search ? "_" : "");
        mvaddstr(h - 3, (w - display_width(sline)) / 2, sline);
    }

    const char *hint = in_search
        ? "type to filter  [enter] select  [esc] cancel"
        : "[j/k] move  [J/K] reorder  [D] delete  [enter] open  [s] ssh  [t] tmux  [/] search  [r] refresh  [q] quit";
    attron(A_DIM);
    mvaddstr(h - 2, (w - display_width(hint)) / 2, hint);
    attroff(A_DIM);

    refresh();
}

static void refresh_host(Host *h, const char *hosts_path) {
    int hh, ww;
    getmaxyx(stdscr, hh, ww);
    const char *msg = "Probing...";
    move(hh - 2, 0);
    clrtoeol();
    attron(A_DIM);
    mvaddstr(hh - 2, (ww - display_width(msg)) / 2, msg);
    attroff(A_DIM);
    refresh();

    char new_markers[8];
    if (probe_host(h->host, h->jump, new_markers, sizeof(new_markers)) != 0)
        return;
    if (hosts_path) update_markers(hosts_path, h->nick, new_markers);
    size_t n = strlen(new_markers);
    if (n >= sizeof(h->markers)) n = sizeof(h->markers) - 1;
    memcpy(h->markers, new_markers, n);
    h->markers[n] = '\0';
}

HostPick run_host_menu(Host *hosts, int *count, const char *hosts_path) {
    HostPick result = {-1, INTENT_CANCEL};

    if (*count == 0) {
        mvaddstr(0, 0, "No hosts found. Use: menu --add name@address nickname");
        getch();
        return result;
    }

    int filtered[MAX_HOSTS];
    char query[64] = "";
    int fcount = apply_filter(hosts, *count, query, filtered);
    int in_search = 0;
    int selected = 0;
    int top = 0;

    while (1) {
        int h, w;
        getmaxyx(stdscr, h, w);
        (void)w;
        int visible = h - 7 - ((in_search || query[0]) ? 1 : 0);
        if (visible < 1) visible = 1;
        if (selected >= fcount) selected = fcount > 0 ? fcount - 1 : 0;
        top = clamp_top(top, selected, visible);

        draw_menu(hosts, filtered, fcount, selected, top, visible, query, in_search);
        int key = getch();

        if (in_search) {
            if (key == 27) {
                in_search = 0;
                query[0] = '\0';
                fcount = apply_filter(hosts, *count, query, filtered);
                selected = 0;
                top = 0;
            } else if (key == '\n' || key == KEY_ENTER) {
                if (fcount > 0) {
                    result.host_index = filtered[selected];
                    result.intent = INTENT_TMUX_CHOOSE;
                    return result;
                }
            } else if (key == KEY_UP && selected > 0) {
                selected--;
            } else if (key == KEY_DOWN && selected < fcount - 1) {
                selected++;
            } else if (key == KEY_BACKSPACE || key == 127 || key == 8) {
                size_t n = strlen(query);
                if (n > 0) {
                    query[n - 1] = '\0';
                    fcount = apply_filter(hosts, *count, query, filtered);
                    selected = 0;
                    top = 0;
                }
            } else if (key >= 32 && key < 127) {
                size_t n = strlen(query);
                if (n + 1 < sizeof(query)) {
                    query[n] = (char)key;
                    query[n + 1] = '\0';
                    fcount = apply_filter(hosts, *count, query, filtered);
                    selected = 0;
                    top = 0;
                }
            }
        } else {
            if ((key == KEY_UP || key == 'k') && selected > 0) {
                selected--;
            } else if ((key == KEY_DOWN || key == 'j') && selected < fcount - 1) {
                selected++;
            } else if ((key == KEY_SR || key == 'K') && selected > 0) {
                int a = filtered[selected];
                int b = filtered[selected - 1];
                Host tmp = hosts[a]; hosts[a] = hosts[b]; hosts[b] = tmp;
                selected--;
                if (hosts_path) save_hosts(hosts_path, hosts, *count);
            } else if ((key == KEY_SF || key == 'J') && selected < fcount - 1) {
                int a = filtered[selected];
                int b = filtered[selected + 1];
                Host tmp = hosts[a]; hosts[a] = hosts[b]; hosts[b] = tmp;
                selected++;
                if (hosts_path) save_hosts(hosts_path, hosts, *count);
            } else if ((key == KEY_SDC || key == KEY_DC || key == 'D') && fcount > 0) {
                int idx = filtered[selected];
                char msg[MAX_LINE + 32];
                snprintf(msg, sizeof(msg), "Delete '%s'?", hosts[idx].nick);
                if (ui_confirm(msg)) {
                    for (int i = idx; i < *count - 1; i++) hosts[i] = hosts[i + 1];
                    (*count)--;
                    if (hosts_path) save_hosts(hosts_path, hosts, *count);
                    fcount = apply_filter(hosts, *count, query, filtered);
                    if (selected >= fcount) selected = fcount > 0 ? fcount - 1 : 0;
                }
            } else if (key == '\n' || key == KEY_ENTER) {
                if (fcount > 0) {
                    result.host_index = filtered[selected];
                    result.intent = INTENT_TMUX_CHOOSE;
                    return result;
                }
            } else if (key == 's') {
                if (fcount > 0) {
                    result.host_index = filtered[selected];
                    result.intent = INTENT_SSH;
                    return result;
                }
            } else if (key == 't') {
                if (fcount > 0) {
                    result.host_index = filtered[selected];
                    result.intent = INTENT_TMUX_DEFAULT;
                    return result;
                }
            } else if (key == '/') {
                in_search = 1;
            } else if (key == 'r') {
                if (fcount > 0)
                    refresh_host(&hosts[filtered[selected]], hosts_path);
            } else if (key == 'q') {
                return result;
            } else if (key >= '1' && key <= '9') {
                int idx = key - '1';
                if (idx < fcount) selected = idx;
            }
        }
    }
}

static void draw_submenu(const char *title, const char * const *items, int n,
                         int selected, int top, int visible,
                         const char *warning, const char *hint) {
    int h, w;
    getmaxyx(stdscr, h, w);
    erase();

    int tw = display_width(title);
    mvaddstr(1, (w - tw) / 2, title);
    mvchgat(1, (w - tw) / 2, tw, A_BOLD | A_UNDERLINE, 0, NULL);

    int end = top + visible;
    if (end > n) end = n;
    for (int i = top; i < end; i++) {
        char label[256];
        snprintf(label, sizeof(label), "  %s  ", items[i]);
        int lw = display_width(label);
        int x = (w - lw) / 2;
        int row = 4 + (i - top);
        if (i == selected) {
            attron(COLOR_PAIR(1));
            mvaddstr(row, x, label);
            attroff(COLOR_PAIR(1));
        } else {
            mvaddstr(row, x, label);
        }
    }

    if (warning && warning[0]) {
        attron(COLOR_PAIR(2));
        mvaddstr(h - 3, (w - display_width(warning)) / 2, warning);
        attroff(COLOR_PAIR(2));
    }

    attron(A_DIM);
    mvaddstr(h - 2, (w - display_width(hint)) / 2, hint);
    attroff(A_DIM);

    refresh();
}

static int submenu_loop(const char *title, const char * const *items, int n,
                        const char *warning) {
    int selected = 0, top = 0;
    const char *hint = "[j/k] move  [enter] select  [q] back";
    while (1) {
        int h, w;
        getmaxyx(stdscr, h, w);
        (void)w;
        int visible = h - 6 - (warning && warning[0] ? 1 : 0);
        if (visible < 1) visible = 1;
        if (selected < 0) selected = 0;
        if (selected >= n) selected = n - 1;
        top = clamp_top(top, selected, visible);

        draw_submenu(title, items, n, selected, top, visible, warning, hint);
        int key = getch();

        if ((key == KEY_UP || key == 'k') && selected > 0)
            selected--;
        else if ((key == KEY_DOWN || key == 'j') && selected < n - 1)
            selected++;
        else if (key == '\n' || key == KEY_ENTER)
            return selected;
        else if (key == 'q' || key == 27)
            return -1;
    }
}

SubChoice run_tmux_menu(const char *host_label, const char *host, const char *jump,
                        char sessions[][128], int *n, int has_tmuxp) {
    SubChoice result = {SUB_CANCEL, "", 0};

    if (*n > 16) *n = 16;

    char title[256];
    snprintf(title, sizeof(title), "%s: pick session", host_label);

    const char *warn = has_tmuxp ? NULL : "tmuxp not on host: templates won't load";
    const char *term = getenv("TERM_PROGRAM");
    int is_iterm_local = term && strcmp(term, "iTerm.app") == 0;
    const char *hint = is_iterm_local
        ? "[j/k] move  [enter] select  [t] plain (no -CC)  [D] kill  [q] back"
        : "[j/k] move  [enter] select  [D] kill  [q] back";

    int selected = 0, top = 0;
    while (1) {
        int item_count = 2 + *n;
        const char *items[2 + 16];
        char session_labels[16][160];
        items[0] = "Plain shell";
        items[1] = "New tmux session";
        for (int i = 0; i < *n; i++) {
            snprintf(session_labels[i], sizeof(session_labels[i]), "Attach: %s", sessions[i]);
            items[2 + i] = session_labels[i];
        }

        int h, w;
        getmaxyx(stdscr, h, w);
        (void)w;
        int visible = h - 6 - (warn && warn[0] ? 1 : 0);
        if (visible < 1) visible = 1;
        if (selected < 0) selected = 0;
        if (selected >= item_count) selected = item_count - 1;
        top = clamp_top(top, selected, visible);

        draw_submenu(title, items, item_count, selected, top, visible, warn, hint);
        int key = getch();

        if ((key == KEY_UP || key == 'k') && selected > 0) {
            selected--;
        } else if ((key == KEY_DOWN || key == 'j') && selected < item_count - 1) {
            selected++;
        } else if ((key == KEY_SDC || key == KEY_DC || key == 'D') && selected >= 2) {
            int sidx = selected - 2;
            char msg[256];
            snprintf(msg, sizeof(msg), "Kill session '%s'?", sessions[sidx]);
            if (ui_confirm(msg)) {
                ui_status("Killing session...");
                kill_session(host, jump, sessions[sidx]);
                for (int i = sidx; i < *n - 1; i++)
                    memcpy(sessions[i], sessions[i + 1], sizeof(sessions[i]));
                (*n)--;
                if (selected >= 2 + *n) selected = 2 + *n - 1;
                if (selected < 0) selected = 0;
            }
        } else if (key == '\n' || key == KEY_ENTER || key == 't') {
            int force_plain = (key == 't');
            if (selected == 0) {
                result.kind = SUB_PLAIN;
            } else if (selected == 1) {
                result.kind = SUB_NEW;
                result.force_plain = force_plain;
            } else {
                result.kind = SUB_ATTACH;
                result.force_plain = force_plain;
                int sidx = selected - 2;
                size_t len = strlen(sessions[sidx]);
                if (len >= sizeof(result.session)) len = sizeof(result.session) - 1;
                memcpy(result.session, sessions[sidx], len);
                result.session[len] = '\0';
            }
            return result;
        } else if (key == 'q' || key == 27) {
            return result;
        }
    }
}

TemplatePick run_template_menu(const char *host_label, char templates[][128], int n) {
    TemplatePick result = {TPL_CANCEL, ""};

    if (n > 32) n = 32;
    int item_count = 1 + n;
    const char *items[1 + 32];
    items[0] = "default (no template)";
    for (int i = 0; i < n; i++)
        items[1 + i] = templates[i];

    char title[256];
    snprintf(title, sizeof(title), "%s: pick tmuxp template", host_label);

    int picked = submenu_loop(title, items, item_count, NULL);
    if (picked < 0) return result;

    if (picked == 0) {
        result.kind = TPL_DEFAULT;
    } else {
        result.kind = TPL_NAMED;
        size_t len = strlen(templates[picked - 1]);
        if (len >= sizeof(result.name)) len = sizeof(result.name) - 1;
        memcpy(result.name, templates[picked - 1], len);
        result.name[len] = '\0';
    }
    return result;
}
