## ErrorDetector Port Description

An ErrorDetector port is a Digital port whose state is determined by the error states of one or more specified ports. If any of these ports have an error, i. e. their hasError() method returns true, the state of this port will be `High` and `Low` otherwise. The logic level can be negated.

## Settings

### Type
Fixed value `ErrorDetector`.

### InputPorts
A required [port list specification](../concepts.md#port_lists) for ports of any type that are considered for error detection.

### Negate
If this optional flag is `True` the ErrorDetector port is `High` if no error is detected and `Low` otherwise. Default is `False`. 
