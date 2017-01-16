## Counter Port Description

A Counter port is a Dial port whose value changes with time or based on other events. The counter can **increment** or **decrement** periodically. The `Period` is a numeric value based either on seconds, milliseconds, or frames. The `Increment` can be any signed 64 bit number. The default `Minimum` of a counter port is the lowest number representable by a signed 64 bit variable. The default `Maximum` of a counter port is the highest number representable by a signed 64 bit variable. The `Step` is 1. The default `Position` is 0. A Counter port is read-only by default.

Please note that the maximum time resolution you can get depends on your platform/operating system and the "frames per second" resolution of the doWork loop. This means that the Counter port is not suitable for accurate time-keeping.

A Counter port can also be controlled by a [Trigger port](trigger_port.md). In this case the counter is increased or decreased when the Trigger port has detected a trigger condition. You should set the `Period` to 0 if you want to have the Counter port controlled by a Trigger port. This will prevent the Counter port from automatically increasing.

The Counter port will wrap around its minimum and maximum values, if it detects that the new position is less than `Minimum` or greater than `Maximum`. These conditions are called **underflow** and **overflow**. The detection will only work properly if you have set `Minimum` and `Maximum` to values that allow proper arithmethic in the 64 bit range. It fails if incrementing or decrementing by itself over- or underflows the range of a signed 64 bit variable. This will be the case for the default settings.

An underflow is detected if the newly calculated position is less than the minimum port value and the increment is negative. In this case the new position will be the maximum value minus the amount of the underflow. For example suppose the current value is 10, the minimum is 8, the maximum is 100 and the increment is -5. In this example the underflow will result in a value of 97 = 100 - (8 - 5).

An overflow is detected if the newly calculated position is greater than the maximum port value and the increment is positive. In this case the new position will be the minimum value plus the amount of the overflow. For example suppose the current value is 95, the minimum is 20, the maximum is 100 and the increment is 10. In this example the overflow will result in a value of 25 = 20 + (105 - 100).   

An underflow or overflow can trigger a notification mechanism that sets a list of Digital ports to High when the condition has been detected. The ports are set to `High` at the moment the condition occurred. If an incrementation has been made without an over- or underflow the notification ports are set to `Low`.

## Settings

### Type
Fixed value `Counter`.

### TimeBase
The `TimeBase` setting specifies the unit for the `Period` setting. It supports the following values:

	Seconds
	Milliseconds
	Frames

The default `TimeBase` is `Seconds`.

### Period
The interval in which to apply the change with respect to the time base. The default is 1.

You can specify a [ValueResolver](../ports.md#value_resolvers) whose value is resolved at every iteration of the doWork loop. If the ValueResolver returns an error the counter is not changed. Also, if the period is less than 1 the counter is not changed.

### Increment
The value by which to increase or decrease the port's value. The value increases if the increment is greater than 0 and decreases if the value is less than 0. If the increment is 0 changes have no effect.

You can specify a [ValueResolver](../ports.md#value_resolvers) whose value is resolved when the period is up. If the ValueResolver returns an error the counter is not changed.

### UnderflowPorts
A list of IDs of Digital ports that should be set to High when an underflow occurs.

### OverflowPorts
A list of IDs of Digital ports that should be set to High when an overflow occurs.

## Example

{!testconfigs/test_counter.ini!}