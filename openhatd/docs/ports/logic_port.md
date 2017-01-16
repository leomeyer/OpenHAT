## Logic Port Description

A Logic port is a Digital port that calculates a **logical function** over the states of a set of Digital ports called **input ports**. The Logic port is special in that it reflects the outcome of the calculation in its own state, i. e. it will be `High` if the result of the logic function is `True` and `Low` if it is `False`. In this way it is possible to use a Logic port directly as a further input without the need for a separate output port. You can, however, assign the result to a number of specified **output ports**, and there is also a list of **inverted output ports**.

A Logic port supports the following logical calculations:

- OR (default): The result is High if at least one of the inputs is High.
- AND: The result is High if all of the inputs are High.
- XOR: The result is High if an odd number of inputs is High.
- ATLEAST n: The line is High if at least n inputs are High.
- ATMOST n: The line is High if at most n inputs are High.
- EXACTLY n: The line is High if exactly n inputs are High.

Additionally you can specify whether the result should be **negated**.

The logic port updates the specified optional output ports only if its state changes, i. e. there is no continuous update of the output ports. If a different value is assigned to an output port by a different component it will only be overwritten by the Logic port at the next change of the outcome of the logic function.

## Settings

### Type
Fixed value `Logic`.

### InputPorts
Required. A space-separated list of IDs of Digital ports whose Line values should be considered as inputs. 

### Function
Optional. One of `OR` (default), `AND`, `XOR`, `ATLEAST`, `ATMOST`, or `EXACTLY`. The keywords `ATLEAST`, `ATMOST` and `EXACTLY` are expected to be followed by a blank and a positive integer number parameter.

### Negate
Optional boolean value. If `True`, the result of the logic function is inverted.

### OutputPorts
Optional space-separated list of IDs of Digital ports that are updated when the result of the logic calculation changes.

### InverseOutputPorts
Optional space-separated list of IDs of Digital ports that are updated when the result of the logic calculation changes. The Line state of these ports is set to the inverted result.

## Example

{!testconfigs/test_logic.ini!}