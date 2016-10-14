Window automation plugin for OpenHAT

This plugin provides the functionality to operate motor-controlled windows (or shutters etc.).
A window is normally either fully closed or fully open. However, it can be stopped at any time.
The plugin provides a SelectPort that requires at least three items for the Off, Closed and Open mode.
You must provide the usual [<SelectPort-ID>.Items] section for the labels of the states. The first
of these items corresponds with the "Off" mode of the window. The second item corresponds with
the "Closed" mode of the window. The third item corresponds with the "Open" mode of the window. 
All further items disable the manual control of the window and switch it to automatic control mode.
You can choose the labels of these items freely; however, if you do not provide all items you
won't be able to select the functions associated with the missing items.

The window supports an optional sensor to detect the closed position. You can specify a DigitalPort
node for the plugin to treat as a sensor input.

The window supports two different control modes for the actuators: H-bridge Mode and Serial Relay Mode.
H-bridge mode is used e. g. for to-wire DC motors that need their polarity reversed in order to change
direction:

                  Vcc |--------------------------|
                      |                          |
                    | o                          o |
           .------|=|                              |=|------.
           |        | o                          o |        |
           |          |       (DC motor)         |          |
           |          *--------*--(M)--*---------*          |
           |          |        |       |         |          |
           |        | o        |       |         o |        |
           |   .--|=|          |---||--|           |=|--.   |
           |   |    | o           0.47uF         o |    |   |
           |   |      |                          |      |   |
           |   |      |                          |      |   |
           |   |  Gnd |--------------------------|      |   |
           |   |                                        |   |
           |   |                                        |   |
           |   '------------------.   .-----------------'   |
           '-------------------.  |   |  .------------------'
                               |  |   |  |
                           .---o--o---o--o---.
                           |  Control logic  |
                           |                 |
                           |    e. g. L298   |
                           '----|-------|----'
                                |       |
                                |       |
                      Motor A   o       o   Motor B

Serial relay mode is used e. g. for three- or four-wire AC motors that use two phases against
a common ground for the directions. To avoid applying current to both phases at the same time the circuit
usually uses two relays, one to set the direction and one to actually turn the motor on:

                                    Motor Coil 1
                                        ___
                                     .-|___|-.
                 Relay 1      Relay 2|       |
                 Enable     Direction|       |
                                     |       |
                    _/             o-'       |
            L o---o/  o--------o--__         +--o N
                                   o-.       |
                                     |       |
                                     |       |
                                     |  ___  |
                                     '-|___|-'

                                    Motor Coil 2

This avoids the possibility that the two coils see current at the same time.
Before changing the direction the Enable switch must be opened to avoid damage to the motor coils and/or
internal capacitors. There must also be some delay before enabling it again.

The Window plugin uses different settings for the two control modes. The mode is determined by the 
ControlMode setting and can be "H-Bridge" or "SerialRelay".

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
H-bridge Mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The motor is controlled by two lines MotorA and MotorB whose truth table is as follows:

MotorA | MotorB | Motor action
-------+--------+-----------------------
     0 |      0 | No movement
     1 |      0 | Turn in "open" direction
     0 |      1 | Turn in "close" direction
   (1) |    (1) | Not used

You can specify that the line levels should be inverted by setting MotorActive to 0. This effectively
inverts the truth table.

Optionally, there is a third line called Enable which can be activated before the motor action
should take place. This is useful e. g. if there is a separate power supply for the motor which should not
be always active. The delay between enabling/disabling and the motor action can be set using EnableDelay.
You can specify that the line level should be inverted by setting EnableActive to 0.

The lines A and B and (optionally) Enable must be connected to DigitalPort nodes (in Output mode).

As there is no end position sensor for the fully open position you have to experimentally determine
a setting for the time the window needs to open completely. This time in milliseconds must be specified in the
OpeningTime setting. If you are using no closing sensor you should also set the ClosingTime setting.
If this setting is not specified it is set to the value of OpeningTime.
If a closed sensor is specified, the window status will go to Closed when the sensor is detected during closing.

After detecting a closed sensor the motor can remain active for some time to ensure that the window is fully closed.
Use the MotorDelay setting to specify this time in milliseconds. This value must be lower or equal to EnableDelay.
MotorDelay and EnableDelay are counted down at the same time in this situation.

If a closed sensor is specified but the sensor is not actually closed during operation the window will go into
an error state. See below (Window State) for further explanations.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Serial Relay Mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In Serial Relay Mode you must specify a DigitalPort for the Enable setting. The output of this port corresponds
with the Enable relay. You can specify that the line level should be inverted by setting EnableActive to 0.

You further have to specify a DigitalPort for the Direction setting. The output of this port corresponds
with the Direction relay. You can specify that the line level should be inverted by setting MotorActive to 0.

When an open or close command is issued the Window port will first switch off the Enable line, set the direction
and wait for the time in milliseconds specified in EnableDelay.

You have to experimentally determine a setting for the time the window needs to open or close completely.
This time in milliseconds must be specified in the OpeningTime and ClosingTime settings.
If ClosingTime is not specified it is set to the value of OpeningTime.
If a closed sensor is specified, the window status will go to Closed when the sensor is detected during closing.

If a closed sensor is specified but the sensor is not actually closed during operation the window will go into
an error state. See below (Window State) for further explanations.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Window State
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The select port gives the user a choice of the window operation mode. In the automatic control setting,
the window state is controlled by the settings AutoOpen and AutoClose. You can specify any number of
DigitalPort IDs in these settings. If any of these ports has a line state of High this will trigger opening 
or closing of the window respectively. The AutoClose setting takes precedence.
AutoOpen and AutoClose will only work in the automatic mode. In manual Off, Closed and Open modes changes of
these lines have no effect.

You can optionally specify one or more DigitalPorts that force the window to the open or closed states
if at least one port has a line state of High. This mechanism can be used to open or close the window (e. g. in 
case there is rain etc). This mechanism also works when the mode has been set to another mode than Automatic.
As long as one of the ForceOpen ports is High, the window cannot be closed any more.
As long as one of the ForceClose ports is High, the window cannot be opened any more.
You cannot use both settings at the same time. This is to avoid a decision about force precedence.
Usually a regular window will have to be force closed while shutters etc. will preferably be force opened.

The window can detect some errors that may arise due to hardware failures or misconfigurations.
An error is assumed if 
a) the window is being opened and after the specified opening time the sensor still reads "Closed",
b) the window is being closed and after the specified closing time the sensor does not read "Closed".

The error can be due to
- sensor failure (switch misaligned, contact problem, cable interruption...)
- motor failure (power failure, contact problem, motor damage...)
- blocked window (doesn't fully close any more due to obstructions...)
- misconfiguration (wrong sensor or motor ports specified)
- etc.

In case of an error there is no meaningful positional setting of the select port any more. 
Also, automatic control, including the ForceOpen and ForceClose mechanisms, will be disabled.
The window will remain in the error state until the OpenHAT system restarts or the select port position is changed
to Off, Closed or Open. This serves as a protection in case of some serious problem.
The mode cannot be set to Automatic once the window is in the error state.

If the position is manually opened or closed after an error the window will try to put itself into the specified
position. If this works the mode can again be set to automatic. Otherwise the window will return to the error state.

To reset from an error condition you can specify any number of digital ResetPorts. If any of these ports is High
it causes the window state to return to UNKNOWN. The target state will be set to the value specified in the ResetTo
setting, which may be either "Open" or "Closed". If the value is not specified the window will not do anything.
Otherwise it will try to put itself in the defined state after the reset has been initiated.
