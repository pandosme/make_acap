PROG	= acapp_events
OBJS1   = main.c cJSON.c APP.c HTTP.c FILE.c DEVICE.c EVENTS.c

PROGS	= $(PROG)

PKGS	= glib-2.0 gio-2.0 axhttp axevent

CFLAGS  += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS  += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))
LDLIBS  += -Wl,-ldl
LDLIBS += -s -lm -laxparameter

all:	$(PROGS)

$(PROG): $(OBJS1)
	$(CC) $^ $(CFLAGS) $(LIBS) $(LDLIBS) -o $@
	$(STRIP) $@

clean:
	rm -f $(PROGS) *.o core *.eap