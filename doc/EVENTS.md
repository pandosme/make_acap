# AXIS Camera Events Documentation

This document provides a comprehensive listing of event types available across AXIS camera devices. Events are organized hierarchically by their namespace topics and include property definitions and state information.

## Event Structure

Each event is defined with:
- **Nice Name**: Human-readable event name
- **Namespace**: Hierarchical topic path (Topic0/Topic1/Topic2/Topic3)
- **State Type**: Stateful (maintains state) or Stateless (one-time trigger)
- **Source Properties**: Input/trigger parameters with data types
- **Data Properties**: Output/event data with data types

---

## Device Events (tns1:Device)

### Input/Output Events

#### Device > IO > Virtual Input
- **Nice Name**: Virtual input
- **Namespace**: `tns1:Device` / `tnsaxis:IO` / `VirtualInput`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `port` (xsd:int) - Port number
- **Data Properties**:
  - `active` (xsd:boolean, State) - Active status

#### Device > IO > Digital Input Port
- **Nice Name**: Digital input port
- **Namespace**: `tns1:Device` / `tnsaxis:IO` / `Port`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `port` (xsd:int) - Port number
- **Data Properties**:
  - `state` (xsd:boolean, State) - Active status

#### Device > IO > Manual Trigger
- **Nice Name**: Manual trigger
- **Namespace**: `tns1:Device` / `tnsaxis:IO` / `VirtualPort`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `port` (xsd:int) - Port number
- **Data Properties**:
  - `state` (xsd:boolean, State) - Active status

#### Device > IO > Digital Output Port
- **Nice Name**: Digital output port
- **Namespace**: `tns1:Device` / `tnsaxis:IO` / `OutputPort`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `port` (xsd:int) - Port number
- **Data Properties**:
  - `state` (xsd:boolean, State) - Active status

#### Device > Trigger > Digital Input
- **Nice Name**: Digital Input Trigger
- **Namespace**: `tns1:Device` / `Trigger` / `DigitalInput`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `InputToken` (tt:ReferenceToken) - Input token reference
- **Data Properties**:
  - `LogicalState` (xsd:boolean, State) - Logical state

#### Device > Trigger > Relay
- **Nice Name**: Relays and Outputs
- **Namespace**: `tns1:Device` / `Trigger` / `Relay`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `RelayToken` (tt:ReferenceToken) - Relay token reference
- **Data Properties**:
  - `LogicalState` (tt:RelayLogicalState, State) - Logical state

### Casing Events

#### Device > Casing > Open
- **Nice Name**: Casing Open
- **Namespace**: `tns1:Device` / `tnsaxis:Casing` / `Open`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `Name` (xsd:string) - Device name
- **Data Properties**:
  - `Open` (xsd:boolean, State) - Casing open status

### Light Events

#### Device > Light > Status
- **Nice Name**: Light Status
- **Namespace**: `tns1:Device` / `tnsaxis:Light` / `Status`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `id` (xsd:int) - Light ID
- **Data Properties**:
  - `state` (xsd:string, State) - Light state

### Hardware Failure Events

#### Device > Hardware Failure > Fan Failure
- **Nice Name**: Fan failure
- **Namespace**: `tns1:Device` / `tnsaxis:HardwareFailure` / `FanFailure`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `fan` (xsd:int) - Fan identifier
- **Data Properties**:
  - `fan_failure` (xsd:boolean, State) - Fan failure status

#### Device > Hardware Failure > Storage Failure
- **Nice Name**: Storage Failure
- **Namespace**: `tns1:Device` / `tnsaxis:HardwareFailure` / `StorageFailure`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `disk_id` (xsd:string) - Disk identifier (e.g., SD_DISK, NetworkShare)
- **Data Properties**:
  - `disruption` (xsd:boolean, State) - Storage disruption status

#### Device > Hardware Failure > Power Supply > PTZ Power Failure
- **Nice Name**: PTZ Power Failure
- **Namespace**: `tns1:Device` / `HardwareFailure` / `PowerSupplyFailure` / `tnsaxis:PTZPowerFailure`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `Token` (tt:ReferenceToken) - Reference token
- **Data Properties**:
  - `Failed` (xsd:boolean, State) - PTZ power failed status

### Fan Events

#### Device > Fan > Status
- **Nice Name**: Fan Status
- **Namespace**: `tns1:Device` / `tnsaxis:Fan` / `Status`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `fan` (xsd:int) - Fan identifier
- **Data Properties**:
  - `running` (xsd:boolean, State) - Fan running status

