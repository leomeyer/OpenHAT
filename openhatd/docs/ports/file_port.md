## File Port Description

The File port is a port that allows openhatd to read and write content from or to a **specified file**. It is actually implemented as two ports: A Digital port that does the monitoring if its line is `High`, and a **value port** that reflects the file content. If the value port's value is changed its current value is written to the file, unless it is readonly.

When configuring a File port you have to specify a configuration node for the value port. The `Root` section of the configuration must not reference this node. The value port must be of one of the basic types Digital, Analog, Dial, or Select port. The mode of Digital or Analog types must be `Input`. Other than that, the value port can be arbitrarily configured. 

The File port monitors the file for changes. It reloads the file content if it is `High` whenever it detects a modification. The file does not need to exist initially. If the file does not exist the value port's error will be set to "value not available". Its value will be set (and the error be cleared) as soon as the file is created.

Numeric values read from or written to the file can be scaled by a `Numerator` and a `Denominator` value. Their defaults are 1 which means that scaling is disabled.

Before parsing, the file content is trimmed of whitespace. File content is then parsed according to the following rules, depending on the type of the value port:

- Digital: A value of `0` represents `Low`, a value of `1` represents `High`. Other values cause an error.
- Analog: A floating point number that is multiplied with the numerator and divided by the denominator to give a result between 0 and 1, inclusive. The relative value of the value port is set from this value, with 1 representing the value `2^resolution - 1`. Decimal separator is the dot (`.`). Thousand separator is the comma (`,`).
- Dial: A signed 64 bit integer number multiplied with the numerator and divided by the denominator. The result must be in the range of the value port's `Minimum` and `Maximum`.
- Select: An integer value between 0 and the maximum position number of the value port, inclusive. This value is not scaled by numerator and denominator.

Writing the file happens the other way round. For Analog and Dial ports the port's value is divided by the denominator and multiplied by the numerator first.

A File port can be configured to delete its file after it has been read. You can also specify a reload delay to avoid reading the file too fast. Additionally, the value port's error can be set to "value expired" if the file has not been re-read within a specified time.

## Usage
A File port is used to integrate external components with openhatd. Any script or program that can create files can thus provide input for openhatd. This is especially useful with external sensor input that is read using an external program because it saves the need for a specialized sensor driver within openhatd.

In most cases it is advisable in these cases to have the value port expire after some time, for example 10 seconds. If the external sensor ceases to function or some problem occurs with the script that updates the input file it will be reflected in openhatd. This is preferable over displaying and calculating with outdated values.

## Settings

### Type
Fixed value `File`. 

### File
Required. The file to monitor or write. If a relative path is specified the path is assumed to be relative to the configuration file. This setting can be overridden by specifying `RelativeTo = CWD` which interprets the path as relative to the current working directory.

### RelativeTo
Optional specification what the `File` path is relative to; possible values are `Config` (default, specifying the directory of the current configuration file), and `CWD` (specifying the current working directory of the application).

For more information see [Relative paths](../configuration.md#relative_paths).

### PortNode
Required. The configuration node for the value port. This port must be specified in the same configuration as the File port and it must specify at least a `Type` setting which may be `DigitalPort`, `AnalogPort`, `DialPort`, or `SelectPort`. The port is added to openhatd's list of ports, i. e. it may not be referenced by the `Root` section of the configuration.

### ReloadDelay
An optional integer value in milliseconds that specifies the minimum time to wait between file reloads. The default is 0.

### Expiry
An optional integer value in milliseconds that specifies the time until the value port's error is set to "value not available" if no file change is detected. The default is 0 which means that the file never expires.

During the configuration phase, if an Expiry > 0 is specified, the File port checks whether the file's modification date is within the specified expiry time. If so, the value is read from the file. If not, the file is treated as expired and the value port's error is set accordingly. 

### DeleteAfterRead
If this optional boolean value is set to `True` the file is deleted after it has been read.

### Numerator
An optional integer value that specifies the number to multiply or divide the value by when reading or writing the file. Only applies to Analog and Dial ports. The default is 1.

### Denominator
An optional integer value that specifies the number to divide or multiply the value with when reading or writing the file. Only applies to Analog and Dial ports. The default is 1.

## Example

{!testconfigs/test_file.ini!}
