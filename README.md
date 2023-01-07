# make_acap
A collection of demo ACAP that utilize various services in Axis Cameras.  These can be used as a base for your ACAP.  
More information about ACAP SDK development can be found on [Axis Developer Community](https://www.axis.com/developer-community/acap).

All examples use simplified abstraction layers for common ACAP SDK services such as events, image capture, http, configuration and motion analytics data.  This lets you focus on what you want to create without the need to dig into the SDK.  You only need to focus on the main.c file.  Note that abstraction layer source files are in CAPITAL letters. All examples use cJSON to store and pass data.  All examples have a user interfaces to test/validate functionality using a web browser.

Examples ACAP
* Config - How to define and store dynamic configuration settings.
* Image - How to capture an image and respond a JPEG file through http GET.
* Events - How to define and fire various events (event, state and data)
* MQTT - How to send MQTT messages using [SIMQTT](https://api.aintegration.team/acap/simqtt?source=acapp) as a message proxy.  SIMQTT needs to be installed and connected to a broker.
* Motion - How to access motion data bounding-boxes.
* Tracker - Publishing motion tracker data on MQTT

If you use any of the examples as a base for your ACAP, make name changes in the following places
1. source/Makefile:  PROG = PACKAGE_NAME
2. source/APP.h: APP_PACKAGE "PACKAGE_NAME"
3. source/manfiest.json: "appname": "PACKAGE_NAME"

To build, go into each directory.  For a selected platform... 
```
docker build --build-arg ARCH=aarch64 --tag acap .
docker cp $(docker create acap):/opt/app ./build
```
or 
```
docker build --build-arg ARCH=armv7hf --tag acap .
docker cp $(docker create acap):/opt/app ./build
```
There is a simplified script that builds both platforms, extracts the EAP-files and removes the build directory.
```
. makeall.sh
```
