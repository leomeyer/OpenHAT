## Pulse Port Description

A Pulse port is a Digital port that generates a digital pulse with a defined **period** (measured in milliseconds) and a **duty cycle** in percent. The pulse is active if the line of this port is `High`. If Digital **enable ports** are specified the pulse is also being generated if at least one of the enable ports is `High`.

The output can be normal or inverted. There are two lists of output digital ports which receive the **normal** or **inverted output** respectively.

The actual duration of the pulses depends on the time resolution of the doWork loop. This in turn depends on the `TargetFPS` setting of the General configuration section. Please see [Time in openhatd](../concepts.md#time) for more information.

## Applications
The Pulse port can be used if you need blinking lights or LEDs, for example for indicators. Please note that a Pulse port is too slow to do PWM, so it should not be used for dimming lights.

A Pulse port can also be used to control internal automation. If itself is included in its output port list it will be automatically deactivated when the pulse becomes low again. This behaviour can be used to trigger other actions, for example reset other ports, after some delay. See the example below for a practical instance of this configuration. 

## Settings

### Type
Fixed value `Pulse`.

### Period
Required. A positive integer value that specifies the pulse duration in milliseconds.

You can specify a [ValueResolver](../ports.md#value_resolvers) whose value is resolved at every iteration of the doWork loop. If the ValueResolver returns an error or a value less than 1 the pulse is set to the disabled state, if specified.

### DutyCycle
Required. An integer value between 0 and 100 that specifies the pulse duty cycle in percent. 

You can specify a [ValueResolver](../ports.md#value_resolvers) whose value is resolved at every iteration of the doWork loop. If the ValueResolver returns an error, a value less than 0 or a value greater than 100 the pulse is set to the disabled state, if specified.

### Negate
If this optional flag is `True` the logical meaning of the pulse output is inverted. Default is `False`. 

### EnablePorts
An optional [port list specification](../concepts.md#port_lists) for Digital ports. The pulse is generated if at least one of the enable ports is High.

### OutputPorts
An optional [port list specification](../concepts.md#port_lists) for Digital ports. The output of the pulse is applied to these ports directly.

### InverseOutputPorts
An optional [port list specification](../concepts.md#port_lists) for Digital ports. The output of the pulse is inverted and applied to these ports.

### DisabledState
Optional value of either `Low` or `High`. If this value is set it determines the logical output state of the pulse when it is disabled (i. e. set to `Low`). The output value is applied directly to the output ports and inverted for the inverted output ports.

## Example

{!testconfigs/test_pulse.ini!}
