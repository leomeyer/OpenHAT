## Exec Port Description

The Exec port allows openhatd to interact with the operating system by **executing scripts or other programs**. The Exec port supports passing **parameters** (the current state of ports within openhatd) to the external program.

The Exec port is a Digital port that executes the specified program when it is changed to `High`. The Exec port will remain `High` as long as the program is running. When the program has terminated the Exec port will set itself back to `Low`. It is not possible to set the Exec port to `Low` while the program is still running.

The Exec port first verifies that the program file exists. It then builds the argument list from the specified parameters and starts a new process that executes the program. Standard output and error streams are captured and injected into the openhatd log output. Standard output messages are logged with LogVerbosity `Verbose`. Messages on stderr are logged with LogVerbosity `Normal`.

The Exec port monitors the started program until it terminates. The exit code of the program, a 32 bit unsigned integer value, can be optionally assigned to a Dial port (`ExitCodePort`). This Dial port should be configured to accept the range of expected exit codes (the default `Maximum` is 100). It also reflects various error conditions that may occur during program execution.

An optional `KillTime` (in milliseconds) can be specified. If the running time of the process exceeds this value it is terminated by using the OS-specific process termination method. If specified, an `ExitCodePort`'s error state is set to "value not available". The Exec port returns to `Low` and monitoring of standard output and error stream stops. The Exec port will then be ready to start a new instance of the program. 

## Settings

### Type
Fixed value `Exec`.

### Program
Required. The name of the program to execute. If a relative path is specified the path is assumed to be relative to the configuration file. This setting can be overridden by specifying `RelativeTo = CWD` which interprets the path as relative to the current working directory.

The program can either be a native binary or a shell script. For platform independent config files you can use the parameter `$PLATFORM_SPECIFIC_SHELLSCRIPT_EXTENSION` which yields `.bat` on Windows and `.sh` on Linux.

If an error occurs during the start of the program an optional `ExitCodePort`'s error will be set to "value not available".

### RelativeTo
Optional specification what the `Program` path is relative to; possible values are `Config` (default, specifying the directory of the current configuration file), and `CWD` (specifying the current working directory of the application).

For more information see [Relative paths](../configuration.md#relative_paths).

### Parameters
Optional specification of parameters that are to be supplied to the program as arguments. If this string contains parameters from the runtime environment (starting with a `$`) they are replaced with the current values of these parameters. The runtime environment contains all parameters from environment variables, the default openhatd environment parameters, any parameters that have been passed to openhatd using the `-p name=value` command line syntax, the current values of ports plus the special parameter `$ALL_PORTS` that contains a `name=value`-formatted, space separated list of all port IDs and their current values. If a port's value cannot be resolved due to an error, the value will be `<error>`. 

For details about the runtime environment, see [Configuration Parameters](../configuration.md#parameters).

Parameter replacement is done using a simple string substitution. Afterwards, the result is being split at space characters and passed to the program as an argument list.

### KillTime
An optional value in milliseconds that determines when a running process is automatically killed. Use this setting to limit the process time in case the started program hangs. The kill mechanism is inactive if this value is 0 (default).

### ExitCodePort
An optional ID of a Dial port that accepts the exit code of the process. If the process is killed by the `KillTime` mechanism this port's error will be set to "value not available".
 
## Example

{!testconfigs/test_exec.ini!}
