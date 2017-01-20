##FritzBoxPlugin

The FritzBoxPlugin can control smart home devices connected to an AVM FRITZ!Box. Supported devices are:

- FRITZ!DECT200 remote controllable power socket, power and energy meter

An instance of a FritzBoxPlugin connects to exactly one FRITZ!Box. You have to use a separate plugin node for each FRITZ!Box if you use more than one. One FritzBoxPlugin instance can use an arbitrary number of devices (actors/sensors). Each device in turn defines certain ports that can be used to interact with the device.

Device status is queried in regular intervals. If a device becomes unavailable or a communication error occurs its ports will signal an error. You can expect some delays when changing state or reading values; these devices are not suitable for fast switching or real-time monitoring. Additional delays may be introduced by communication between the FRITZ!Box and devices; naturally, this plugin cannot control these delays. For this reason device control messages are sent asynchronously: Incoming requests for state changes etc. are placed in a queue and processed sequentially. If you fill the queue faster than it can be processed (which may happen, for example, if you connect a Switch to a fast Pulse), it will most likely result in the openhatd process consuming more and more memory until the server stops responding. 

This plugin uses the HTTP web service API exposed by FRITZ!Boxes. Interacting with a FRITZ!Box requires logging in with a user that has the proper permissions (set on the FRITZ!Box administration web site). Login is performed using the challenge-response protocol defined by current FRITZ!Box operating systems. This plugin does not support HTTPS, so all communication is unencrypted. It is not recommended to use this over public networks.

### Plugin settings

#### Host  
Required. The host name or IP address of the FRITZ!Box.

#### Port
Optional. The web service port. Defaults to 80.

#### User
Required. The user name  to use for the login.

#### Password
Required. The password to use for the login.

#### Timeout
Optional. Timeout setting for communications in seconds. Defaults to 5 seconds (this assumes a fast response if both devices are in the same local network).

#### Group
Optional. The default group to use for the device ports. Can be overridden by the device port specification (see below).

#### LogVerbosity
Optional. The default log verbosity to use for the device ports. Can be overridden by the device port specification (see below).

### Device definition

Devices are specified in device nodes. Suppose a FritzBoxPlugin instance is defined like this:

	[FritzBox1]
	Type = Plugin
	Driver = FritzBoxPlugin
	... more settings

The plugin assumes that its devices are to be configured in the `Devices` sub-node. Devices (actors/sensors) are to be specified with their order of evaluation:

	[FritzBox1.Devices]
	DECT200_1 = 1
	DECT200_2 = 2

If the order number is negative the device will not be used.

Each actor/sensor specification requires a node on the same configuration level as the FritzBoxPlugin:

	[DECT200_1]
	Type = FritzDECT200
	AIN = ... 
	QueryInterval = 30

Currently, only the type `FritzDECT200` is supported.

#### AIN

The (required) AIN setting specifies the Actor Identification Number of the device. It can be obtained from the FRITZ!Box's administration web site.

#### QueryInterval

The query interval specifies the time between update queries in seconds. This setting affects all ports of the device. The default is 30 seconds.

### FRITZ!DECT200 

A node of type FritzDECT200 defines three ports: a switch port (Digital), a power port (Dial) and an energy port (Dial). The switch port is defined in the `_Switch` node:

	[DECT200_1_Switch]

The power port is defined in the '_Power' node:

	[DECT200_1_Power] 

The energy port is defined in the `_Energy` node:

	[DECT200_1_Energy]  

Note that these nodes are not configuration sub-sections (separated by a dot). The reason for this is that the names of these nodes will result in port IDs which should not contain special characters.

All three ports will be created, regardless of whether the configuration sections are present. You can override the default settings of these ports in their configuration sections.

It is not necessary to specify the `Type` of these nodes as it is set automatically. All ports are initialized with the `Group` and `LogVerbosity` settings inherited from the plugin main node. Individual `Group` and `LogVerbosity` specifications can override these settings.

#### Switch port
The switch port reflects the current switch state of the FRITZ!DECT200. It can be used to switch the actor on and off.

Default settings:

	Mode = Output
	Icon = powersocket
	
### Power port
The power port contains the current power consumption measurement. It is measured in milliwatts (mW). As the FRITZ!DECT200 is rated for up to 2300 W its max value is 2300000. A power port is readonly by default.

Default settings:

	Unit = electricPower_mW
	Icon = powermeter

#### Energy port
The energy port reflects the total energy consumption since starting or resetting the DECT200. It is measured in watt hours (Wh). The max value is 2147483647. An energy port is readonly by default.

Default settings:

	Unit = electricEnergy_Wh
	Icon = energymeter

### Example configuration

{!plugins/automation/FritzBoxPlugin/example.ini!}