### System Message Events

#### Device > System Message > Action Failed
- **Nice Name**: Action failed
- **Namespace**: `tns1:Device` / `tnsaxis:SystemMessage` / `ActionFailed`
- **State**: Stateless (Client event)
- **Data Properties**:
  - `description` (xsd:string) - Failure description

### Temperature Events

#### Device > Status > Temperature > Above
- **Nice Name**: Above operating temperature
- **Namespace**: `tns1:Device` / `tnsaxis:Status` / `Temperature` / `Above`
- **State**: Stateful (Property-based)
- **Data Properties**:
  - `sensor_level` (xsd:boolean, State) - Above range indicator

#### Device > Status > Temperature > Below
- **Nice Name**: Below operating temperature
- **Namespace**: `tns1:Device` / `tnsaxis:Status` / `Temperature` / `Below`
- **State**: Stateful (Property-based)
- **Data Properties**:
  - `sensor_level` (xsd:boolean, State) - Below range indicator

#### Device > Status > Temperature > Inside
- **Nice Name**: Within operating temperature
- **Namespace**: `tns1:Device` / `tnsaxis:Status` / `Temperature` / `Inside`
- **State**: Stateful (Property-based)
- **Data Properties**:
  - `sensor_level` (xsd:boolean, State) - Within range indicator

#### Device > Status > Temperature > Above or Below
- **Nice Name**: Above or below operating temperature
- **Namespace**: `tns1:Device` / `tnsaxis:Status` / `Temperature` / `Above_or_below`
- **State**: Stateful (Property-based)
- **Data Properties**:
  - `sensor_level` (xsd:boolean, State) - Above or below range indicator

#### Device > Status > System Ready
- **Nice Name**: System Ready
- **Namespace**: `tns1:Device` / `tnsaxis:Status` / `SystemReady`
- **State**: Stateful (Property-based)
- **Data Properties**:
  - `ready` (xsd:boolean, State) - System ready status

### Heater Events

#### Device > Heater > Status
- **Nice Name**: Heater Status
- **Namespace**: `tns1:Device` / `tnsaxis:Heater` / `Status`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `heater` (xsd:int) - Heater identifier
- **Data Properties**:
  - `running` (xsd:boolean, State) - Heater running status

### MQTT Events

#### Device > MQTT > Client Status
- **Nice Name**: MQTT Client Status
- **Namespace**: `tns1:Device` / `tnsaxis:MQTT` / `ClientStatus`
- **State**: Stateful (Property-based)
- **Data Properties**:
  - `active` (xsd:boolean) - Active status
  - `connected` (xsd:boolean, State) - Connection status

### Network Events

#### Device > Network > Lost
- **Nice Name**: Network lost
- **Namespace**: `tns1:Device` / `tnsaxis:Network` / `Lost`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `interface` (xsd:string) - Network interface (e.g., Any, All, eth0)
- **Data Properties**:
  - `lost` (xsd:boolean, State) - Network lost status

#### Device > Network > Address Removed
- **Nice Name**: AddressRemoved
- **Namespace**: `tns1:Device` / `tnsaxis:Network` / `AddressRemoved`
- **State**: Stateless
- **Source Properties**:
  - `interface` (xsd:string) - Network interface
- **Data Properties**:
  - `origin` (xsd:string) - Address origin
  - `address` (xsd:string) - IP address

#### Device > Network > Address Added
- **Nice Name**: AddressAdded
- **Namespace**: `tns1:Device` / `tnsaxis:Network` / `AddressAdded`
- **State**: Stateless
- **Source Properties**:
  - `interface` (xsd:string) - Network interface
- **Data Properties**:
  - `origin` (xsd:string) - Address origin
  - `address` (xsd:string) - IP address

#### Device > Network > Blocked IP
- **Nice Name**: IP address blocked
- **Namespace**: `tns1:Device` / `tnsaxis:Network` / `BlockedIP`
- **State**: Stateless
- **Data Properties**:
  - `address` (xsd:string) - Blocked IP address

### Ring Power Events

#### Device > Ring Power Limit Exceeded
- **Nice Name**: Ring power overcurrent protection
- **Namespace**: `tns1:Device` / `tnsaxis:RingPowerLimitExceeded`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `input` (xsd:int) - Input identifier
- **Data Properties**:
  - `limit_exceeded` (xsd:boolean, State) - Limit exceeded status

