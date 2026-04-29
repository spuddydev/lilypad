#ifndef LILYPAD_CONFIG_H
#define LILYPAD_CONFIG_H

#include <stddef.h>

/** Resolve the config file path. */
void get_config_path(char *out, size_t size);

/** Load the config from disk into in-process state. Missing file is fine. */
void config_load(void);

/** Persist current state to disk atomically. Returns 0 on success. */
int config_save(void);

/** Read a string value. Returns the in-process string or `defv` if unset. */
const char *config_get_str(const char *key, const char *defv);

/** Read an integer value. Returns `defv` if unset or unparseable. */
int config_get_int(const char *key, int defv);

/** Read a boolean. Accepted truthy: on, true, 1, yes. */
int config_get_bool(const char *key, int defv);

/** Set a string value (in-process; caller is responsible for config_save). */
int config_set_str(const char *key, const char *value);

/** Remove a key (in-process). Returns 0 if removed, -1 if not present. */
int config_unset(const char *key);

/** Iterate all known schema keys. Returns NULL when index is out of range. */
const char *config_known_key(int index);

/** Print the current config to stdout (key = value, comments included). */
void config_print(void);

#endif
