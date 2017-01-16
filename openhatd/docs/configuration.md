# Configuration Details

This document explains how openhatd processes configuration files. It also covers the details about the common configuration sections of an openhatd main configuration file, and common settings of ports. Information about how advanced ports or plugins are configured is provided with their respective documentation.

Please also see [general information about configuration files](concepts.md#configuration). 

## Configuration Parameters<a name="parameters"></a>

When opening a configuration file openhatd performs a case-sensitive text substitution of certain placeholders in the file. These placeholders have the form `$NAME` with NAME being the parameter name. By convention these should be all uppercase. openhatd provides the following parameters:

1. Environment variables (e. g. `HOSTNAME`)
2. Command line parameters specified using the `-p` option  (Example: `-p NAME=value`)
3. Internally provided parameters, these are:
	1. `OPENHAT_VERSION`: The version of openhatd at compile time in format MAJOR.MINOR.PATCH
	2. `DATETIME`: The startup timestamp in format %Y-%m-%d %H:%M:%S.%i
	3. `LOG_DATETIME`: The startup timestamp in format %Y%m%d_%H%M%S, useful for creating log file names
	4. `CWD`: The absolute path of the current working directory
4. When including configuration files, all parameters that are specified with the include directive (see below).

Placeholders that have no corresponding parameter are not replaced. In [log verbosity](concepts.md#logVerbosity) Debug and Extreme all access to configuration settings is logged. This allows for easy inspection of actual settings values at runtime.

## Including Configuration Files<a name="includes"></a>

Configuration files are included by specifying `Type = Include`. An include node must contain the `Filename` setting. Relative paths are assumed to be relative to the current configuration file. This can be overridden using the `RelativeTo` setting (see "Relative paths" below).

An include node has an associated parameters node that is required to have the name `<IncludeNode>.Parameters`. The `Key = Value` pairs in this node are provided to the include file as parameters (see above). Example:

	[Root]
	...
	IncludeNode1 = 9999

	[IncludeNode1]
	Type = Include
	Filename = MyInclude.ini

	[IncludeNode1.Parameters]
	MyParam1 = MyValue1 
	MyParam2 = MyValue2 

In `MyInclude.ini`, all occurrences of the string `$MyParam1` will be replaced by `MyValue1`, and `$MyParam2` by `MyValue2` correspondingly.  

An included configuration file must have its own `Root` node which is evaluated just like the `Root` node of the main configuration file. Included files can in turn include other files. If you include a file recursively openhatd will enter an infinite loop.

Include files are meant to avoid repeating common combinations of ports that differ only in IDs, labels and other minor parameters. One use case is to have a Select port together with an Exec port that triggers a shell script when the selection changes. If you need more than one combination like this it may make sense to outsource them to an include file. You can also employ another neat trick which involves making the include file executable (on Linux) and code the script to be executed at the beginning of the include file. This keeps code and configuration together. The following example accepts the parameters `$Name`, `$Label`, `$Group` and `$Command` (note that `$1`, `$2`, `$DEVICE` and `$COMMAND` are shell script variables; these are not replaced unless they are present in the environment or the include file parameters, so take care how you name your parameters).

Filename: select\_remote\_arducom.ini
	
	#!/bin/bash
	
	DEVICE=<device_ip>
	COMMAND=$2
	
	if [ "$1" = "1" ]
	then
		# switch on
		arducom -d $DEVICE -c $COMMAND -p 00
	elif [ "$1" = "2" ]
	then
		# switch off
		arducom -d $DEVICE -c $COMMAND -p 01
	else
		echo "parameter value not supported: $1"
	fi
	
	exit
	
	# openhatd configuration part
	
	[Root]
	
	$Name_Select = 1
	$Name_Exec = 2
	
	[$Name_Select]
	Type = SelectPort
	Label = $Label
	Group = $Group
	OnChangeUser = $Name_Exec
	OnChangeInternal = $Name_Exec
	
	[$Name_Select.Items]
	Switch = 0
	On = 1
	Off = 2
	
	[$Name_Exec]
	Type = Exec
	Program = select_remote_arducom.ini
	Parameters = $$Name_Select $Command
	ResetTime = 1000
	Hidden = true

The purpose of this file is to provide a Select port plus an associated Exec port that is triggered when the selection is changed. The select port will execute the configuration file itself as a shell script. Because of the `exit` command the openhatd configuration part will not be executed by the shell.

The shell script's parameters are the current value of the Select port and the value of the $Command parameter at include file load time. Now this involves a bit of trickery; note the `$$Name_Select` part which will be resolved in two stages: `$Name` will be replaced at include file load time; what remains is the ID of the Select port, for example `$Switch_Select`. When the Exec port is about to execute the script it replaces this value with the current position of the referenced Select port and passes the parameter string to the script which receives this value as its first parameter, `$1`.

## Relative Paths

Some nodes in openhatd require the specification of filenames, for example include files or dynamic plugin libraries. Absolute paths can be used in all cases but this is rarely a good choice. However, relative paths can be ambiguous: it is not always clear what they are relative to.

Most openhatd path specifications therefore support the `RelativeTo` setting. If this setting is specified it must be either `CWD` which means the current working directory, or `Config`, which is the location of the current configuration file. A third setting, `Plugin`, is used internally by plugins to specify locations relative to the current plugin shared library. The default value depends on the context.

The openhatd project folder structure is designed to be identical for development and production systems. This means that configuration files designed for testing during development can be used for production testing without changes. If you are building your own configurations you should have a look at these configuration files (they are located in the folder `testconfigs`) to understand how relative paths should be used.

In many cases it is a good choice to keep additional resources (for example, a custom HTML GUI) relative to the main configuration file. Program components, such as plugins, should be located relative to the current working directory to conform to the project structure.     

To specify a path relative to the current working directory the above include file example can be changed to:

	[IncludeNode1]
	Type = Include
	Filename = MyInclude.ini
	RelativeTo = CWD 

### Configuration string format<a name="configurationStringFormat"></a>

openhatd (and the OPDI libraries) use a special format for certain configuration values that is called the "configuration string format". This format can be described by:

	name1=value1[;nameN=valueN]+

Such a configuration string can contain an arbitrary number of `key=value` pairs separated by a semicolon. Any semicolon, backslash or equals-sign in keys or values must be escaped by a backslash. For example, the value `x=y` must be escaped to `x\=y`.

## General Section<a name="general"></a>

The general section supports the following configuration settings:

### `SlaveName`
Required. The `SlaveName` is the name of the openhatd server instance as displayed on a GUI.

### `LogVerbosity`

The LogVerbosity specifies the logging detail and the amount of diagnostic output that openhatd will generate. It accepts one of the [log level constants](concepts.md#logVerbosity) as its value. The default is `Normal`.

### `PersistentConfig`

Specify a file name to be used to store the persistent configuration. Relative paths are assumed to be relative to the current working directory, unless the setting `RelativeTo` specifies otherwise. The file must be writable by the user executing the openhatd process. Example:

	PersistentConfig = persistent-config.txt

### `HeartbeatFile`
 
If this setting is specified a file is written every five seconds containing the process ID and current CPU metrics of the openhatd process. It can be used to monitor the correct operation of openhatd and is most useful with service management systems like e. g. upstart.

Relative paths are always assumed to be relative to the current working directory. It is recommended to use absolute paths, however, and to prefer ramdisks over Flash memory on systems like the Raspberry Pi to reduce SD card wear.

Example:

	HeartbeatFile = /var/tmp/openhatd-heartbeat.txt

### `TargetFPS` ("frames per second")

This optional setting specifies the maximum number of doWork loop iterations openhatd should perform per second. The default is 20 which yields a resolution of 50 milliseconds; enough for most processes. Increasing this number also increases CPU load and energy use. Change this setting only if you have very good reasons to do so.

The regulation mechanism is quite slow so it may take some time to reach the intended FPS with some degree of accuracy. Note that during an active OPDI connection there is no regulation taking place to increase responsiveness. This also means that during this time the process consumes 100% CPU time.

###  `MessageTimeout`

This setting (in milliseconds) specifies the timeout for OPDI messages until the connection is assumed to be lost. The default is 10000 (10 seconds). Normally a connected master will send at least a ping message every five seconds to keep the connection alive. The maximum value for this setting is 65535.

### `IdleTimeout`

This setting (in milliseconds) specifies the time a connected OPDI master may be idle until the slave disconnects. The default is 180000 (180 seconds). The idle timeout is reset by user interactions only. Automatic refreshes do not cause the timer to be reset.

### `DeviceInfo`

The device info is a [configuration string](#configurationStringFormat) that is sent to a GUI when connecting. How this string is interpreted depends on the GUI. Example:

	DeviceInfo = startGroup=DialPorts

This device info string tells a GUI that the port group to start out with is the DialPorts group.

### `AllowHidden`

Ports can be hidden from GUIs if they are used for internal purposes only. If you want to find out more about the operation of hidden ports during a test you can globally disable port hiding by setting this value to false. You do not need to change the `Hidden` settings of all ports individually:

	AllowHidden = false

### `SwitchToUser`

Operating systems with some decent sense of security will require root privileges to access certain protected resources, like memory-mapped GPIO, certain TCP port ranges and such. It is therefore necessary to at least start openhatd as root. After the preparation phase (and after all necessary resources have been allocated) it is unsafe to keep running as root user, though. To provide some protection against remote exploits you can specify the name of a user with lower privileges that openhatd will switch to before entering the running phase.

It is highly recommended to use this setting for production systems. On Linux systems it is advised to create a user `openhat` and define

	SwitchToUser = openhat 

### `Encryption`

OPDI-based connections can optionally use an encryption method. This is especially useful if authentication is required to avoid transferring user name and password as clear text. Currently the only supported encryption method is AES with a 16 character key. Configure it like this:

	[General]
	...
	Encryption = AESEncryption
	
	[AESEncryption]
	Type = AES
	Key = 0123456789012345

### `Authentication`

OPDI-based connections can optionally require credentials (user name and password). They can be configured as follows:

	[General]
	...
	Authentication = LoginCredentials
	
	[LoginCredentials]
	Type = Login
	Username = Test
	Password = test

Note that the OPDI protocol specifies a hard-coded interval of one minute after requesting login credentials. If the data is not provided within this time the connection is closed.

## Connection Section<a name="connection"></a>

This section is used to define OPDI connection settings. You need to at least specify the transport method.

### `Transport`

Required. Currently, only `TCP` is supported.

### Port

The listening port to use for TCP connections. The default value is 13110.

## Common Port Settings

### General Considerations

All ports have an internal ID which must be unique. Ports can be referenced by other ports using this ID. The ID is taken to be the node name that defines the port. Internally generated ports may have generated IDs.

IDs of ports can be referenced in different contexts, such as port lists (separated by space), expressions (separated by other tokens) etc.; also, the contexts may specify modifier characters that alter the meaning of port IDs. It is therefore strongly recommended to:

**Use only alphanumeric characters and the underscore for port IDs and do not start them with a digit!**

The convention for naming ports is upper camel case, for example:

	DigitalPort1
	WeatherRe_Port
	FluxCompensatorPressure

but not:

	1LittlePort
	EnergyCostIn$
	Fly^High

It is also recommended to keep port IDs as short as possible to avoid running into the hard-coded OPDI limits.

The following settings apply to all ports. Individual port implementations, however, may choose to deal with these settings differently, or not respect them at all. Consult their documentation for more details.


### `Hidden`

Setting `Hidden = true` will hide the port from GUIs unless the setting `General.AllowHidden` is set to `false`.
Use it for internal ports that you don't want the user to see.

### `Readonly`

Setting `Readonly = true` will prevent a port's value to be changed by the user. It is still possible for the value to be changed by internal functions, though.

### `Persistent`

If the setting `General.PersistentConfig` is defined to be the name of a valid writable file stting `Persistent = true` enables a port to write its state to this file whenever it is changed. When reading the initial configuration the port will also take its state from this file rather than the configuration file.

How ports persist their state depends on their implementations. Port states are also persisted on shutdown before the openhatd process exits.

### `Label`

This setting defines the port's label on a GUI. It defaults to the port ID (which is the port's node name) if it is not specified.

### `DirCaps`

The "directional capabilities" of a port. May be one of `Input`, `Output` or `Bidi`. When a port is bidirectional its direction can be changed from the GUI. Is hardly used in openhatd because port directions are usually fixed within an automation model.

### `RefreshMode`

The `RefreshMode` decides whether and when GUIs should be updated when values change. Refresh messages are sent to connected GUIs on a per-port basis. The GUIs then query the current state of the affected port.

The refresh mode can be `Off`, `Auto`, or `Periodic`. If it is off, no refresh messages are sent. In automatic refresh mode a refresh message is sent when the port state changes or when the port signals an error. Periodic mode means that refresh messages are sent in millisecond intervals specified by the setting `RefreshTime` (required). The default mode is `Auto`.

To avoid overloading GUIs with too many refresh messages they are sent once a second at maximum.
 
### `RefreshTime`

If the `RefreshMode` setting is `Periodic`, this required setting specifies the refresh interval in milliseconds.

### `Unit` 

The unit specification tells GUIs how the port can be displayed. See [Unit Specifications](concepts.md#unitSpecifications) for more details.

### `Tags` <a name="tags"></a>

Port tags are a list of tag names separated by space. Tags can be used to select sets of ports using [port list specifications](ports.md#port_lists). Other than that, tags have no internal purpose, and they are not communicated to GUIs.

A good use case for this is a `log` tag. A Logger port, for example, can then specify all ports to be logged using a single `tag=log` specification.

### Port Ordering

The `Root` section's values specify the order in which nodes are processed. This roughly corresponds with the internal ordering of ports, which can be relevant for the doWork loop. It also influences the ordering of ports on a GUI.

Sometimes it may be necessary to change the internal ordering of ports independent of the order of their creation. You can specify a port's property `OrderID` to re-assign the order number. If the number specified here is higher than the internal order counter subsequently created ports will be auto-numbered with higher OrderIDs, which means they will appear after the port that has been assigned manually.

