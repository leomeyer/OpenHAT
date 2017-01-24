## Exec Port Description

The Exec port allows openhatd to interact with the operating system by **executing scripts** or other commands. The Exec port supports passing **parameters** (the current state of ports within openhatd) to the external program.

The Exec port is a Digital port that executes its program when it is set to `High`. The Exec port will remain `High` as long as the program is running or until a specified `ResetTime` has expired. If the program exits or the `ResetTime` is up the Exec port will be set back to `Low`.



 