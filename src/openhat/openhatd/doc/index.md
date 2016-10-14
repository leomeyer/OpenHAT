
##OpenHAT Automation Software

The Open Home Automation Toolkit server is an open-source, cross-platform automation server written in C++11 for performance and small memory footprint. It can be configured for a variety of automation tasks, for example home automation.
It has the following features:


 - Ideally suited for Raspberry Pi and other Linux based systems
 - Configurable by text files in INI file format
 - Controllable via an Android app or HTML/Javascript (with a JSON-RPC web service API)
 - Supports a plugin architecture for extensibility
 - Controls a variety of actors, for example relays or dimmers
 - Accepts input from switches or A/D converters
 - Supports scene files for predefined combinations of settings
 - Stores user settings in a file for persistence over restarts
 - Has plugins for:
	 - Raspberry Pi Gertboard I/O board utilizing the onboard ATMEGA as a port expander
	 - Weather station support (interoperates with the Weewx weather station software, see: http://weewx.com)
	 - Window and shutter control with a variety of options
	 - Fritz!Box smart home devices
	 - An integrated web server
	 - Radio-controlled power sockets (experimental)
 - Built-in functions:
	 - Timer that supports periodical, astronomic (sunset/sunrise) or manual scheduling
	 - Expression engine for complex mathematical calculations
	 - Interoperation with the OS: Monitor files to read input, execute OS commands
	 - Logic functions for changing outputs based on logical conditions
	 - Pulse with configurable duration and duty cycle
	 - Fader for smooth control of e. g. dimmers
	 - Statistical aggregator for calculations of means etc.
	 - Trigger that is activated when inputs change
	 -  and more

OpenHAT allows you to monitor and control the state of sensors and actors such as lamps, fans, dimmers, windows and shutters, radio-controlled power sockets, weather sensors, electric, gas and water meters and many more.

Typical use cases in home automation include:


 - Open or close windows and shutters manually or timer-controlled
 - Ensure that windows stay closed when it rains or if it is too cold outside
 - Switch on the lights or fade in a dimmer in the morning
 - Display current meter values and water or energy consumption statistics
 - Show current weather conditions
 - Control heating equipment  
 
OpenHAT relies on numerous free libraries. A big thanks goes to the providers of those libraries whose effort made this work possible.
 
Basic Concepts
==============

In order to work with OpenHAT it is necessary to understand some basic concepts. This will also give you some understanding of OpenHAT's flexibility and limitations.

The basic automation building block is called a "Port". Everything in OpenHAT revolves around the configuration of ports and their connections. Ports encapsulate simple or advanced behavior and get or provide information from or to other ports. It is the combination of ports that makes the emergence of complex automation behavior possible.

You can think of ports as internal variables of the system. Ports can also be exposed on a user interface, giving read-only or writable access to the user. OpenHAT can automatically log the values of ports. At the same time, ports can implement the system's functionality, for example window operation state machines etc.

There are different types of ports in OpenHAT. To be able to model automation behavior with ports you'll have to understand these different types of ports and their properties and functions. The basic properties of all types of ports are:


 - A unique ID which is a text string preferably without blanks and special characters, for example "Window1"
 - A "Hidden" flag which decides whether to display this port on a user interface or not
 - An optional label and icon specification to display on a user interface (if the port is not hidden)
 - An optional unit specification which determines value conversions, formatting etc.
 - A "Readonly" flag which decides whether a user may change this port's value
 - A "Persistent" flag which decides whether the state of this port is remembered permanently
 - An optional group ID (ports can be ordered into hierarchical groups)
 - A refresh mode that decides how state changes affect the connected user interfaces
 - Some port specific flags and capabilities settings
 - A freely assignable tag value

All ports share these properties but their meaning or use may depend on the specific port type.

Basically, ports are of five different types:

Digital Port
------------

A Digital port is the most elementary port type. A Digital port has a "Mode" and a "Line" state. Modes can be Input and Output, and the line can be either Low or High.

The Digital port is modeled after a digital I/O pin of a microcontroller. The important thing here is that a Digital port's state is always known, and it is either Low or High. Essentially, the Digital port models a switch. In OpenHAT it is used for "things that can be on or off", no matter whether the state of these things is controlled directly by a user or by internal functions, or whether the Digital port is an actor or reflects the state of some outside sensor (by reading its state from some driver).

Typical examples of Digital ports are LEDs or relays (output only), and switches or buttons (input only). However, most higher-level functions in OpenHAT are also modeled as Digital ports, thus giving you the ability to switch these functions on or off. For example, a Pulse port can periodically change the state of one or more Digital ports ("outputs"). The Pulse port itself is again a Digital port, and it will only be active if its Line state is High. Thus, you can switch the Pulse port on or off using a user interface, or the Pulse port can again be controlled by any other port that accepts a Digital port as an output. In this way you can connect elementary blocks together to form complex behavior.

Analog Port
-----------

An Analog port is modeled after the the properties of an A/D or D/A port of a microcontroller. Its value ranges from 0 to 2^n - 1, with n being a value between 8 and 12 inclusively.
The Analog port also has a Reference setting (internal/external), and a Mode (input/output). The Analog port is less useful in an OpenHAT automation context because it is modeled so close to the metal. In most cases it is better to use a Dial port instead.

Dial Port
---------

The Dial port is the most versatile and flexible port. It represents a 64 bit signed value. There's a minimum, maximum and a step setting which limit the possible values of a Dial port to a meaningful range; for example, to represent an ambient temperature in degrees Celsius you could limit the range to -50..50.

It's important to understand that OpenHAT does not do floating point arithmetics internally. You therefore cannot store fractional values in ports. This is not a problem in practice, though; if you need to represent for example tenths of degrees Celsius you could specify the range as -500..500 and divide the value by 10 for all internal calculations. If the value has to be presented to a user the conversion happens in the UI component; the port's unit specification tells the UI what the value is exactly and what options there are for converting and formatting the value. This also helps with localizing the UI; for example, even though the value is processed internally as tenths of degrees Celsius the UI might select to display it as degrees Fahrenheit, by means of a conversion routine specified for this unit. The most important thing here is to remember what the internal value actually means for a specific Dial port.

Dial ports can represent numeric information as well as dates/times (see below). As usual with OpenHAT ports, the value of a Dial port might be set by a user, or it might be the result of an internal calculation, or it might reflect the state of some hardware or other external component (like the content of a file). The best way to initially think about a Dial port is as of a universal numeric variable.

Select Port
-----------

A Select port represents a set of distinct labeled options. Its most useful application is to present the user with a choice; however, the Select port can also have internal uses. The most important difference to the Digital port is that the Select port does not necessarily have a known state. Take, for example, a radio controlled power socket. The radio control is one-way only in most cases, so that there is no way to know whether the power socket is actually on or off; its state is essentially unknown. Such a device should not be modeled with a Digital port as a Digital port must have a known state. It can, however, be conveniently modeled using a Select port with three options: Unknown, Off, and On. If the user selects Off or On the command is sent to the socket via radio, but the Select port's state will not reflect the user's choice but instead remain Unknown.

Streaming Port
--------------

A Streaming port can be used to transfer text or binary data. Its use in OpenHAT, for now, is very limited.

Built-In Advanced Ports
=======================

All available ports in OpenHAT are built on one of the above fundamental port types. However, the advanced ports implement specific functionality extending the capabilities of the basic types.

Logic Port
----------

The Logic port allows you to process the state of one or more Digital ports by applying a logic function (OR, AND, XOR, ATLEAST(n), ATMOST(n), EXACT(n) and their inversions). The result can be sent to other Digital ports. A Logic port is itself a Digital port; it will do its processing only if its Line is High (i. e. active).

Pulse Port
----------

A Pulse port is a Digital port that can generate periodic pulses with a defined period and duty cycle. Period and duty cycle can be determined from other port's values at runtime. The output can be sent to Digital ports (inverted and non-inverted). Its maximum frequency depends on your system but in most cases would not exceed 100 Hz, so it's not really suitable for dimming lights or LEDs like a "real" PWM. It should be used for blinking lights (status indicators) or periodic actions that do not repeat very quickly.

Selector Port
-------------

A Selector port is a bridge between a Digital port and a designated Select port. If it is set to High it will select a certain position from the Select port. It will also be High if the Select port is in that position, and Low otherwise.

Error Detector Port
-------------------

An Error Detector is a Digital Port that is High when at least one of a list of ports has an error. The different types of errors of ports, and what it exactly means if a port has an error, are explained below.

Fader Port
----------

A Fader port can fade Analog or Dial ports in or out. You can specify a number of options such as the fader mode (linear or exponential), the start and end values, and what happens when the Fader port is switched off. The Fader port is itself a Digital port, and it will begin its fading operation at the moment its state is set to High.

Scene Select Port
-----------------

A Scene Select port lets you select one of several pre-defined so-called scenes. A scene is just a specification of the states of some ports in the system. These can be defined in a configuration file that is being applied when the corresponding option is selected.

File Port
---------------

A File port is a Digital port which, while its Line is High, monitors the content of a specified file. If the file's content changes it is being read and the result is set to a certain specified "value port", possibly applying some transformations. If the value port is not read-only any changes in its value are written to the specified file.

The File port is an important port which allows the OpenHAT automation to receive input from its surroundings in a generic way, i. e. without the need for specialized drivers.

Exec Port
---------

The Exec port is a Digital port which, when it is set to High, executes a predefined operating system command. As a File Input port provides input to an OpenHAT instance, the Exec port allows OpenHAT to interact with the environment in a generic way; for example, send an email, execute maintenance scripts, or interact with proprietary hardware via command line tools. The Exec port can pass information about the current state of ports to the called program.

Aggregator Port
---------------

An Aggregator port can collect data about a specified Dial port over time and perform some statistical calculations. It can also provide historical data which can be used by the UI to display a graph or other information.

Counter Port
------------

A Counter port is a Dial port whose value increments either linearly with time in specified intervals, or with events detected by a Trigger port.

Trigger Port
------------

A Trigger port can detect specified events on one or more Digital ports, for example, whether they are set to High or Low, or toggled. It can in turn set the state of specified output ports when this action occurs, or increment a specified Counter port. A Trigger port is itself a Digital port which operates only if its Line is High.

Timer Port
----------

A Timer port is a Digital port that switches other Digital ports on or off according to one or more scheduled events. Events can be scheduled in different ways: once, in recurring intervals, periodically at predefined date/time patterns, astronomically (sunrise/sunset), or manually.

Expression Port
---------------

An Expression port is a very versatile component that allows you to evaluate formulas or even small programs depending on the state of other ports, including value transformations, comparisons, and more complex formulas. Ports can be referred to in the formula by using their IDs as variable names. The result of the expression can be assigned to an output port. The Expression port uses the Exprtk library whose documentation can be found here: http://www.partow.net/programming/exprtk/

Port Errors
===========

All port types support an error state. An error state is an error mode plus an optional error message. Error states and messages are defined by the port types, and may vary. An error becomes apparent when the value of the port is requested; either for display on a user interface or if the value is needed by an internal calculation or evaluation.

The basic port types do never signal an error. This makes sense if you think of them as internal variables without external functions; accessing a variable always works and always yields a result. For advanced ports that may represent external components it is not so easy any more. These ports must react to an external component not being available or an internal calculation failing, perhaps in response of other ports being in an error state.

There are two main functions that port errors have:


 - Signal the user (on a user interface) that something is not working as expected
 - Tell dependent ports that something is wrong
	
The user interfaces indicate port errors by displaying an error symbol, hiding the port's value (because it is not available) and perhaps displaying an error message. If ports access the values of a port that has an error the reaction depends on the port's implementation or sometimes configuration. There are a variety of options:


 - Ignore the error and pretend that all is well
 - Use a default value for calculations instead of the port's "real" value
 - Enter an error state as well
 - Output diagnostic information
	
How a port reacts to other ports' errors is specified in the port's documentation.

Port Refreshes
==============

Refreshing port information is another important topic in OpenHAT. It serves two main purposes:


 - A user interface must be updated if the value of an internal port changes
 - Ports may regularly need to query external components to update their internal state
	

Persistence
===========

Persistence deals with saving port state permanently. It allows you to store the state of certain ports in a separate configuration file. The state in this "dynamic" configuration file then takes precedence over the specifications in the "static" configuration files when the port states are initialized at startup. This effectively restores the last known state of the ports and is especially useful for user preferences like timer and other automation settings.

Ports update the persistent configuration file whenever some relevant state changes occur. For a Digital port for example, this is the change of Mode and Line. For a Dial port it is the change of its value.

Persistence is active if the General section of the configuration specifies a file in the PersistentConfig setting, and the Persistent setting of a port is set to true.

Aggregator ports (special ports that collect historic values and perform calculations on them) can also persist their history in the persistent configuration file. This allows them not to lose their history over restarts of the OpenHAT service. In addition to persisting their history whenever a new value is collected they also persist it on shutdown of the OpenHAT service. Also, they store a timestamp to determine whether the history data is outdated or not. When the data is being read the timestamp must be within the specified interval period for the values to be accepted. There are two caveats, however: On shutdown, the values are saved with the current timestamp regardless of when the last value has been collected. When the data is being loaded, collection resumes with the persisted timestamp as start of the interval. This may cause the last persisted value and the next value being collected to have an interval that is larger than the specified interval. Second, the timestamp may not have a defined starting point, meaning that time can start running at server startup. This may cause a loss of historic data over server restarts because the new timestamps may be lower than those in the persisted configuration. This behavior may be OS-dependent.

Automatic aggregator ports, i. e. those that are automatically generated when a Dial port specifies a History setting, are automatically persisted.

On systems that use an SD card as their primary storage care should be taken to put the persistent configuration file into a ramdisk and save/restore to/from SD card on stopping and starting the server to reduce SD card wear.

Time in OpenHAT
=============

OpenHAT is not a real time system. For operations that must be regularly timed (such as periodic refreshes of port information, timer actions, pulses etc.) the most finely grained unit is the millisecond. Some settings do have to be specified in milliseconds, others in seconds (depending on what makes more sense).

Measuring time is platform dependent. On Windows the time resolution may easily be reduced to 10-16 milliseconds. On Linux the time granularity is usually better.

After startup, OpenHAT periodically iterates through all ports that have been created and registered during the initialization process. This is called the doWork loop. Depending on the number of ports and how they are configured an iteration of the doWork loop requires a certain amount of time to process. The length of the doWork iterations determines the speed on which ports can act; for example, if you specify a Pulse port with a duration of 10 milliseconds while the doWork loop already requires 20 milliseconds to run, the Pulse port will not be able to keep up its 10 milliseconds duration. Instead, it will run with a lower time resolution.

In analogy to computer game programming, the number of doWork iterations per seconds are called "frames per second" or fps. Having OpenHAT run at a high fps rate gives you a finer granularity of the time slices and consequently more precision, but most of the time this is not necessary. It also consumes more CPU cycles and more power in turn. OpenHAT allows you to specify a target fps rate that the system tries to attain by putting the process to sleep for the rest of the time if the doWork loop runs faster than required. This is done by measuring the current doWork duration and comparing it against a set target fps rate. The sleep time is adjusted based on this comparison. It may take quite some time until the defined target fps rate is actually reached (easily one hour or so). For a long-running server this is usually not a problem.

Generally it is recommended to set the fps rate fairly low, to maybe 10 or 20. This saves CPU power but still gives you a resolution of 50 to 100 milliseconds which is enough for most applications. In Debug log verbosity, OpenHAT outputs a warning if the doWork loop consumes a lot of CPU time.

All OpenHAT ports should try to do as little as possible in the doWork loop. Some actions in OpenHAT need to be performed independently of the doWork loop because they require more work or employ some kind of blocking. These functions are usually implemented using separate threads. For port implementors who use threads it is important to know that everything that modifies OpenHAT port state must only be done in the doWork loop which is called from the main thread.

However, some operations that must be done on the main thread may cause the doWork iteration to delay for a time that is longer (maybe much longer) than the specified fps rate. An example is the WebServerPlugin which serves incoming requests in the doWork method. So, while time in OpenHAT (as obtained by the internal function opdi_get_time_ms) is guaranteed to increase monotonically, the intervals between doWork invocations may vary greatly. This point should be taken into account when configuring port settings and developing OpenHAT plugins or ports. On Windows, due to the time granularity being 10 ms or more, two or more subsequent doWork iterations may even seem to run at the same time as reported by the opdi_get_time_ms function.

Miscellaneous information
=========================

Operating system compatibility
------------------------------

OpenHAT is designed to be platform-independent and portable between C++ compilers. It works with recent MSVC and GCC that support the C++11 standard. There are currently some problems with clang that might eventually be worked out.

Home automation server hardware needs to provide lots of I/O ports and ideally consumes little power. This rules out most consumer grade PCs or laptops. Instead, home automation with OpenHAT is best done with small and efficient headless computers like the Raspberry Pi.

Compiling OpenHAT on a Raspberry Pi unfortunately is not feasible, so it requires the use of a cross-compiler. The recommended system for cross-compiling OpenHAT for Raspberry Pi is an Ubuntu-based Linux system (you can also use a virtual machine on Windows). Different operating systems provide different advantages when developing or compiling OpenHAT:


- Windows: Good tooling (Visual Studio 2015 Community Edition provides an excellent debugger). Well suited for development and testing.
- Linux: Good toolchains, but debugger support is somewhat basic. Good for tests, static analysis, and cross-compiling, or trying different compilers. Linux specific development, obviously, must be done here.
- Raspberry Pi: Suitable for production use, but less for development. OK for developing specific plugins which compile reasonably well on the Pi.

Only the binaries for the Raspberry Pi are compiled in "release mode", i. e. without debug symbols and with optimizations for speed and size. In general, OpenHAT binaries should be statically linked to avoid problems with shared libraries. Plugins, however, are provided as dynamic libraries that are linked at runtime.

Logging
-------

* Log levels (LogVerbosity)

The log level determines the amount of logging output. The following log levels are (somewhat loosely) defined (from lowest to highest, a higher log level includes the lower levels):

QUIET: The absolute minimum. Only warnings and errors are logged.

NORMAL: Fairly minimal logging with limited information. Recommended setting when a system is stable.

VERBOSE: Recommended for testing if some amount of information is required. Will log non-regular events (i. e. events that occur on user interaction, or happen infrequently) plus some technical information.

DEBUG: Will additionally log regularly occurring events plus more detailed technical information, but not from the inner doWork loop. Can produce a large amount of log messages. Recommended if you need additional hints about system behaviour, but not for normal operation. Will also log the OPDI message streams if a master is connected.

EXTREME: Will additionally output messages from the inner doWork loop. This level will generate log messages in every frame and will quickly produce lots of output. Recommended for isolated testing only.

* Individual ports can override the global log level

The log level is globally configurable using the LogVerbosity ini file setting. Individual nodes or ports can specify their own log level which then takes precedence. This allows you to selectively increase or decrease log levels for certain ports.


Environment
-----------

A home automation server like OpenHAT should start running at system startup. This can be done in a variety of ways, depending on OS and personal preference. An example script for Upstart on Linux is provided (for more information about Upstart see http://upstart.ubuntu.com).

OpenHAT is typically started by the root account. In fact, root permissions must be provided to gain access to certain resources if required; for example, on the Raspberry Pi, to have fast access to the GPIO pins root is required as these pins are memory-mapped to a restricted system memory area. However, it it is not desirable to keep OpenHAT running as root, especially if you expose network access which OpenHAT naturally must do. OpenHAT allows you to specify a SwitchToUser setting which is applied after initializing (and acquiring restricted resources). The process will then continue to run with the permission of this user making your setup more secure.

It is recommended to create a user "openhat" on your system, with home directory being for example /home/openhat. Within this directory you should create a sub-directory for each OpenHAT instance configuration. The default instance which does the main work might be called "openhat"; there may also an instance like "openhat-test" or a restricted system administration instance. The SwitchToUser setting should be set accordingly.

All necessary code (application, plugins, and support files) plus the configuration should be put into such a folder. This folder should also keep scene files, and serve as the permanent storage location for the persistent config file.

During operation, OpenHAT needs (optionally) to write a few useful files, these are:


	- A log file which is most useful for testing and inspecting the current state of the file (log rotation is being used, with two log files of 1 MB max),
	- A heartbeat file used in determining server state,
	- A persistent configuration file for user settings that should not be lost on a restart,
	- A number of (possibly timestamped) protocol files that contain the state of ports; these are useful for creating statistics or graphs.

Writing these files puts quite a lot of stress on systems that use an SD card as primary storage. Therefore it is recommended to reserve a ram disk with mount point /var/openhat and have OpenHAT's configuration use this directory as base for the file locations. During server startup and shutdown scripts can be run to create this folder and copy necessary files to or from there. Care should be taken to get the folder and file permissions correct, especially if using the SwitchToUser setting with a user with less privileges.

SD card or harddisk failure should be expected at any time. It would therefore be wise to make regular backups of the relevant configuration files or to keep a reserve medium at hand.

Heartbeat file
--------------


