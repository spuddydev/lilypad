#ifndef SSH_MENU_COMMON_H
#define SSH_MENU_COMMON_H

#define MAX_PATH  4096
#define MAX_LINE  512
#define MAX_HOSTS 256

#define REMOTE_PATH_PREFIX \
    "PATH=$HOME/.local/bin:$HOME/bin:/opt/homebrew/bin:/usr/local/bin:$PATH; "

typedef struct {
    char nick[MAX_LINE];
    char host[MAX_LINE];
    char jump[MAX_LINE];
    char markers[8];
} Host;

#endif
