PROG	= config
OBJS1	= main.c APP.c cJSON.c HTTP.c FILE.c DEVICE.c STATUS.c

PROGS	= $(PROG)

PKGS =  glib-2.0 gio-2.0 axhttp

CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))
LDLIBS += -s -lm -laxparameter 

all:	$(PROGS)

$(PROG): $(OBJS1)
	$(CC) $^ $(CFLAGS) $(LIBS) -lm $(LDLIBS) -o $@
	$(STRIP) $@

clean:
	rm -f $(PROGS) *.o core *.eap