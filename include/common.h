#ifndef LILYPAD_COMMON_H
#define LILYPAD_COMMON_H

/** Human-readable lilypad version. Bumped per release. */
#define LILYPAD_VERSION "1.1.2"

/** Filesystem path buffer size. */
#define MAX_PATH  4096
/** Generic line / short-string buffer size. */
#define MAX_LINE  512
/** Hard cap on the number of saved hosts. */
#define MAX_HOSTS 256

/** Prefix prepended to every remote shell command so common install
 *  locations (`~/.local/bin`, Homebrew paths) appear on `$PATH`. */
#define REMOTE_PATH_PREFIX \
    "PATH=$HOME/.local/bin:$HOME/bin:/opt/homebrew/bin:/usr/local/bin:$PATH; "

/** A single saved host entry. */
typedef struct {
    char nick[MAX_LINE];    /**< Local nickname (whitespace-free). */
    char host[MAX_LINE];    /**< `user@addr` connection string. */
    char jump[MAX_LINE];    /**< Optional comma-separated ProxyJump hops. */
    char markers[8];        /**< Missing-capability marker string. */
} Host;

#endif
