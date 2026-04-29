#ifndef LILYPAD_INTEGRATION_H
#define LILYPAD_INTEGRATION_H

#include "common.h"

typedef enum {
    KIND_LOCAL,
    KIND_REMOTE,
} IntegrationKind;

typedef struct {
    const char *name;
    IntegrationKind kind;
    char marker_letter;            /**< 0 for KIND_LOCAL */
    int (*detect_local)(void);     /**< may be NULL */
    const char *probe_remote_cmd;  /**< NULL for KIND_LOCAL */
    int (*offer_install)(const Host *h); /**< may be NULL */
    int enabled;                   /**< hydrated at startup */
} Integration;

/** Walk all integrations in registry order. Returns NULL when out of range. */
const Integration *integration_at(int index);

/** Hydrate the `enabled` flag from config and detect_local. Call once at startup. */
void integrations_init(void);

#endif
