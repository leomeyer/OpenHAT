A test port is a Digital port that can be used to test certain properties of ports during the operation of the program. A test is executed in regular intervals. The interval can be based on seconds, milliseconds, or frames. A test port is only active if its Line is High and its interval greater than 0.

A test port contains a number of predefined test-case settings that use the following schema:

	<portID>:<property> = <expected value>

When the test has reached its specified interval time all test cases are checked by resolving the specified ports and testing the property against the expected value. The comparison is done by the ports themselves and may be implementation specific.

Most port properties can be tested by this method. It depends on the individual ports which properties they want to expose for testing, what the required format for the expected values is and how the comparison is done.

 