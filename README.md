# Make ACAP
Working with ACAP SDK from scratch is not simple.  The goal is to let you focus on the use case and not batteling the SDK API.  
The examples uses abstract layer code (ACAP.c and ACAP.h) that simplifies interaction with SDK services.  By separating your from the SDK API will make it easier to manage future (breaking) changes in the ACAP SDK.

## Base
The standard base for majority ACAP including
- HTTP (based on fastgci) incuding GET, POST and file transfer
- Image capture
- Managing ACAP configuration parameters
- Decalreing and Fire events
- Web Interface with video streaming capabilities

## Base Event Subscription
- Simple declarations of events to be subscribed to
- Simple callback to process events

## Base MQTT
- An MQTT using the same MQTT libraries as the Axis Device MQTT client
- Does not impact the device MQTT client
- Simple configuration
- Simple Publish and Subscribe functions

# Build
Clone Repository
```
git clone https://github.com/pandosme/make_acap.git
```
Select template project to work with
```
cd base
```
Compile/Build
```
. build.sh
```
Customize by updating
- app/main.c
- app/mainfest.json
- Makefile
- app/html/config/settings.json
- app/html/config/sevents.json
- app/html/index.html
