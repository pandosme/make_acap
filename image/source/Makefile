PROG	= acapp_image
OBJS1   = main.c cJSON.c APP.c HTTP.c FILE.c

PROGS	= $(PROG)

PKGS	= glib-2.0 gio-2.0 axhttp vdostream

CFLAGS  += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS  += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))
LDLIBS  += -Wl,-ldl

all:	$(PROGS)

$(PROG): $(OBJS1)
	$(CC) $^ $(CFLAGS) $(LIBS) -lm $(LDLIBS) -o $@
	$(STRIP) $@

clean:
	rm -f $(PROGS) *.o core *.eap