### Audit Log Events

#### Device > Log > Audit
- **Nice Name**: Audit Log
- **Namespace**: `tns1:Device` / `tnsaxis:Log` / `Audit`
- **State**: Stateless
- **Data Properties**:
  - `class` (xsd:string) - Event class (e.g., Network Remediation Activity, Account Change, API Activity, Base Event, Network Activity, Authentication, Entity Management)
  - `message` (xsd:string) - Log message

---

## Light Control Events (tns1:LightControl)

#### Light Control > Light Status Changed > Status
- **Nice Name**: Light Status Changed
- **Namespace**: `tns1:LightControl` / `tnsaxis:LightStatusChanged` / `Status`
- **State**: Stateful (Property-based)
- **Data Properties**:
  - `state` (xsd:string, State) - Light state

---

## Video Source Events (tns1:VideoSource)

### Global Scene Change

#### Video Source > Global Scene Change > Imaging Service
- **Nice Name**: Global Scene Change
- **Namespace**: `tns1:VideoSource` / `GlobalSceneChange` / `ImagingService`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `Source` (tt:ReferenceToken) - Video source reference
- **Data Properties**:
  - `State` (xsd:boolean, State) - Scene change state

### Bitrate Events

#### Video Source > ABR
- **Nice Name**: Average bitrate degradation
- **Namespace**: `tns1:VideoSource` / `tnsaxis:ABR`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `VideoSourceConfigurationToken` (xsd:int) - Video source configuration token
- **Data Properties**:
  - `abr_error` (xsd:boolean, State) - ABR error status

### Day/Night Events

#### Video Source > Day Night Vision
- **Nice Name**: Day night vision
- **Namespace**: `tns1:VideoSource` / `tnsaxis:DayNightVision`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `VideoSourceConfigurationToken` (xsd:int) - Video source configuration token
- **Data Properties**:
  - `day` (xsd:boolean, State) - Day mode indicator

### Tampering Events

#### Video Source > Tampering
- **Nice Name**: Camera tampering
- **Namespace**: `tns1:VideoSource` / `tnsaxis:Tampering`
- **State**: Stateless
- **Source Properties**:
  - `channel` (xsd:int) - Channel number
- **Data Properties**:
  - `tampering` (xsd:int) - Tampering level

### Autofocus Events

#### Video Source > Autofocus
- **Nice Name**: Autofocus Completed
- **Namespace**: `tns1:VideoSource` / `tnsaxis:Autofocus`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `VideoSourceConfigurationToken` (xsd:int) - Video source configuration token
- **Data Properties**:
  - `focus` (xsd:double, State) - Focus value

### Live Stream Events

#### Video Source > Live Stream Accessed
- **Nice Name**: Live stream accessed
- **Namespace**: `tns1:VideoSource` / `tnsaxis:LiveStreamAccessed`
- **State**: Stateful (Property-based)
- **Data Properties**:
  - `accessed` (xsd:boolean, State) - Stream accessed status

### Motion Alarm

#### Video Source > Motion Alarm
- **Nice Name**: Motion Alarm
- **Namespace**: `tns1:VideoSource` / `MotionAlarm`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `Source` (tt:ReferenceToken) - Video source reference
- **Data Properties**:
  - `State` (xsd:boolean, State) - Motion detected state

---

## User Alarm Events (tns1:UserAlarm)

### Schedule Events

#### User Alarm > Recurring > Interval
- **Nice Name**: Scheduled event
- **Namespace**: `tns1:UserAlarm` / `tnsaxis:Recurring` / `Interval`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `id` (xsd:string) - Schedule ID (e.g., com.axis.schedules.after_hours, com.axis.schedules.office_hours, com.axis.schedules.weekends, com.axis.schedules.weekdays)
- **Data Properties**:
  - `active` (xsd:boolean, State) - Schedule active status

#### User Alarm > Recurring > Pulse
- **Nice Name**: Recurring pulse
- **Namespace**: `tns1:UserAlarm` / `tnsaxis:Recurring` / `Pulse`
- **State**: Stateless
- **Source Properties**:
  - `id` (xsd:string) - Schedule ID

---

## PTZ Controller Events (tns1:PTZController)

