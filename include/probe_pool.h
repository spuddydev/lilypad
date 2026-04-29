#ifndef LILYPAD_PROBE_POOL_H
#define LILYPAD_PROBE_POOL_H

#include "common.h"

#define POOL_MAX_PARALLEL 16
#define POOL_MAX_SESSIONS 16

typedef struct {
    char nick[MAX_LINE];
    char markers[8];
    char sessions[POOL_MAX_SESSIONS][128];
    int session_count;
} ProbeResult;

/** Initialise the pool with a parallelism cap. Pulled from
 *  config.probe.max_parallel by run_host_menu. */
void probe_pool_init(int max_parallel);

/** Queue a host for probing. Idempotent for the same nick within a tick. */
void probe_pool_enqueue(const char *nick, const char *host, const char *jump);

/** Launch any queued items up to the parallelism cap. */
void probe_pool_pump(void);

/** Non-blocking drain. For each completed probe, invoke `cb` with the
 *  parsed result. Returns the number of completed probes. */
int probe_pool_drain_now(void (*cb)(const ProbeResult *r, void *user), void *user);

/** SIGTERM all in-flight probes, waitpid up to ~1s, then SIGKILL stragglers. */
void probe_pool_shutdown(void);

/** Number of slots currently running. */
int probe_pool_active(void);

/** Number of items queued behind the parallelism cap. */
int probe_pool_queued(void);

#endif
