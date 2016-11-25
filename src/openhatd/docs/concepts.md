##Basic Concepts

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
