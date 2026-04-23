#include "ui.h"

#include <curses.h>
#include <stdio.h>
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
        int x = (w - display_width(label)) / 2;
        int row = 4 + (i - top);
        if (i == selected) {
            attron(COLOR_PAIR(1));
            mvaddstr(row, x, label);
            attroff(COLOR_PAIR(1));
        } else {
            mvaddstr(row, x, label);
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
        ? "type to filter  ↑↓ navigate  Enter select  Esc cancel"
        : "↑↓/jk navigate  Enter select  s search  q quit";
    attron(A_DIM);
    mvaddstr(h - 2, (w - display_width(hint)) / 2, hint);
    attroff(A_DIM);

    refresh();
}

int run_menu(Host *hosts, int count) {
    initscr();
    noecho();
    cbreak();
    curs_set(0);
    keypad(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_BLACK, COLOR_CYAN);

    if (count == 0) {
        mvaddstr(0, 0, "No hosts found. Use: menu --add name@address nickname");
        getch();
        endwin();
        return -1;
    }

    int filtered[MAX_HOSTS];
    char query[64] = "";
    int fcount = apply_filter(hosts, count, query, filtered);
    int in_search = 0;
    int selected = 0;
    int top = 0;
    int chosen = -1;

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
                fcount = apply_filter(hosts, count, query, filtered);
                selected = 0;
                top = 0;
            } else if (key == '\n' || key == KEY_ENTER) {
                if (fcount > 0) {
                    chosen = filtered[selected];
                    break;
                }
            } else if (key == KEY_UP && selected > 0) {
                selected--;
            } else if (key == KEY_DOWN && selected < fcount - 1) {
                selected++;
            } else if (key == KEY_BACKSPACE || key == 127 || key == 8) {
                size_t n = strlen(query);
                if (n > 0) {
                    query[n - 1] = '\0';
                    fcount = apply_filter(hosts, count, query, filtered);
                    selected = 0;
                    top = 0;
                }
            } else if (key >= 32 && key < 127) {
                size_t n = strlen(query);
                if (n + 1 < sizeof(query)) {
                    query[n] = (char)key;
                    query[n + 1] = '\0';
                    fcount = apply_filter(hosts, count, query, filtered);
                    selected = 0;
                    top = 0;
                }
            }
        } else {
            if ((key == KEY_UP || key == 'k') && selected > 0)
                selected--;
            else if ((key == KEY_DOWN || key == 'j') && selected < fcount - 1)
                selected++;
            else if (key == '\n' || key == KEY_ENTER) {
                if (fcount > 0) {
                    chosen = filtered[selected];
                    break;
                }
            } else if (key == 'q') {
                break;
            } else if (key == 's' || key == '/') {
                in_search = 1;
            } else if (key >= '1' && key <= '9') {
                int idx = key - '1';
                if (idx < fcount) {
                    chosen = filtered[idx];
                    break;
                }
            }
        }
    }

    endwin();
    return chosen;
}
