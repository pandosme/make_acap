# Make ACAP
Working with ACAP SDK from scratch is not simple.  These examples uses abstract layer code (ACAP.c and ACAP.h) that can be used as templates deveopling ACAP services.  Separating the code from the SDK API will also make it easier to manage future (breaking) changes.
The goal is to let you focus on the use case and not batteling the SDK API.

## Base
The standard base for majority ACAP including
- HTTP (based on fastgci)
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
git clone 
