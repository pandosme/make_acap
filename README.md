# make_acap
A collection of demo ACAP that utilize various services in Axis Cameras.  These can be used as a base for your ACAP.  More information about ACAP SDK development can be found on [Axis Developer Community](https://www.axis.com/developer-community/acap).

All examples ACAP uses simplified abstraction layers for common ACAP SDK services such as events, http end points, configuration and motion analytics data.  This lets you focus on what you want create without digging into the SDK.  You only need to focus on the main.c file.  Note that abstraction layer source files are in CAPITAL letters. The examples uses cJSON to store and pass data.  All ACAP have user interfaces to test/validate functionaluty with a web browser.

Examples ACAP
* Config - How to define define and store dynaic configuration parameters without using the camera parameter handler.
* Image - How to capture an camera image and respond a JPEG file through http GET.
* Events - How an acap can define and fire various events (event, state and data)
* MQTT - How an ACAP can send MQTT messages using [SIMQTT](https://pandosme.github.io/acap/mqtt/component/2021/10/18/simqtt.html) as a message proxy.  SIMQTT needs to be installed and connected to a broker
* Motion - How to get motion data bounding-boxes and send them on MQTT (via SIMQTT)

If you use any of the examples as a base for your ACAP, make name changes in the following places
1. source/Makefile:  PROG = PACKAGE_NAME
2. source/APP.h: APP_PACKAGE "PACKAGE_NAME"
3. source/manfiest.json: "appname": "PACKAGE_NAME"

To build for a selected platform: 
docker build --build-arg ARCH=aarch64 --tag acap .
docker cp $(docker create acap):/opt/app ./build

or 

docker build --build-arg ARCH=armv7hf --tag acap .
docker cp $(docker create acap):/opt/app ./build

There is a simplified script that builds both platforms, extracts the EAP-files and removes the build directory.
. makeall.sh


