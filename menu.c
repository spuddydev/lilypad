#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <mach-o/dyld.h>

#define MAX_PATH  4096
#define MAX_LINE  512
#define MAX_HOSTS 256

typedef struct {
    char nick[MAX_LINE];
    char host[MAX_LINE];
    char jump[MAX_LINE];
} Host;

static void get_hosts_path(char *out, size_t size) {
    char exe[MAX_PATH];
    uint32_t len = sizeof(exe);
    _NSGetExecutablePath(exe, &len);
    snprintf(out, size, "%s/hosts", dirname(exe));
}

static int load_hosts(const char *path, Host *hosts, int max) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int count = 0;
    char line[MAX_LINE];
    while (count < max && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        hosts[count].jump[0] = '\0';
        int fields = sscanf(line, "%511s %511s %511[^\n]",
                            hosts[count].nick, hosts[count].host, hosts[count].jump);
        if (fields >= 2)
            count++;
    }
    fclose(f);
    return count;
}

static int cmd_add(int argc, char *argv[]) {
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "Usage: %s --add name@address nickname [jump_hosts]\n", argv[0]);
        return 1;
    }
    const char *host = argv[2];
    const char *nick = argv[3];
    const char *jump = argc == 5 ? argv[4] : NULL;

    if (!strchr(host, '@')) {
        fprintf(stderr, "Error: host must be in name@address format\n");
        return 1;
    }

    char hosts_path[MAX_PATH];
    get_hosts_path(hosts_path, sizeof(hosts_path));

    FILE *f = fopen(hosts_path, "r");
    if (f) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            char existing[MAX_LINE];
            if (sscanf(line, "%511s", existing) == 1 && strcmp(existing, nick) == 0) {
                fclose(f);
                fprintf(stderr, "Error: nickname '%s' already exists\n", nick);
                return 1;
            }
        }
        fclose(f);
    }

    f = fopen(hosts_path, "a");
    if (!f) { perror("Error opening hosts file"); return 1; }
    if (jump)
        fprintf(f, "%s %s %s\n", nick, host, jump);
    else
        fprintf(f, "%s %s\n", nick, host);
    fclose(f);
    if (jump)
        printf("Added: %s -> %s (via %s)\n", nick, host, jump);
    else
        printf("Added: %s -> %s\n", nick, host);
    return 0;
}

static void draw_menu(Host *hosts, int count, int selected) {
    int h, w;
    getmaxyx(stdscr, h, w);
    erase();

    const char *title = "SSH Menu";
    int tx = (w - (int)strlen(title)) / 2;
    mvaddstr(1, tx, title);
    mvchgat(1, tx, (int)strlen(title), A_BOLD | A_UNDERLINE, 0, NULL);

    for (int i = 0; i < count; i++) {
        char label[MAX_LINE * 2 + 16];
        snprintf(label, sizeof(label), "  %d. %s: %s  ", i + 1, hosts[i].nick, hosts[i].host);
        int x = (w - (int)strlen(label)) / 2;
        if (i == selected) {
            attron(COLOR_PAIR(1));
            mvaddstr(4 + i, x, label);
            attroff(COLOR_PAIR(1));
        } else {
            mvaddstr(4 + i, x, label);
        }
    }

    const char *hint = "↑↓/jk navigate  Enter select  q quit";
    attron(A_DIM);
    mvaddstr(h - 2, (w - (int)strlen(hint)) / 2, hint);
    attroff(A_DIM);

    refresh();
}

static int cmd_menu(void) {
    char hosts_path[MAX_PATH];
    get_hosts_path(hosts_path, sizeof(hosts_path));

    Host hosts[MAX_HOSTS];
    int count = load_hosts(hosts_path, hosts, MAX_HOSTS);

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
        return 0;
    }

    int selected = 0;
    int chosen = -1;

    while (1) {
        draw_menu(hosts, count, selected);
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

    if (chosen >= 0) {
        const Host *h = &hosts[chosen];
        if (h->jump[0])
            execlp("ssh", "ssh", "-J", h->jump, h->host, NULL);
        else
            execlp("ssh", "ssh", h->host, NULL);
        perror("ssh");
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--add") == 0)
        return cmd_add(argc, argv);
    return cmd_menu();
}
