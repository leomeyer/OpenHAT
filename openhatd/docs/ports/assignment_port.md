## Assignment Port Description

An Assignment port is a Digital port that can be used to assign a set of predefined or runtime-determined values to **target ports**. The assignments take place at the moment the Assignment port is switched from `Low` to `High`. After applying all assignments the line is set to `Low` again, to make the Assignment port ready for new assignments. Thus, an Assignment port will always appear to be `Low`.

An Assignment port contains a number of predefined assignment settings that use the following schema:

	<targetPortID> = <value>

When doing the assignments the original source (internal or user) that changed the Assignment port is passed to the target ports.

Assignments are performed in alphanumerically sorted order of target port IDs. Only one assignment is done per target port, even if multiple assignments have been specified for this port.

## Settings

### Type
Fixed value `Assignment`.

### [_portID_.Assignments]
This section contains the assignment specifications belonging to the Assignment port with ID `portID`. 
They use the following schema:

	<targetPortID> = <value>

The `targetPortID` must be a valid ID of a port in the current configuration. `value` is a string representation of the value that is to be assigned. You can also use a [ValueResolver](../ports.md#value_resolvers). 

The rules for target ports are:

- Digital port: Is set to `Low` if the determined value is 0 and to `High` in all other cases.
- Analog port: The value is expected to be a floating point number between 0 and 1, inclusive. The relative value of the target port is set from this value, with 1 representing the value `2^resolution - 1`.
- Dial port: The target port's position is set to the value which is treated as a 64 bit signed integer.
- Select port: The target port's position is set to the value which is treated as a 16 bit unsigned integer.

## Example

{!testconfigs/test_assignment.ini!}