#### PTZ Controller > PTZ Ready
- **Nice Name**: PTZ ready
- **Namespace**: `tns1:PTZController` / `tnsaxis:PTZReady`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `channel` (xsd:int) - Channel number
- **Data Properties**:
  - `ready` (xsd:boolean, State) - PTZ ready status

#### PTZ Controller > PTZ Error
- **Nice Name**: PTZ error
- **Namespace**: `tns1:PTZController` / `tnsaxis:PTZError`
- **State**: Stateless
- **Source Properties**:
  - `channel` (xsd:int) - Channel number
- **Data Properties**:
  - `ptz_error` (xsd:string) - Error description

---

## Storage Events (tnsaxis:Storage)

#### Storage > Alert
- **Nice Name**: Storage alert
- **Namespace**: `tnsaxis:Storage` / `Alert`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `disk_id` (xsd:string) - Disk identifier (e.g., SD_DISK, NetworkShare)
- **Data Properties**:
  - `temperature` (xsd:int) - Temperature value
  - `alert` (xsd:boolean, State) - Alert status
  - `wear` (xsd:int) - Wear level
  - `overall_health` (xsd:int) - Overall health value

#### Storage > Disruption
- **Nice Name**: Storage disruption
- **Namespace**: `tnsaxis:Storage` / `Disruption`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `disk_id` (xsd:string) - Disk identifier (e.g., SD_DISK, NetworkShare)
- **Data Properties**:
  - `disruption` (xsd:boolean, State) - Disruption status

#### Storage > Recording
- **Nice Name**: Recording ongoing
- **Namespace**: `tnsaxis:Storage` / `Recording`
- **State**: Stateful (Property-based)
- **Data Properties**:
  - `recording` (xsd:boolean, State) - Recording status

---

## Audio Source Events (tns1:AudioSource)

#### Audio Source > Trigger Level
- **Nice Name**: Audio detection
- **Namespace**: `tns1:AudioSource` / `tnsaxis:TriggerLevel`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `channel` (xsd:int) - Channel number
  - `AudioSource` (xsd:string) - Audio source identifier
- **Data Properties**:
  - `triggered` (xsd:boolean, State) - Trigger status (above alarm level)

---

## Audio Control Events (tnsaxis:AudioControl)

#### Audio Control > Digital Signal Status
- **Nice Name**: Digital signal status
- **Namespace**: `tnsaxis:AudioControl` / `DigitalSignalStatus`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `input` (xsd:int) - Input number
- **Data Properties**:
  - `signal_status` (xsd:string, State) - Signal status

#### Audio Control > Digital Signal Status Invalid
- **Nice Name**: Digital signal has invalid sample rate
- **Namespace**: `tnsaxis:AudioControl` / `DigitalSignalStatusInvalid`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `input` (xsd:int) - Input number
- **Data Properties**:
  - `signal_status_invalid` (xsd:boolean, State) - Invalid sample rate status

#### Audio Control > Digital Signal Status No Signal
- **Nice Name**: Digital signal missing
- **Namespace**: `tnsaxis:AudioControl` / `DigitalSignalStatusNoSignal`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `input` (xsd:int) - Input number
- **Data Properties**:
  - `signal_status_no_signal` (xsd:boolean, State) - No signal status

#### Audio Control > Digital Signal Status Metadata
- **Nice Name**: Audio metadata received
- **Namespace**: `tnsaxis:AudioControl` / `DigitalSignalStatusMetadata`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `input` (xsd:int) - Input number
- **Data Properties**:
  - `audio_metadata` (xsd:boolean, State) - Metadata present status

#### Audio Control > Digital Signal Status OK
- **Nice Name**: Digital signal OK
- **Namespace**: `tnsaxis:AudioControl` / `DigitalSignalStatusOK`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `input` (xsd:int) - Input number
- **Data Properties**:
  - `signal_status_ok` (xsd:boolean, State) - Signal OK status

---

## Audio Classification Events (tnsaxis:AudioClassification)

#### Audio Classification > Speech
- **Nice Name**: Speech
- **Namespace**: `tnsaxis:AudioClassification` / `Speech`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `AudioSource` (xsd:string) - Audio source identifier
- **Data Properties**:
  - `Detected` (xsd:boolean, State) - Speech detected

#### Audio Classification > Shout
- **Nice Name**: Shout
- **Namespace**: `tnsaxis:AudioClassification` / `Shout`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `AudioSource` (xsd:string) - Audio source identifier
- **Data Properties**:
  - `Detected` (xsd:boolean, State) - Shout detected

