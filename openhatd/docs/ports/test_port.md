## Test Port Description

A Test port is a Digital port that can be used to test certain properties of ports during the operation of the program. A test is executed in regular intervals. The interval can be based on seconds, milliseconds, or frames. A test port is only active if its Line is High and its interval greater than 0.

A test port contains a number of predefined test-case settings that use the following schema:

	<targetPortID>:<property> = <expectedValue>

When the test has reached its specified interval time all test cases are checked by resolving the specified target port and testing the property against the expected value. The comparison is done by the ports themselves and may be implementation specific.

It depends on the individual ports which properties they want to expose for testing, what the required format for the expected values is and how the comparison is done. Testable properties should correspond with their configuration setting name, but this is not guaranteed. Generally, only port properties that can be changed during runtime need to be exposed for testing.

A Test port can be configured to warn or terminate on test failure, and to terminate the program after executing the tests. This feature is useful for automatic testing.

## Settings

### Type
Fixed value `Test`.

### TimeBase
The `TimeBase` setting specifies the unit for the `Interval` setting. It supports the following values:

	Seconds
	Milliseconds
	Frames

The default `TimeBase` is `Seconds`.

### Interval
The `Interval` specifies how often the tests are run with respect to the `TimeBase`. If this value is less than 1 tests will not be run. The default value is 0 which means the tests will not be performed. Tests will start to run when the specified interval has expired after the first iteration of the doWork loop. 

### WarnOnFailure
If `WarnOnFailure` is set to `True` the test will only output a warning if a test comparison does not match. Otherwise, the program outputs an error message and terminates with error code 128 (`TEST_FAILURE`). The default is `False` which means that any test failure causes an immediate termination.

### ExitAfterTest
If `ExitAfterTest` is set to `True` the program will exit after the test has been performed for the first time. The default is `False`.

### [_portID_.Cases]
This section contains the test cases belonging to the Test port with ID `portID`. 
They use the following schema:

	<targetPortID>:<property> = <expectedValue>

The `targetPortID` must be a valid ID of a port in the current configuration. `property` specifies the port property that should be tested, while `expectedValue` is a string representation of the target property.

The default comparison is for string equality. A test fails if the expected value is not equal to the actual value of the port property. For Boolean properties such as `Hidden` or `Readonly` the values `1`, `true` and `yes` (case insensitive) are interpreted as a Boolean `true`.
 
Port implementations may choose to interpret the expected value in any way they like. They may also choose to test attributes other than string equality. Please refer to their documentation for a list of testable properties.
