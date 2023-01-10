PROG	= acapp_class
OBJS1   = main.c cJSON.c APP.c FILE.c HTTP.c DEVICE.c STATUS.c CLASSIFICATION.c video_object_detection.pb-c.c protobuf-c.c

PROGS	= $(PROG)

PKGS	= glib-2.0 gio-2.0 axhttp video-object-detection-subscriber libprotobuf-c gio-unix-2.0

CFLAGS  += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS  += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))
LDLIBS  += -Wl,-ldl
LDLIBS += -s -lm -laxparameter -lvideo-object-detection-subscriber

all:	$(PROGS)

$(PROG): $(OBJS1)
	$(CC) $^ $(CFLAGS) $(LIBS) $(LDLIBS) -o $@
	$(STRIP) $@

clean:
	rm -f $(PROGS) *.o core *.eap

