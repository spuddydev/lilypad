#include "ui.h"

#include <curses.h>
#include <stdio.h>

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

static void draw_menu(Host *hosts, int count, int selected, int top, int visible) {
    int h, w;
    getmaxyx(stdscr, h, w);
    erase();

    const char *title = "SSH Menu";
    int tw = display_width(title);
    int tx = (w - tw) / 2;
    mvaddstr(1, tx, title);
    mvchgat(1, tx, tw, A_BOLD | A_UNDERLINE, 0, NULL);

    int end = top + visible;
    if (end > count) end = count;
    for (int i = top; i < end; i++) {
        char label[MAX_LINE * 2 + 16];
        snprintf(label, sizeof(label), "  %d. %s: %s  ", i + 1, hosts[i].nick, hosts[i].host);
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
    if (end < count)
        mvaddstr(4 + visible, (w - more_w) / 2, more);

    const char *hint = "↑↓/jk navigate  Enter select  q quit";
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

    int selected = 0;
    int top = 0;
    int chosen = -1;

    while (1) {
        int h, w;
        getmaxyx(stdscr, h, w);
        int visible = h - 6;
        if (visible < 1) visible = 1;
        top = clamp_top(top, selected, visible);

        draw_menu(hosts, count, selected, top, visible);
        int key = getch();

        if ((key == KEY_UP || key == 'k') && selected > 0)
            selected--;
        else if ((key == KEY_DOWN || key == 'j') && selected < count - 1)
            selected++;
        else if (key == '\n' || key == KEY_ENTER) {
            chosen = selected;
            break;
        } else if (key == 'q') {
            break;
        } else if (key >= '1' && key <= '9') {
            int idx = key - '1';
            if (idx < count) {
                chosen = idx;
                break;
            }
        }
    }

    endwin();
    return chosen;
}
