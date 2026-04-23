CC      = cc
CFLAGS  = -Wall -Wextra -O2
LDLIBS  = -lcurses

OBJS = menu.o hosts.o ui.o

menu: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDLIBS)

menu.o:  menu.c  hosts.h ui.h common.h
hosts.o: hosts.c hosts.h common.h
ui.o:    ui.c    ui.h    common.h

clean:
	rm -f menu $(OBJS)

.PHONY: clean
