# Tracker

Tracker monitors objects in motion and publish motion data on MQTT when an object has moved more than 5% of the camera view.  This reduces the amount of motion data 
that a consumer needs to process while providing sufficient data for majority of motion-based use cases.  
[SIMQTT](https://api.aintegration.team/acap/simqtt?source=acapp) acts as a message proxy for Tracker and must be installed and connected to a broker.  

Tracker provides a user interface for configuration and validation.  Note that the motion visualization is only updated every 500ms and may not present all object published on MQTT.

The tracker data includes the following properties
```
{
	"id": number,			Unique object ID,
	"active": bool,			True = still tracking, False = Object tracking lost/dead
	"timestamp": number,	Last position timestamp (EPOCH milli seconds)
	"birth": number,		Birth timestamp (EPOCH milli seconds)
	"x": 0-1000,
	"y": 0-1000,
	"w": 0-1000,
	"h": 0-1000,
	"cx": 0-1000,			Center of gravity.  Placement depends on configuration
	"cy": 0-1000,			Either in the center of the box or bottom-middle (default)
	"bx": 0-1000,			Birth X (cx)
	"by": 0-1000,			Birth Y (cy)
	"px": 0-1000,			Previous cx
	"py": 0-1000,			Previous cy
	"dx": 0-1000,			Total delta x movement from birth ( positive = right, negative = left )
	"dy": 0-1000,			Total delta y movement from birth ( positive = down, negative = up )
	"distance": number,		Total percent movment of camer view.  Can be more than 100% if objects moves around
	"age": number			Seconds between birth and death
}
```
The coordinate system is [0,0]-[1000,1000] with origo at top-left corner.   

Under /nodered there is an example flow for Node-RED how to visualize tracker data in a dashboard.
