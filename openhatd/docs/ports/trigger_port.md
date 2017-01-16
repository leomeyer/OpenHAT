## Trigger Port Description

A Trigger port is a Digital port that continuously monitors one or more digital ports for **changes in their state** if its `Line` is `High`. Triggering can happen on a change from `Low` to `High` (**rising edge**), from `High` to `Low` (**falling edge**), or on **both changes**.

The checks are done in each iteration of the doWork loop. If a state change is detected the Trigger port can modify the states of other Digital ports (**output ports**). The effected state change can either be to set the output Digital ports to `High`, `Low`, or toggle them. If you specify **inverse output ports** the state change is inverted, except for the Toggle specification.

Disabling the TriggerPort causes the monitoring to stop. Startup or enabling a Trigger will cause it to read the current state of the monitoring ports. A port that returns an error will be ignored.

A Trigger port can work together with a [Counter port](counter_port.md) to count the occurrences of a certain trigger event.

## Settings

### Type
Fixed value `Trigger`.

### InputPorts
Required. A list of IDs of Digital ports that are to be monitored by this Trigger port.

### Trigger
Defines the triggering event. One of `RisingEdge`, `FallingEdge`, or `Both`. The default is `RisingEdge`.

### Change
Defines how to change the output ports. One of `SetHigh`, `SetLow`, or `Toggle`. The default is `Toggle`.

### OutputPorts
An optional list of IDs of Digital ports that are modified when a trigger event occurs. If `Change` is `SetHigh` these ports will be set to `High`. If `Change` is `SetLow` these ports will be set to `Low`. If `Change` is `Toggle` the line of these ports will be toggled.   

### InverseOutputPorts
An optional list of IDs of Digital ports that are modified when a trigger event occurs. If `Change` is `SetHigh` these ports will be set to `Low`. If `Change` is `SetLow` these ports will be set to `High`. If `Change` is `Toggle` the line of these ports will be toggled.   

### CounterPort
An optional ID of a Counter port that will be incremented by 1 when the trigger event occurs.

## Example

{!testconfigs/test_trigger.ini!}
