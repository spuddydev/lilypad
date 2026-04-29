#ifndef LILYPAD_CLI_H
#define LILYPAD_CLI_H

#include <stddef.h>
#include "common.h"

/**
 * @defgroup cli CLI Dispatch
 * @brief Subcommand routing and post-pick install prompt orchestration.
 * @{
 */

/** Parse argv and run the matching subcommand or open the menu. */
int cli_dispatch(int argc, char *argv[]);

/** Run the interactive ncurses menu loop. */
int cmd_menu(void);

/** Curses-side: prompt y/n/never for each missing remote integration.
 *  NEVER decisions persist immediately into state. YES letters are
 *  returned in `out` (NUL-terminated). */
void prompt_install_decisions(const Host *h, char *out, size_t out_size);

/** Post-curses: run offer_install for each letter, then reprobe to
 *  refresh markers. Prints "lilypad: re-probing %s..." to stdout. */
void apply_install_decisions(const Host *h, const char *letters);

/** @} */

#endif