#### Audio Classification > Glass Break
- **Nice Name**: Glass Break
- **Namespace**: `tnsaxis:AudioClassification` / `GlassBreak`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `AudioSource` (xsd:string) - Audio source identifier
- **Data Properties**:
  - `Detected` (xsd:boolean, State) - Glass break detected

#### Audio Classification > Screaming
- **Nice Name**: Screaming
- **Namespace**: `tnsaxis:AudioClassification` / `Screaming`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `AudioSource` (xsd:string) - Audio source identifier
- **Data Properties**:
  - `Detected` (xsd:boolean, State) - Screaming detected

---

## Sound Pressure Level Events (tnsaxis:SoundPressureLevel)

#### Sound Pressure Level > Above Threshold Upper
- **Nice Name**: Above Threshold Upper
- **Namespace**: `tnsaxis:SoundPressureLevel` / `AboveThresholdUpper`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `AudioSource` (xsd:string) - Audio source identifier
- **Data Properties**:
  - `MaxSPL` (xsd:double) - Maximum SPL value
  - `Detected` (xsd:boolean, State) - Above upper threshold

#### Sound Pressure Level > Below Threshold Lower
- **Nice Name**: Below Threshold Lower
- **Namespace**: `tnsaxis:SoundPressureLevel` / `BelowThresholdLower`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `AudioSource` (xsd:string) - Audio source identifier
- **Data Properties**:
  - `Detected` (xsd:boolean, State) - Below lower threshold
  - `MinSPL` (xsd:double) - Minimum SPL value

#### Sound Pressure Level > Summary
- **Nice Name**: Summary
- **Namespace**: `tnsaxis:SoundPressureLevel` / `Summary`
- **State**: Stateless (Application data)
- **Source Properties**:
  - `AudioSource` (xsd:string) - Audio source identifier
- **Data Properties**:
  - `MaxSPL` (xsd:double) - Maximum SPL
  - `LEQ` (xsd:double) - Equivalent continuous sound level
  - `MinSPL` (xsd:double) - Minimum SPL

---

## Adaptive Audio Detection Events (tnsaxis:AdaptiveAudioDetection)

#### Adaptive Audio Detection > Detected
- **Nice Name**: Detected
- **Namespace**: `tnsaxis:AdaptiveAudioDetection` / `Detected`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `AudioSource` (xsd:string) - Audio source identifier
- **Data Properties**:
  - `Detected` (xsd:boolean, State) - Audio detected

---

## Camera Application Platform Events (tnsaxis:CameraApplicationPlatform)

### Node-RED Events

#### Camera Application Platform > Node-RED > Event
- **Nice Name**: Node-RED:Event
- **Namespace**: `tnsaxis:CameraApplicationPlatform` / `Node-RED` / `event`
- **State**: Stateless
- **Data Properties**:
  - `value` (xsd:int) - Event value

#### Camera Application Platform > Node-RED > State
- **Nice Name**: Node-RED:State
- **Namespace**: `tnsaxis:CameraApplicationPlatform` / `Node-RED` / `state`
- **State**: Stateful (Property-based)
- **Data Properties**:
  - `state` (xsd:boolean, State) - State value

#### Camera Application Platform > Node-RED > Data
- **Nice Name**: Node-RED:Data
- **Namespace**: `tnsaxis:CameraApplicationPlatform` / `Node-RED` / `data`
- **State**: Stateless
- **Data Properties**:
  - `data` (xsd:string) - Data payload

### Timelapse Events

#### Camera Application Platform > Timelapse2 > Sun Noon
- **Nice Name**: Sun noon
- **Namespace**: `tnsaxis:CameraApplicationPlatform` / `timelapse2` / `sunnoon`
- **State**: Stateless

### Object Analytics Events

#### Camera Application Platform > Object Analytics > Human
- **Nice Name**: Object Analytics: Human
- **Namespace**: `tnsaxis:CameraApplicationPlatform` / `ObjectAnalytics` / `Device1Scenario1`
- **State**: Stateful (Property-based)
- **Data Properties**:
  - `triggerTime` (xsd:string) - Trigger timestamp
  - `scenarioType` (xsd:string) - Scenario type
  - `classTypes` (xsd:string) - Object class types
  - `objectId` (xsd:string) - Object identifier
  - `active` (xsd:boolean, State) - Active status

