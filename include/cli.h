#ifndef LILYPAD_CLI_H
#define LILYPAD_CLI_H

/** Parse argv and run the matching subcommand or open the menu. */
int cli_dispatch(int argc, char *argv[]);

/** Run the interactive ncurses menu loop. */
int cmd_menu(void);

#endif
