# Plug-In-Battery

This project is a modular ESP32-C6 bridge for:

- LAN / Ethernet via W5500
- MQTT communication with Victron
- Zigbee integration for Home Assistant / Zigbee2MQTT

Currently handled values:

Read-only (MQTT -> ESP -> Zigbee):
- Battery SoC
- Battery Voltage
- Battery Current
- Battery Power

Writable (Zigbee -> ESP -> MQTT):
- Grid Setpoint
- Max Feed-In Limit

The project is designed in a modular way to clearly separate Ethernet, MQTT, Zigbee, and global system state.



## 1) FILE STRUCTURE AND RESPONSIBILITIES

Recommended reading order:
1. config.h
2. state_store.h
3. state_store.cpp
4. eth_manager.h
5. eth_manager.cpp
6. mqtt_bridge.h
7. mqtt_bridge.cpp
8. zigbee_bridge.h
9. zigbee_bridge.cpp
10. plug_in_battery.ino



### 1. config.h
This is the central configuration file.

Contains:
- Ethernet pin configuration
- Static IP configuration
- MQTT host and port
- All MQTT topics
- Timing parameters
- Allowed limits (min/max)
- Zigbee endpoints

Summary:
config.h holds all system-wide constants and settings.

If you need to change topics, limits, timers, or pins, start here.


### 2. state_store.h
Defines the structure of the global system state.

Includes:
- TelemetryState
- WriteState
- MqttState
- ZigbeeCacheState
- AppState

Also declares helper functions:
- floatChanged(...)
- floatEqual(...)
- clampFloat(...)
- mqttValueStateText()
- writeStateText(...)

Summary:
Defines WHAT data exists and how it is structured.


### 3. state_store.cpp
Implements the global state.

Includes:
- Definition of global variable g_state
- Implementation of helper functions

Summary:
Implements the logic defined in state_store.h.


### 4. eth_manager.h
Header for Ethernet module.

Defines:
- ethAlive()
- connectLAN()

Summary:
Public interface for Ethernet handling.


### 5. eth_manager.cpp
Contains Ethernet logic.

Handles:
- SPI initialization
- W5500 startup
- Static IP configuration
- Ethernet events
- Link/IP monitoring
- MQTT disconnect on network loss

Summary:
Responsible for physical network connectivity.

### 6. mqtt_bridge.h
Header for MQTT module.

Defines:
- mqttSetup()
- ensureMqtt()
- mqttLoop()
- sendVictronKeepAlive()
- handlePendingWrites()
- mqttPublishFloatValue(...)
- trySendGridSetpoint()
- trySendMaxFeedIn()
- mqttForceDisconnect()
- mqttIsConnected()

Summary:
Public interface for MQTT operations.


### 7. mqtt_bridge.cpp
Contains all MQTT logic.

Handles:
- MQTT connection
- Topic subscriptions
- JSON parsing
- Updating system state from Victron
- Handling readbacks (actual values)
- Sending write commands
- Retry logic for pending writes
- Keepalive messages

Important:
"actual" values are always taken from MQTT readback.

Summary:
Core communication bridge to Victron.


### 8. zigbee_bridge.h
Header for Zigbee module.

Defines:
- connectZigbee()
- reportZigbeeValues(...)
- handleZigbeeFactoryResetButton()
- zigbeeIsConnected()

Summary:
Public interface for Zigbee functionality.


### 9. zigbee_bridge.cpp
Contains all Zigbee logic.

Handles:
- Endpoint creation
- Analog Input / Output setup
- Write callbacks (Grid Setpoint, Max Feed-In)
- Reporting values to Zigbee
- Reporting confirmed "actual" values
- Factory reset handling

Important:
Only confirmed values (actual) are reported back.

Summary:
Connects internal state to Zigbee2MQTT / Home Assistant.


### 10. plug_in_battery.ino 
Main file of the project.

Handles:
- System initialization
- Calling module functions
- Main loop execution
- Periodic tasks (MQTT, Zigbee, Keepalive)
- Debug output

Summary:
Acts as the orchestrator of the system.

Important:
Contains no business logic, only coordination.


## 2) EXISTING MQTT TOPICS

Three types of topics are used:

1. Read Topics
2. Readback Topics
3. Write Topics


### 1. Read Topics (Victron -> ESP -> Zigbee)

Battery Voltage
N/.../Dc/Battery/Voltage
-> Voltage in V

Battery Current
N/.../Dc/Battery/Current
-> Current in A

Battery Power
N/.../Dc/Battery/Power
-> Power in W

Battery SoC
N/.../Dc/Battery/Soc
-> State of Charge in %


### 2. Readback Topics (confirmation)

Grid Setpoint (actual)
N/.../Settings/CGwacs/AcPowerSetPoint

Max Feed-In (actual)
N/.../Settings/CGwacs/MaxFeedInPower

Important:
These are the confirmed values from Victron.


### 3. Write Topics (ESP -> Victron)

Grid Setpoint
W/.../Settings/CGwacs/AcPowerSetPoint

Max Feed-In
W/.../Settings/CGwacs/MaxFeedInPower

Used when values are changed via Zigbee.


### 4. Wake / Keepalive Topics


Used to keep Victron data alive:

R/.../Serial
R/.../keepalive


## 3) HOW TO SET LIMITS

Limits are defined in config.h:

#define GRID_SP_MIN   (-100.0f)
#define GRID_SP_MAX   ( 300.0f)

#define MAX_FEEDIN_MIN   (0.0f)
#define MAX_FEEDIN_MAX   (800.0f)

Example change:

#define GRID_SP_MIN   (-200.0f)
#define GRID_SP_MAX   ( 500.0f)

#define MAX_FEEDIN_MAX (1200.0f)

Important:
These limits apply everywhere:
- Zigbee input
- MQTT readback
- internal validation


## 4) ADDING NEW MQTT TOPICS

Depends on type:

A) Read-only
B) Writable


### 1. ADD READ-ONLY TOPIC

Steps:

1. Add topic in config.h
2. Add variable in state_store.h
3. Subscribe in mqtt_bridge.cpp
4. Parse value in mqtt_bridge.cpp
5. Add Zigbee endpoint (optional)
6. Add reporting in zigbee_bridge.cpp
7. Add debug output (optional)


### 2. ADD WRITABLE TOPIC

Steps:

1. Add Read + Write topics in config.h
2. Add min/max limits
3. Extend state_store.h (actual + requested + flags)
4. Subscribe to readback in mqtt_bridge.cpp
5. Implement write function
6. Add retry logic
7. Add Zigbee output endpoint
8. Implement write callback
9. Add debug output


## 5) IMPORTANT CONCEPT

Each writable value has:

requested
-> value requested by Zigbee / HA

actual
-> value confirmed via MQTT readback

Why:
- ensures reliability
- avoids false UI feedback
- enables retry logic


## 6) SUMMARY

config.h
-> configuration

state_store.*
-> system state

eth_manager.*
-> Ethernet

mqtt_bridge.*
-> MQTT communication

zigbee_bridge.*
-> Zigbee interface

plug_in_battery.ino
-> main control loop

