#ifndef LILYPAD_STATE_H
#define LILYPAD_STATE_H

#include <stddef.h>
#include <time.h>
#include "common.h"

/** Cap on cached tmux session names per host. */
#define STATE_MAX_SESSIONS 16

/** Per-host runtime state persisted in the state file. */
typedef struct {
    char nick[MAX_LINE];                       /**< Host nickname (key). */
    char markers[8];                           /**< Last-probed marker string. */
    time_t last_probe;                         /**< Unix time of the last probe. */
    char declined[8];                          /**< Letters the user said "never" to. */
    char sessions[STATE_MAX_SESSIONS][128];    /**< Cached session names. */
    int session_count;                         /**< Valid entries in `sessions`. */
    int set;                                   /**< Internal occupancy flag. */
} HostState;

/** Read the state file into in-process memory. Idempotent. */
void state_load(void);

/** Persist current state to disk atomically. Returns 0 on success. */
int state_save(void);

/** Look up state by nick, or NULL if absent. */
HostState *state_get(const char *nick);

/** Insert or replace state for a nick. Triggers a save unless inside a batch. */
int state_put(const HostState *st);

/** Update markers and last_probe for a nick. Creates the entry if missing. */
int state_set_markers(const char *nick, const char *markers);

/** Update the cached session list for a nick. */
int state_set_sessions(const char *nick, char sessions[][128], int n);

/** Append a marker letter to declined if not already present. */
int state_add_decline(const char *nick, char letter);

/** Remove a marker letter from declined if present. */
int state_remove_decline(const char *nick, char letter);

/** Start a write batch. Mutations buffer in memory until state_commit_batch. */
void state_begin_batch(void);

/** Flush a write batch atomically. */
int state_commit_batch(void);

/** Abandon a write batch without flushing. */
void state_abort_batch(void);

/** True when a state file exists on disk. */
int state_file_exists(void);

/** Migrate any legacy `#markers` token from the hosts file into state.
 *  Trigger: state file absent AND hosts file contains a `#`-prefixed token. */
void migrate_hosts_to_state(void);

/** Percent-encode a value containing space, comma, '=', '#', '%'. */
void state_encode(char *dst, size_t dstsz, const char *src);

/** Inverse of state_encode. */
void state_decode(char *dst, size_t dstsz, const char *src);

#endif
