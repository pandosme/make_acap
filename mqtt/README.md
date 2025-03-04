# MQTT

Axis Devices have a built-in MQTT client. However, the ACAP SDK does not provide direct support to integrate with MQTT. This project provides an example of how to set up an MQTT client and allow users to configure broker connections.

Key points:

1. This demo uses the same MQTT libraries as the firmware client to create an additional MQTT client instance.
2. The user interface provides a way to publish messages.
3. To customize the application, you should mainly focus on `main.c`. Inspect this file to see how to publish messages.
4. The demo also demonstrates how to subscribe to MQTT messages.
5. Default MQTT settings are located in `app/settings/mqtt.json`.
6. For more details on the MQTT implementation, inspect the `MQTT.h` file.
