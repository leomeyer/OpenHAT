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
