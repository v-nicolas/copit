
GCC=			/usr/bin/gcc

EXEC=			copit

SRC=			./src/

GTKSRC=                 `pkg-config --cflags --libs gtk+-3.0`

CSOURCE=		$(SRC)main.c

OBJS=			$(CSOURCE:.c=.o)

CFLAGS=			-O2 -I -W -Wall -Wextra \
			-pedantic -Wpedantic -std=c11 \
			-Wbad-function-cast \
			-Wcast-align \
			-Wcast-qual \
			-Wconversion  \
			-Wdate-time \
			-Wfloat-equal \
			-Wformat=2 \
			-Winit-self \
			-Wnested-externs \
			-Wnull-dereference \
			-Wold-style-definition \
			-Wpointer-arith \
			-Wshadow \
			-Wstack-protector \
			-Wstrict-prototypes \
			-Wswitch-default \
			-Wwrite-strings  \
			-Wmissing-prototypes \
			-Wformat-security \
			-fstack-protector-strong \
			-Wduplicated-cond \
			-Wformat-signedness \
			-Wjump-misses-init \
			-Wlogical-op \
			-Wnormalized \
			-Wsuggest-attribute=format \
			-Wtrampolines \
			-pie \
			-fPIE \
			-D_FORTIFY_SOURCE=2 \
			-D_XOPEN_SOURCE=700 \
			-DNDEBUG

LDFLAGS=

all:			$(OBJS) $(EXEC)

$(EXEC):
			$(GCC) -o $@ $(OBJS) $(LDFLAGS)  $(GTKSRC)

.c.o:
			$(GCC) $(CFLAGS) $(LDFLAGS) $(GTKSRC) -o $@ -c $<

.PHONY: clean  distclean install uninstall re

install:
			@echo "install $(EXEC) in /usr/bin/ ..."
			cp $(EXEC) /usr/bin

uninstall:
			@echo "delete $(EXEC) in /usr/bin/ ..."
			rm /usr/bin/$(EXEC)

clean:
			-rm ./*~
			-rm ./src/*~
			-rm ./src/*.o

distclean:
			@rm $(EXEC)

re:
			-make clean
			-make distclean
			make
