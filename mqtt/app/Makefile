PROG1	= basemqtt
OBJS1	= main.c ACAP.c cJSON.c MQTT.c CERTS.c
PROGS	= $(PROG1)

PKGS = glib-2.0 gio-2.0 vdostream axevent fcgi libcurl

CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))
LDLIBS  += -s -lm -ldl -laxparameter -lpthread
CFLAGS += -Wall -Wno-format-overflow

all:	$(PROGS)

$(PROG1): $(OBJS1)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -rf $(PROGS) *.o $(LIBDIR) *.eap* *_LICENSE.txt manifest.json package.conf* param.conf
