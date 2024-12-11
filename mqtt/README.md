# MQTT

Axis Device have a built-in MQTT clients.  There is no way to integrate this.  MQTT is a very simple way to integrate to other systems and services that a VMS via ONVIF events.
By using the MQTT libraries in the devices one can make a new instance of an MQTT client.   This project gives an example how to setup an MQTT client and let user configure broker connection.  The user interface also provides a a user interface to publish messages.
Inspect main.c how to publish messages.  To customize you mainly need to focus on main.c.
The demo also demonstrates how to subscribe to MQTT messages.  
Default MQTT settings are located in app/settings/mqtt.json
Inspect MQTT.h.
