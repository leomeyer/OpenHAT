## Selector Port Description

This port is a DigitalPort that is `High` when a specified Select port is in a certain **position** and `Low` otherwise. The other direction works as well: If changed to `High` it will switch the select port to the specified position.

The Selector port can be used to detect whether a certain position is selected, or to set a certain position from within the automation model.

Monitoring the Select port happens in the doWork loop. At each iteration the state of the Select port is queried. If it returns an **error** and a value for `ErrorState` has been specified, the Select port's `Line` is set to `ErrorState`. Otherwise no further processing takes place.

If the Select port returns a valid position number and this number equals the defined `Position` value, the `Line` is set to `High` if it is not already `High`. If it is already `High`, nothing is done. If the position number does not equal the defined `Position` value the `Line` is set to `Low` if it not already `Low`. If it is already `Low`, nothing is done.

If a Selector port is `Low` and it is set to `High` by another port or by the user the Select port's position is changed to the defined `Position`. If it is set to `Low` nothing is done.

The Selector port can automatically propagate its Line state to a number of optional `output ports` of type Digital.

## Settings

### SelectPort
Required. The ID of the Select port to monitor and control.

### Position
Required. A value between 0 and 65535 that specifies the monitored Select port's position. The Selector port checks during the preparation phase whether the specified Select port supports this value.

### ErrorState
An optional value of either `Low` or `High`. If querying the Select port returns an error and this value is set, the Selector port's line will be set to it.

### OutputPorts
An optional [port list specification](../ports.md#port_lists) for Digital ports.
