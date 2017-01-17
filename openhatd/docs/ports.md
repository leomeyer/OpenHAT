
All ports in openhatd are built upon one of the [basic port types](ports/basic_ports.md). The ports described here implement specific functionality, extending the capabilities of the basic types.

Please see [Automation Examples](automation_examples.md) to understand how to combine ports to model automation behavior. 

At the end of this document you will find information about [port errors](#port_errors), [port list specifications](#port_lists) and [value resolvers](#value_resolvers).

## [Logic Port](ports/logic_port.md)

The Logic port allows you to process the state of one or more Digital ports by applying a logic function (OR, AND, XOR, ATLEAST(n), ATMOST(n), EXACTLY(n) and their inversions). The Logic port is a Digital port whose state reflects the current result of its logic function. The result can also be applied to other Digital ports.

## [Pulse Port](ports/pulse_port.md)

A Pulse port is a Digital port that can generate periodic pulses with a defined period and duty cycle. Period and duty cycle can be determined from other port's values at runtime. The output can be sent to Digital ports (inverted and non-inverted). Its maximum frequency depends on your system but in most cases would not exceed 100 Hz, so it's not really suitable for dimming lights or LEDs like a "real" PWM. It should be used for blinking lights (status indicators) or periodic actions that do not repeat very quickly.

## [Selector Port](ports/selector_port.md)

A Selector port is a bridge between a Digital port and a Select port. If it is set to High it will select a certain position from the Select port. It will also be High if the Select port is in that position, and Low otherwise.

The Selector port is used when certain conditions should result in the selection of a specified label, or if the choice of a certain label should influence other ports. 

## [Error Detector Port](ports/error_detector_port.md)

An Error Detector is a Digital Port that is High when at least one of a list of ports has an error. The different types of errors of ports, and what it exactly means if a port has an error, are explained below.

## [Fader Port](ports/fader_port.md)

A Fader port can fade Analog or Dial ports in or out. You can specify a number of options such as the fader mode (linear or exponential), the start and end values, and what happens when the Fader port is switched off. The Fader port is itself a Digital port, and it will begin its fading operation at the moment its state is set to High.

## [Scene Select Port](ports/scene_select_port.md)

A Scene Select port lets you select one of several pre-defined so-called scenes. A scene is just a specification of the states of some ports in the system. These can be defined in a configuration file that is being applied when the corresponding option is selected.

## [File Port](ports/file_port.md)

A File port is a Digital port which, while its Line is High, monitors the content of a specified file. If the file's content changes it is being read and the result is set to a certain specified "value port", possibly applying some transformations. If the value port is not read-only any changes in its value are written to the specified file.

The File port is an important port which allows openhatd to receive input from its surroundings in a generic way, i. e. without the need for specialized drivers.

## [Exec Port](ports/exec_port.md)

The Exec port is a Digital port which, when it is set to High, executes a predefined operating system command. As a File Input port provides input to an openhatd instance, the Exec port allows openhatd to interact with the environment in a generic way; for example, send an email, execute maintenance scripts, or interact with proprietary hardware via command line tools. The Exec port can pass information about the current state of ports to the called program.

## [Aggregator Port](ports/aggregator_port.md)

An Aggregator port can collect data about a specified Dial port over time and perform some statistical calculations. It can also provide historical data which can be used by the UI to display a graph or other information.

## [Counter Port](ports/counter_port.md)

A Counter port is a Dial port whose value increments either linearly with time in specified intervals, or with events detected by a Trigger port.

## [Trigger Port](ports/trigger_port.md)

A Trigger port can detect specified events on one or more Digital ports, for example, whether they are set to High or Low, or toggled. It can in turn set the state of specified output ports when this action occurs, or increment a specified Counter port. A Trigger port is itself a Digital port which operates only if its Line is High.

## [Timer Port](ports/timer_port.md)

A Timer port is a Digital port that switches other Digital ports on or off according to one or more scheduled events. Events can be scheduled in different ways: once, in recurring intervals, periodically at predefined date/time patterns, astronomically (sunrise/sunset), or manually.

## [Expression Port](ports/expression_port.md)

An Expression port is a very versatile component that allows you to evaluate formulas or even small programs depending on the state of other ports, including value transformations, comparisons, and more complex formulas. Ports can be referred to in the formula by using their IDs as variable names. The result of the expression can be assigned to an output port. The Expression port uses the Exprtk library whose documentation can be found here: http://www.partow.net/programming/exprtk/

## [InfluxDB Port](ports/influxdb_port.md)


## Port Errors <a name="port_errors"></a>

All port types support an error state. An error state is an error code plus an optional error message. Error codes and messages are defined by the ports that use them. An error becomes apparent when the value of the port is requested; either for display on a user interface or if the value is requested by an internal calculation or evaluation.

The basic port types do never signal an error. This makes sense if you think of them as internal variables without external functions; accessing a variable always works and always yields a result. For advanced ports that may represent external components it is not so easy any more. These ports must react to an external component not being available or an internal calculation failing, perhaps in response of other ports being in an error state.

There are two main functions that port errors have:

 - Signal the user (on a user interface) that something is not working as expected
 - Tell dependent ports that something is wrong
	
The user interfaces indicate port errors by displaying an error symbol, hiding the port's value (because it is not available) and perhaps displaying an error message. If ports access the values of a port that has an error the reaction depends on the port's implementation or sometimes configuration. There are a number of options:


 - Ignore the error and pretend that all is well
 - Use a default value for calculations instead of the port's "real" value
 - Enter an error state as well
 - Output diagnostic information
	
How a port reacts to other ports' errors is specified in the port's documentation.

## Port Lists Specifications <a name="port_lists"></a>

Many ports connect to other ports, mostly as inputs or outputs. For example, a Logic port's output is expected to be a number of other Digital ports in the automation model. Internally these ports need to build a list of their connected ports. This list must be provided in the configuration.

There are a number of rules you can use when specifying port lists. Ports can be included or excluded based on their IDs, their group membership, or their [tag values](configuration.md#tags).

Port list specifications are a space-separated list that allow the following components:

- An asterisk `*` includes all ports of the model.
- Port IDs that must match exactly, separated by space.
- `id=regex`: All ports whose ID matches the specified regular expression.
- `group=regex`: All ports whose group matches the specified regular expression.
- `tag=regex`: All ports who have at least one tag that matches the specified regular expression.  

All components, except `*`, can be inverted by prepending an exclamation mark `!`.

Regular expressions are implemented using PCRE, the [Perl Compatible Regular Expressions library by Philip Hazel](http://www.pcre.org). Refer to the [Regular Expression Syntax](http://www.pcre.org/current/doc/html/pcre2pattern.html) for more information.

Port IDs must be specified with the correct case. Regular expression searches are case insensitive.

Examples:

- `MyPort1 MyPort2`: Select ports `MyPort1` and `MyPort2`.
- `* !MyPort1`: Select all ports except MyPort1.
- `group=^My.*`: Select all ports whose group starts with 'My'.
- `MyPort1 tag=log !group=test`: Select MyPort1 plus all ports that have a tag named 'log' but not those whose group contains 'test' (case insensitive).

## Value Resolvers <a name="value_resolvers"></a>

Most port parameters expect numeric values as their settings. You can either specify these values directly in the configuration, in which case they remain fixed as long as openhatd is running, or use a so-called **value resolver** to tell openhatd to use the current runtime value of another port as the parameter value. This allows for more complex interactions between ports. It is up to a port's implementation whether a value resolver can be used for a certain parameter or not.

For example, a Fader port used for dimming lights might choose to take the current value of a dimmer port as a starting point for gradually fading out to zero. Providing a fixed value might have the unwanted effect of starting out at a higher value which would make the light brighter before fading out. To avoid this the value can be provided to the Fader port not only as a numeric value but as a port reference (a value resolver), too.

A value resolver specification has the following syntax:

- either a numeric value,
- or an expression of type `<PortID>[(scale_factor)][/(error_value)]` with the parts in brackets being optional.

Examples:

*   `10`               - fixed value 10
*   `MyPort1`          - value of MyPort1
*   `MyPort1(0.1)`     - value of MyPort1, multiplied by 0.1
*   `MyPort1/0`        - value of MyPort1, 0 in case of an error
*   `MyPort1(100)/0`   - value of MyPort1, multiplied by 100, 0 in case of an error

If an error value is not specified any errors will be propagated to the port that is using the value resolver. It depends on this port how it chooses to react to any error conditions.

You can specify any port type (except Streaming ports) in a value resolver. The following rules apply:

* A Digital port's value is either 0 (Low) or 1 (High).
* An Analog port's value is its relative value within the range 0..1. In most cases a scale factor will need to be provided to map the value to a useful range (because most port parameters are integer values).
* A Dial port's value is its current position.
* A Select port's value is the number of its currently selected label (0-based).

If you require more sophisticated calculations consider using an [Expression port](ports/expression_port.md).