#### Camera Application Platform > Object Analytics > Any Scenario
- **Nice Name**: Object Analytics: Any Scenario
- **Namespace**: `tnsaxis:CameraApplicationPlatform` / `ObjectAnalytics` / `Device1ScenarioANY`
- **State**: Stateful (Property-based)
- **Data Properties**:
  - `triggerTime` (xsd:string) - Trigger timestamp
  - `classTypes` (xsd:string) - Object class types
  - `objectId` (xsd:string) - Object identifier
  - `active` (xsd:boolean, State) - Active status

#### Camera Application Platform > Object Analytics > Internal Data
- **Nice Name**: xinternal_data
- **Namespace**: `tnsaxis:CameraApplicationPlatform` / `ObjectAnalytics` / `xinternal_data`
- **State**: Stateless (Application data)
- **Data Properties**:
  - `svgframe` (xsd:string) - SVG frame data

### DataQ Events

#### Camera Application Platform > DataQ > Anomaly
- **Nice Name**: DataQ: Anomaly
- **Namespace**: `tnsaxis:CameraApplicationPlatform` / `DataQ` / `anomaly`
- **State**: Stateful (Property-based)
- **Data Properties**:
  - `state` (xsd:boolean, State) - Anomaly state

---

## External MQTT Events (tnsaxis:MQTT)

#### MQTT > Message > Stateless
- **Nice Name**: Stateless Event
- **Namespace**: `tnsaxis:MQTT` / `Message` / `Stateless`
- **State**: Stateless
- **Source Properties**:
  - `device-prefix` (xsd:boolean) - Device prefix flag
  - `mqtt-topic` (xsd:string) - MQTT topic
- **Data Properties**:
  - `mqtt-payload` (xsd:string) - MQTT payload

---

## Recording Config Events (tns1:RecordingConfig)

#### Recording Config > Create Recording
- **Nice Name**: Create Recording
- **Namespace**: `tns1:RecordingConfig` / `CreateRecording`
- **State**: Stateless
- **Source Properties**:
  - `RecordingToken` (xsd:string) - Recording token

#### Recording Config > Delete Recording
- **Nice Name**: Delete Recording
- **Namespace**: `tns1:RecordingConfig` / `DeleteRecording`
- **State**: Stateless
- **Source Properties**:
  - `RecordingToken` (xsd:string) - Recording token

#### Recording Config > Recording Configuration
- **Nice Name**: Recording Configuration
- **Namespace**: `tns1:RecordingConfig` / `RecordingConfiguration`
- **State**: Stateless
- **Source Properties**:
  - `RecordingToken` (xsd:string) - Recording token
- **Data Properties**:
  - `Configuration` (xsd:string) - Recording configuration (ONVIF element)

#### Recording Config > Track Configuration
- **Nice Name**: Track Configuration
- **Namespace**: `tns1:RecordingConfig` / `TrackConfiguration`
- **State**: Stateless
- **Source Properties**:
  - `RecordingToken` (xsd:string) - Recording token
  - `TrackToken` (xsd:string) - Track token
- **Data Properties**:
  - `Configuration` (xsd:string) - Track configuration (ONVIF element)

#### Recording Config > Recording Job Configuration
- **Nice Name**: Recording Job Configuration
- **Namespace**: `tns1:RecordingConfig` / `RecordingJobConfiguration`
- **State**: Stateless
- **Source Properties**:
  - `RecordingJobToken` (xsd:string) - Recording job token
- **Data Properties**:
  - `Configuration` (xsd:string) - Recording job configuration (ONVIF element)

---

## Media Events (tns1:Media)

#### Media > Profile Changed
- **Nice Name**: Profile Changed
- **Namespace**: `tns1:Media` / `ProfileChanged`
- **State**: Stateless (Application data)
- **Source Properties**:
  - `Token` (xsd:string) - Profile token

#### Media > Configuration Changed
- **Nice Name**: Configuration Changed
- **Namespace**: `tns1:Media` / `ConfigurationChanged`
- **State**: Stateless (Application data)
- **Source Properties**:
  - `Token` (xsd:string) - Configuration token
  - `Type` (xsd:string) - Configuration type

---

## Air Quality Monitor Events (tnsaxis:AirQualityMonitor)

#### Air Quality Monitor > Vaping
- **Nice Name**: Vaping or smoking detected
- **Namespace**: `tnsaxis:AirQualityMonitor` / `Vaping`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `sensor_name` (xsd:string) - Sensor name
- **Data Properties**:
  - `active` (xsd:boolean, State) - Active status

