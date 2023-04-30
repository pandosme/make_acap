PROG	= motiondetection
OBJS1   = main.c cJSON.c APP.c FILE.c HTTP.c DEVICE.c STATUS.c MotionDetection.c

PROGS	= $(PROG)

PKGS	= glib-2.0 gio-2.0 axhttp

CFLAGS  += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS  += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))
LDLIBS  += -Wl,-ldl
LDLIBS += -s -lm -laxparameter -lscene

all:	$(PROGS)

$(PROG): $(OBJS1)
	$(CC) $^ $(CFLAGS) $(LIBS) $(LDLIBS) -o $@
	$(STRIP) $@

clean:
	rm -f $(PROGS) *.o core *.eap

