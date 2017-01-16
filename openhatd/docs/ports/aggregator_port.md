## Aggregator Port Description

An Aggregator port is a Digital port that collects values of other ports over time and performs statistical calculations on them. It can be used to retain a history of port values (useful for displaying a graph on a GUI, for example), or to calculate values such as the mean, minimum or maximum over a specified time, or the difference of the last and first element. Internally, the values are stored as fractional numbers of type `double`. The output of a specific calculation can be assigned to a Dial port. 

You have to specify a **source port** whose values are to be collected. The source port can be of any of the four basic types Digital, Analog, Dial or Select port. In most cases you will chose a Dial port as source because it simply makes most sense. The rules for collected values are:

- Digital port: The value is either 0 (Low) or 1 (High).
- Analog port: A relative value between 0.0 and 1.0 (1.0 if the value is at 2^resolution - 1).
- Dial port: The current position of the Dial port.
- Select port: The number of the currently selected label, starting from 0.

The collected values can be multiplied using an optional **multiplier**. This is useful for small input values as calculation results are assigned to a Dial ports that can store integer numbers only. The output will be rounded to the next integer number which may cause a loss of precision if the inputs are not sufficiently scaled.

An Aggregator port collects numeric values in a specified **interval**. You also have to specify the maximum **number of values** that are to be collected. When a new value is collected and the list of values is full the oldest value is dropped and the new value is appended to the history. Please note that the Aggregator collects its samples only at the specified intervals. It does not take any values into account that might be present in between.

Data from the source port can also be checked for plausibility. A **minimum and maximum delta** can be provided to perform a range check on new values. If the difference between the new and the last value is outside the range the value will not be accepted; instead, it will be discarded because the Aggregator treats this condition as an error. This feature is useful if you have sensor data that for some reason is sometimes out of a meaningful range. If such an error occurs the Aggregator port will increase an internal error counter.

If an error occurs during value collection (due to port errors such as "value unavailable" etc.) the Aggregator will also increase the internal error counter. You can specify a **number of allowed errors** that will be tolerated until the Aggregator port clears the list of collected values. The default setting is 0 meaning that any occurring error (value out of range or port error) will reset the list and discard any previous calculation results. In case of ports that may have sporadic, temporary errors you can increase this value.

An Aggregator port by defaults provides a history of port values to the port it collectes values from. You can switch this feature off or provide a different **target port** that should be used for the history.

Dial ports support a special feature called an **automatic aggregator**. This is a hidden Aggregator port that is only used to provide history collection for a Dial port. Please see the Dial port documentation on how to use this feature. An automatic aggregator does not perform additional statistical calculations.

**Calculations** are a set of algorithms that are to be performed on the collected values. Each calculation produces an output value that is assigned to a Dial port. These Dial ports are implicitly created by the Aggregator port and must not be specified in the `Root` section of the configuration. They inherit the group and refresh mode of the Aggregator port (these settings can be changed, however) and are always read-only. The target Dial ports have their own sections in the configuration and can be configured just like ordinary Dial ports. At startup all such Dial ports will signal an error of type "value not available".

Calculations are performed whenever a new value has been collected. If the result of a calculation cannot be determined (because the list of values is empty, or incomplete) the target Dial port will reflect this by returning an error of type "value not available".  

You can specify the following calculation types:

- `Delta`: The difference between the last and the first element of the list of values. The list must by default be full for this calculation to work. The use case for this calculation is to determine the rate of change of counters, for example gas counters, over a specified time (moving window).
- `ArithmeticMean` or `Average`: Calculates the arithmetic mean over the list of values. The list does not have to be full unless specified otherwise.
- `Minimum`: Calculates the minimum value over the list of values. The list does not have to be full unless specified otherwise.
- `Maximum`: Calculates the maximum value over the list of values. The list does not have to be full unless specified otherwise.

Aggregator ports can also persist their history in a persistent configuration file. This allows them not to lose their history over restarts of the openhatd service. In addition to persisting their history whenever a new value is collected they also persist it on shutdown. Also, they store a timestamp to determine whether the history data is outdated or not. When the data is being read the timestamp must be within the specified interval period for the values to be accepted. There are two caveats, however: On shutdown, the values are saved with the current timestamp regardless of when the last value has been collected. When the data is being loaded, collection resumes with the persisted timestamp as start of the interval. This may cause the last persisted value and the next value being collected to have a time difference that is larger than the specified interval. Second, the timestamp may not have a defined starting point, meaning that time can start running at server startup. This may cause a loss of historic data over server restarts because the new timestamps may be lower than those in the persisted configuration. This behavior may be OS-dependent.

Automatic aggregator ports, i. e. those that are automatically generated when a Dial port specifies a History setting, are automatically persisted if a persistent file has been specified.

## Settings

### Type
Fixed value `Aggregator`.

### SourcePort
Required. The ID of the source port that should be used for retrieving values.

### Interval
Required. A positive value in seconds that specifies the delay between retrieving values.

### Values
Required. The total number of values to collect.

Commonly used combinations of `Interval` and `Values` are:

One value per minute for one hour:

	Interval = 60
	Values = 60

One value per minute for one day:

	Interval = 60
	Values = 1440

### Multiplier
An optional integer value used to scale up incoming values. The default is 1.

### MinDelta
The optional minimum difference of the new and the last value that is to be accepted (if applicable). For counters that can only increase this value should be set to 0. This means that a decreasing value will not be accepted but increase the internal error counter. The default is a very low negative number that causes all values to be accepted.

### MinDelta
The optional maximum difference of the new and the last value that is to be accepted (if applicable). For meters  (power or electricity etc.) this value should be set to the maximum meter reading. This means that an implausible value will not be accepted but increase the internal error counter. The default is a very high positive number that causes all values to be accepted.

### AllowedErrors
Optional number of errors (port errors or unaccepted values) that this Aggregator port will allow before clearing its internal list and setting the results of all calculations to the error "value not available". The default is 0.

### HistoryPort
The ID of the port to send history values to. By default this is the source port but you may use any other port. 

### SetHistory
Optional boolean value that specifies whether to set the history back to the history port. If you are not interested in history values you can set this to `False`. The default is `True`.

### [_portID_.Calculations]
This section contains an ordered list of Dial port IDs that are required to have their own configuration section in the current configuration. The Aggregator port creates these Dial ports, configures them according to the standard procedure and adds them to the list of openhatd ports. Additionally, each of these ports has the following special configuration settings:

### Algorithm
Required. Expected values are `Delta`, `ArithmeticMean` or `Average`, `Minimum` or `Maximum` (see above for their meanings).

### AllowIncomplete
This optional boolean value specifies whether the calculation requires the list of values to be full or not. The default depends on the respective algorithm and is `True` for `Delta` and `False` for all other algorithms.

## Example

{!testconfigs/test_aggregator.ini!}