#### Air Quality Monitor > Alarm
- **Nice Name**: Air quality outside acceptable range
- **Namespace**: `tnsaxis:AirQualityMonitor` / `Alarm`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `source` (xsd:string) - Measurement source (e.g., AQI, CO2, NOx, VOC, Humidity, Temperature, PM10.0, PM4.0, PM2.5, PM1.0)
  - `sensor_name` (xsd:string) - Sensor name
- **Data Properties**:
  - `active` (xsd:boolean, State) - Active status

#### Air Quality Monitor > Metadata
- **Nice Name**: Air quality monitoring active
- **Namespace**: `tnsaxis:AirQualityMonitor` / `Metadata`
- **State**: Stateless (Application data)
- **Source Properties**:
  - `sensor_name` (xsd:string) - Sensor name
- **Data Properties**:
  - `VOC` (xsd:double) - Volatile Organic Compounds
  - `PM10.0` (xsd:double) - Particulate Matter 10.0
  - `PM1.0` (xsd:double) - Particulate Matter 1.0
  - `Humidity` (xsd:double) - Humidity level
  - `AQI` (xsd:int) - Air Quality Index
  - `NOx` (xsd:double) - Nitrogen Oxides
  - `PM4.0` (xsd:double) - Particulate Matter 4.0
  - `Temperature` (xsd:double) - Temperature
  - `PM2.5` (xsd:double) - Particulate Matter 2.5
  - `CO2` (xsd:int) - Carbon Dioxide

---

## Media Clip Events (tnsaxis:MediaClip)

#### Media Clip > Playing
- **Nice Name**: Playing
- **Namespace**: `tnsaxis:MediaClip` / `Playing`
- **State**: Stateful (Property-based)
- **Source Properties**:
  - `AudioOutput` (xsd:int) - Audio output number
- **Data Properties**:
  - `Playing` (xsd:boolean, State) - Playing status

#### Media Clip > Currently Playing
- **Nice Name**: Currently Playing Media Clip
- **Namespace**: `tnsaxis:MediaClip` / `CurrentlyPlaying`
- **State**: Stateless
- **Source Properties**:
  - `AudioOutput` (xsd:int) - Audio output number
- **Data Properties**:
  - `FileName` (xsd:string) - File name

---

## Event State Classification

### Stateful Events
Stateful events maintain a persistent state and are marked with `aev:isProperty="true"`. These events have properties marked with `isPropertyState="true"` that represent the current state. Stateful events are triggered when the state changes and can be used to track ongoing conditions.

**Key Indicators:**
- Contains `aev:isProperty="true"`
- Data properties include `isPropertyState="true"`
- Used for monitoring continuous states (e.g., temperature, motion, connection status)

### Stateless Events
Stateless events are one-time notifications that occur at a specific moment. They do not maintain state between occurrences and are typically used for discrete actions or alerts.

**Key Indicators:**
- Does not contain `aev:isProperty="true"` OR contains `client-event="true"` OR `isApplicationData="true"`
- No `isPropertyState` properties
- Used for momentary notifications (e.g., action failed, address added, recording created)

---

## Property Types Reference

### Common XSD Types
- `xsd:boolean` - Boolean value (true/false)
- `xsd:int` - Integer number
- `xsd:double` - Double-precision floating point number
- `xsd:string` - String/text value

### ONVIF Types
- `tt:ReferenceToken` - Reference token for ONVIF resources
- `tt:RelayLogicalState` - Relay state enumeration

### Special Attributes
- `isPropertyState="true"` - Indicates a stateful property
- `onvif-element="true"` - ONVIF XML element structure
- `client-event="true"` - Client-side event
- `isApplicationData="true"` - Application-level data

---

## Usage Notes for LLMs

1. **Topic Hierarchy**: Events follow a hierarchical namespace structure with up to 4 levels (Topic0/Topic1/Topic2/Topic3)
2. **State Detection**: Check for `aev:isProperty="true"` and `isPropertyState="true"` to determine if an event is stateful
3. **Property Location**: Source properties define input/trigger parameters, Data properties define output/event data
4. **Type Information**: All properties include explicit type information using XSD or ONVIF type definitions
5. **Device Variations**: Different camera models may support different subsets of these events
6. **Value Constraints**: Some properties include predefined value sets (enumerated in the original declarations)
