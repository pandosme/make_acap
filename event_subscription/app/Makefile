PROG1	= event_subscription
OBJS1	= main.c ACAP.c cJSON.c
PROGS	= $(PROG1)

PKGS = glib-2.0 gio-2.0 vdostream axevent fcgi

CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))
LDLIBS  += -s -lm -ldl -laxparameter -lpthread

all:	$(PROGS)

$(PROG1): $(OBJS1)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -rf $(PROGS) *.o $(LIBDIR) *.eap* *_LICENSE.txt manifest.json package.conf* param.conf
