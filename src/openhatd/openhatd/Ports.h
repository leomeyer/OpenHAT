#pragma once

// need to guard against security check warnings
#define _SCL_SECURE_NO_WARNINGS	1

#include <sstream>
#include <fstream>
#include <list>

#include "Poco/DirectoryWatcher.h"
#include "Poco/TimedNotificationQueue.h"
#include "Poco/Tuple.h"
#include "Poco/Util/AbstractConfiguration.h"

// serial port library
#include "ctb-0.16/ctb.h"

#include "opdi_port.h"

#include "AbstractOpenHAT.h"

namespace openhat {

///////////////////////////////////////////////////////////////////////////////
// Logic Port
///////////////////////////////////////////////////////////////////////////////

/** A LogicPort implements logic functions for digital ports. It supports
* the following operations:
* - OR (default): The line is High if at least one of its inputs is High
* - AND: The line is High if all of its inputs are High
* - XOR: The line is High if an odd number of its inputs is High
* - ATLEAST (n): The line is High if at least n inputs are High
* - ATMOST (n): The line is High if at most n inputs are High
* - EXACT (n): The line is High if exactly n inputs are High
* Additionally you can specify whether the output should be negated.
* The LogicPort requires at least one digital port as input. The output
* can optionally be distributed to an arbitrary number of digital ports.
* Processing occurs in the OPDI waiting event loop.
* All input ports' state is queried. If the logic function results in a change
* of this port's state the new state is set on the output ports. This means that
* there is no unnecessary continuous state propagation.
* If the port is not hidden it will perform a self-refresh when the state changes.
* Non-hidden output ports whose state is changed will be refreshed.
* You can also specify inverted output ports who will be updated with the negated
* state of this port.
*/
class LogicPort : public opdi::DigitalPort {
protected:
	enum LogicFunction {
		UNKNOWN,
		OR,
		AND,
		XOR,
		ATLEAST,
		ATMOST,
		EXACT
	};

	openhat::AbstractOpenHAT* openhat;
	LogicFunction function;
	size_t funcN;
	bool negate;
	std::string inputPortStr;
	std::string outputPortStr;
	std::string inverseOutputPortStr;
	opdi::DigitalPortList inputPorts;
	opdi::DigitalPortList outputPorts;
	opdi::DigitalPortList inverseOutputPorts;

	virtual uint8_t doWork(uint8_t canSend) override;

public:
	LogicPort(AbstractOpenHAT* openhat, const char* id);

	virtual ~LogicPort();

	virtual void configure(ConfigurationView* config);

	virtual void setLine(uint8_t line, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;

	virtual void prepare() override;
};

///////////////////////////////////////////////////////////////////////////////
// Pulse Port
///////////////////////////////////////////////////////////////////////////////

/** A pulse port generates a digital pulse with a defined period (measured
* in milliseconds) and a duty cycle in percent. The period and duty cycle
* can optionally be set by analog ports. The period is in this case
* calculated as the percentage of Period. The pulse is active if the line
* of this port is set to High. If enable digital ports are specified the
* pulse is also being generated if at least one of the enable ports is High.
* The output can be normal or inverted. There are two lists of output digital
* ports which receive the normal or inverted output respectively.
*/
class PulsePort : public opdi::DigitalPort {
protected:
	openhat::AbstractOpenHAT* openhat;
	bool negate;
	opdi::ValueResolver<int32_t> period;
	opdi::ValueResolver<double> dutyCycle;
	opdi::ValueResolver<int32_t> pulses;
	int8_t disabledState;
	std::string enablePortStr;
	std::string outputPortStr;
	std::string inverseOutputPortStr;
	opdi::DigitalPortList enablePorts;
	opdi::DigitalPortList outputPorts;
	opdi::DigitalPortList inverseOutputPorts;

	// state
	uint8_t pulseState;
	uint64_t lastStateChangeTime;
	int32_t pulseCount;
	virtual uint8_t doWork(uint8_t canSend) override;

public:
	PulsePort(AbstractOpenHAT* openhat, const char* id);

	virtual ~PulsePort();

	virtual void configure(ConfigurationView* config);

	virtual void prepare() override;
};

///////////////////////////////////////////////////////////////////////////////
// Selector Port
///////////////////////////////////////////////////////////////////////////////

/** A SelectorPort is a DigitalPort that is High when the specified select port
*   is in the specified position and Low otherwise. If set to High it will switch
*   the select port to the specified position. If set to Low, it will do nothing.
*/
class SelectorPort : public opdi::DigitalPort {
protected:
	openhat::AbstractOpenHAT* openhat;
	std::string selectPortStr;
	opdi::SelectPort* selectPort;
	std::string outputPortStr;
	opdi::DigitalPortList outputPorts;
	uint16_t position;

	virtual uint8_t doWork(uint8_t canSend) override;

public:
	SelectorPort(AbstractOpenHAT* openhat, const char* id);

	virtual ~SelectorPort();

	virtual void configure(ConfigurationView* config);

	virtual void setLine(uint8_t line, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;

	virtual void prepare();
};

///////////////////////////////////////////////////////////////////////////////
// Error Detector Port
///////////////////////////////////////////////////////////////////////////////

/** An ErrorDetectorPort is a DigitalPort whose state is determined by one or more 
*   specified ports. If any of these ports will have an error, i. e. their hasError()
*   method returns true, the state of this port will be High and Low otherwise.
*   The logic level can be negated.
*/
class ErrorDetectorPort : public opdi::DigitalPort {
protected:
	openhat::AbstractOpenHAT* openhat;
	bool negate;
	std::string inputPortStr;
	opdi::PortList inputPorts;

	virtual uint8_t doWork(uint8_t canSend) override;

public:
	ErrorDetectorPort(AbstractOpenHAT* openhat, const char* id);

	virtual ~ErrorDetectorPort();

	virtual void configure(ConfigurationView* config);

	virtual void prepare() override;
};

///////////////////////////////////////////////////////////////////////////////
// Serial Streaming Port
///////////////////////////////////////////////////////////////////////////////

/** Defines a serial streaming port that supports streaming from and to a serial port device.
 */
class SerialStreamingPort : public opdi::StreamingPort {
friend class OPDI;

protected:
	// a serial streaming port may pass the bytes through or return them in the doWork method (loopback)
	enum Mode {
		PASS_THROUGH,
		LOOPBACK
	};

	Mode mode;

	openhat::AbstractOpenHAT* openhat;
	ctb::IOBase* device;
	ctb::SerialPort* serialPort;

	virtual uint8_t doWork(uint8_t canSend) override;

public:
	SerialStreamingPort(AbstractOpenHAT* openhat, const char* id);

	virtual ~SerialStreamingPort();

	virtual void configure(ConfigurationView* config);

	virtual int write(char* bytes, size_t length) override;

	virtual int available(size_t count) override;

	virtual int read(char* result) override;

	virtual bool hasError(void) const override;
};

///////////////////////////////////////////////////////////////////////////////
// Logger Streaming Port
///////////////////////////////////////////////////////////////////////////////

/** Defines a streaming port that can log port states and optionally write them to a log file.
 */
class LoggerPort : public opdi::StreamingPort {
friend class OPDI;

protected:
	enum Format {
		CSV
	};

	openhat::AbstractOpenHAT* openhat;
	uint32_t logPeriod;
	Format format;
	std::string separator;
	std::string portsToLogStr;
	opdi::PortList portsToLog;
	bool writeHeader;
	uint64_t lastEntryTime;
	
	std::ofstream outFile;

	std::string getPortStateStr(opdi::Port* port);

	virtual uint8_t doWork(uint8_t canSend) override;

public:
	LoggerPort(AbstractOpenHAT* openhat, const char* id);

	virtual ~LoggerPort();

	virtual void configure(ConfigurationView* config);

	virtual void prepare() override;

	virtual int write(char* bytes, size_t length) override;

	virtual int available(size_t count) override;

	virtual int read(char* result) override;

	virtual bool hasError(void) const override;
};

///////////////////////////////////////////////////////////////////////////////
// Fader Port
///////////////////////////////////////////////////////////////////////////////

/** A FaderPort can fade AnalogPorts or DialPorts in or out. You can set a left and right value
* as well as a duration time in milliseconds. If the FaderPort is inactive (line = Low),
* it will not act on the output ports. If it is set to High it will start at the
* left value and count to the right value within the specified time. The values have 
* to be specified as percentages. Once the duration is up the port sets itself to 
* inactive (line = Low). When the fader is done it can set optionally specified DigitalPorts
* (end switches) to High.
* If ReturnToLeft is specified as true, the output port is set to the current value of Left
* when switched off.
*/
class FaderPort : public opdi::DigitalPort {
protected:
	enum FaderMode {
		LINEAR,
		EXPONENTIAL
	};

	enum SwitchOffAction {
		NONE,
		SET_TO_LEFT,
		SET_TO_RIGHT
	};

	openhat::AbstractOpenHAT* openhat;
	FaderMode mode;
	opdi::ValueResolver<double> leftValue;
	opdi::ValueResolver<double> rightValue;
	opdi::ValueResolver<int> durationMsValue;
	double left;
	double right;
	int durationMs;
	double expA;
	double expB;
	double expMax;
	bool invert;
	SwitchOffAction switchOffAction;

	std::string outputPortStr;
	opdi::PortList outputPorts;

	std::string endSwitchesStr;
	opdi::DigitalPortList endSwitches;

	Poco::Timestamp startTime;
	double lastValue;
	SwitchOffAction actionToPerform;

	virtual uint8_t doWork(uint8_t canSend) override;

public:
	FaderPort(AbstractOpenHAT* openhat, const char* id);

	virtual ~FaderPort();

	virtual void configure(ConfigurationView* config);

	virtual void setLine(uint8_t line, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;

	virtual void prepare() override;
};

///////////////////////////////////////////////////////////////////////////////
// Scene Select Port
///////////////////////////////////////////////////////////////////////////////

/** A SceneSelectPort is a select port with n scene settings. Each scene setting corresponds
* with a settings file. If a scene is selected the port opens the settings file and sets all
* ports defined in the settings file to the specified values.
* The SceneSelectPort automatically sends a "Refresh all" message to a connected master when
* a scene has been selected.
*/
class SceneSelectPort : public opdi::SelectPort {
protected:
	typedef std::vector<std::string> FileList;

	openhat::AbstractOpenHAT* openhat;
	FileList fileList;
	std::string configFilePath;

	bool positionSet;

	virtual uint8_t doWork(uint8_t canSend) override;

public:
	SceneSelectPort(AbstractOpenHAT* openhat, const char* id);

	virtual ~SceneSelectPort();

	virtual void configure(ConfigurationView* config, ConfigurationView* parentConfig);

	virtual void setPosition(uint16_t position, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;

	virtual void prepare() override;
};

///////////////////////////////////////////////////////////////////////////////
// File Port
///////////////////////////////////////////////////////////////////////////////

/** A FilePort is a port that reads or writes its state from or to a file. It is represented by
* a DigitalPort; as such it can be either active (line = High) or inactive (line = Low).
* The FilePort actually consists of two ports: 1.) one DigitalPort which handles
* the actual file monitoring; it can be enabled and disabled, and 2.) a value port with
* a specified type that reflects the file content.
* You have to specify a PortNode setting which provides information about the value
* port that should reflect the file content. This node's configuration is
* evaluated just as if it was a standard port node.
* The port monitors the file and reads its contents when it changes. The content is
* parsed according to the value port's type:
*  - DigitalPort: content must be either 0 or 1
*  - AnalogPort: content must be a decimal number (separator '.') in range [0..1]
*  - DialPort: content must be a number in range [min..max] (it's automatically adjusted
*    to fit the step setting)
*  - SelectPort: content must be a positive integer or 0 specifying the select port position
*  - StreamingPort: content is injected into the streaming port as raw bytes
* Additionally you can specify a delay that signals how long to wait until the file is
* being re-read. This can help avoid too many refreshes.
* For analog and dial ports the value can be scaled using a numerator and a denominator.
* The FilePort can also be set (by a user or an internal function), if it is not read-only.
* If the FilePort is active (High) when such a state change occurs the state of the value port
* is written to the specified file.
*/
class FilePort : public opdi::DigitalPort {
protected:

	enum PortType {
		UNKNOWN,
		DIGITAL_PORT,
		ANALOG_PORT,
		DIAL_PORT,
		SELECT_PORT,
		STREAMING_PORT
	};

	// Dummy digital port that handles the state change event of the value port
	class ChangeHandlerPort : public opdi::DigitalPort {
		FilePort* filePort;
	public:
		ChangeHandlerPort(const char* id, FilePort* filePort) : opdi::DigitalPort(id) {
			this->filePort = filePort;
		}

		virtual void setLine(uint8_t line, ChangeSource /*changeSource*/) override {
			// the internal state is not updated as this is an event handler only
			if (line == 1)
				this->filePort->writeContent();
		}
	};

	openhat::AbstractOpenHAT* openhat;
	std::string filePath;
	Poco::File directory;
	opdi::Port* valuePort;
	PortType portType;
	int reloadDelayMs;
	int expiryMs;
	bool deleteAfterRead;
	int numerator;
	int denominator;

	Poco::DirectoryWatcher* directoryWatcher;
	uint64_t lastReloadTime;
	Poco::Mutex mutex;
	bool needsReload;

	virtual uint8_t doWork(uint8_t canSend) override;

	void fileChangedEvent(const void*, const Poco::DirectoryWatcher::DirectoryEvent&);

	void writeContent();

public:
	FilePort(AbstractOpenHAT* openhat, const char* id);

	virtual ~FilePort();

	virtual void configure(ConfigurationView* config, ConfigurationView* parentConfig);
};

///////////////////////////////////////////////////////////////////////////////
// Aggregator Port
///////////////////////////////////////////////////////////////////////////////

/** An AggregatorPort is a DigitalPort that collects the values of another port
* (the source port) and calculates with these historic values using specified
* calculations. The values can be multiplied by a specified factor to account
* for fractions that might otherwise be lost.
* The query interval and the number of values to collect must be specified.
* Additionally, a minimum and maximum delta value that must not be exceeded
* can be specified to validate the incoming values. A new value is compared against
* the last collected value. If the difference exceeds the specified bounds the
* whole collection is invalidated as a safeguard against implausible results.
* Each calculation presents its result as a DialPort. At startup all such ports
* will signal an error (value unavailable). The port values will also become invalid 
* if delta values are exceeded or if the source port signals an error.
* A typical example for using this port is with a gas counter. Such a counter
* emits impulses corresponding to a certain consumed gas value. If consumption
* is low, the impulses will typically arrive very slowly, perhaps once every few
* minutes. It is interesting to calculate the average gas consumption per hour
* or per day. To achieve this values are collected over the specified period in
* regular intervals. The Delta algorithm causes the value of the AggregatorPort
* to be set to the difference of the last and the first value (moving window).
* Another example is the calculation of averages, for example, temperature.
* In this case you should can use Average or ArithmeticMean for the algorithm.
* Using AllowIncomplete you can specify that the result should also be computed
* if not all necessary values have been collected. For an averaging algorithm
* the default is True, while for the Delta algorithm the default is false.
* In the case of temperature it might be interesting to determine the minimum
* and maximum in the specified range. 
* The allowedErrors setting specifies how many errors querying a port's value
* may occur in a row until the data is invalidated. In case of an error the last value
* is repeated if it exists.
* If the Persistent property of an AggregatorPort is true, the list of aggregated
* values as well as the last timestamp of the aggregator is stored in the persistent
* file. On startup, if there is a persisted state that is younger than the
* specified interval the values are read into the list.
* This behavior can be used to preserve values in between OpenHAT restarts.
*/
class AggregatorPort : public opdi::DigitalPort {
friend class AbstractOpenHAT;
protected:
	enum Algorithm {
		UNKNOWN,
		DELTA,
		ARITHMETIC_MEAN,
		MINIMUM,
		MAXIMUM
	};

	class Calculation : public opdi::DialPort {
	public:		
		Algorithm algorithm;
		bool allowIncomplete;

		Calculation(std::string id);

		void calculate(AggregatorPort* aggregator);
	};
	friend class AggregatorPort::Calculation;

	openhat::AbstractOpenHAT* openhat;
	std::string sourcePortID;
	opdi::Port* sourcePort;
	int64_t queryInterval;
	uint16_t totalValues;
	int32_t multiplier;
	int64_t minDelta;
	int64_t maxDelta;
	bool setHistory;
	std::string historyPortID;
	opdi::Port* historyPort;
	int32_t allowedErrors;

	std::vector<int64_t> values;
	uint64_t lastQueryTime;
	int32_t errors;
	bool firstRun;

	std::vector<Calculation*> calculations;

	virtual uint8_t doWork(uint8_t canSend) override;

	virtual void persist(void) override;

	void resetValues(std::string reason, opdi::LogVerbosity logVerbosity, bool clearPersistent = true);

public:
	AggregatorPort(AbstractOpenHAT* openhat, const char* id);

	~AggregatorPort();

	virtual void configure(ConfigurationView* portConfig, ConfigurationView* parentConfig);

	virtual void prepare() override;

	virtual void setLine(uint8_t newLine, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;
};

///////////////////////////////////////////////////////////////////////////////
// Counter Port
///////////////////////////////////////////////////////////////////////////////

/** A CounterPort is a dial port whose value increments linearly with time.
*   You can specify the period (in milliseconds) and the increment value.
*   A CounterPort can also count the events detected by a TriggerPort.
*   In this case, set the period to a value below 0 to only count the detected state changes.
*/
class CounterPort : public opdi::DialPort {

protected:
	openhat::AbstractOpenHAT* openhat;
	opdi::ValueResolver<int64_t> increment;
	opdi::ValueResolver<int64_t> periodMs;

	uint64_t lastActionTime;
public:

	CounterPort(AbstractOpenHAT* openhat, const char* id);

	virtual void configure(ConfigurationView* nodeConfig);

	virtual void prepare() override;

	virtual void doIncrement();

	virtual uint8_t doWork(uint8_t canSend) override;
};

///////////////////////////////////////////////////////////////////////////////
// Trigger Port
///////////////////////////////////////////////////////////////////////////////

/** A TriggerPort is a DigitalPort that continuously monitors one or more 
* digital ports for changes of their state. If a state change is detected it
* can change the state of other DigitalPorts.
* Triggering can happen on a change from Low to High (rising edge), from
* High to Low (falling edge), or on both changes.
* The effected state change can either be to set the output DigitalPorts
* High, Low, or toggle them. If you specify inverse output ports the state change
* is inverted, except for the Toggle specification.
* Disabling the TriggerPort sets all previously recorded port states to "unknown".
* No change is performed the first time a DigitalPort is read when its current
* state is unknown. A port that returns an error will also be set to "unknown".
*/
class TriggerPort : public opdi::DigitalPort {
protected:

	enum TriggerType {
		RISING_EDGE,
		FALLING_EDGE,
		BOTH
	};

	enum ChangeType {
		SET_HIGH,
		SET_LOW,
		TOGGLE
	};

	enum PortState {
		UNKNOWN,
		LOW,
		HIGH
	};

	openhat::AbstractOpenHAT* openhat;
	typedef Poco::Tuple<opdi::DigitalPort*, PortState> PortData;
	typedef std::vector<PortData> PortDataList;

	std::string inputPortStr;
	std::string outputPortStr;
	std::string inverseOutputPortStr;
	opdi::DigitalPortList outputPorts;
	opdi::DigitalPortList inverseOutputPorts;
	TriggerType triggerType;
	ChangeType changeType;
	CounterPort* counterPort;
	std::string counterPortStr;

	PortDataList portDataList;

	virtual uint8_t doWork(uint8_t canSend) override;

public:
	TriggerPort(AbstractOpenHAT* openhat, const char* id);

	virtual void configure(ConfigurationView* portConfig);

	virtual void prepare() override;

	virtual void setLine(uint8_t newLine, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;
};

/** An InfluxDB port is a DigitalPort that, when High, sends data to an InfluxDB instance
*   in regular intervals. Data is sent via HTTP using the InfluxDB API documented at
*   https://docs.influxdata.com/influxdb/v1.0/guides/writing_data/
*   The host must be specified. Default port is 8086.
*   You have to specify a database that must exist on the InfluxDB instance. You can 
*   specify an optional list of tags in format tag=value[,...]. You have to specify
*   a measurement and at least the ID of one port that is to be logged. The port IDs can
*   be specified using the port specification syntax (see findPortIDs).
*   The retention point is optional.
*   A port that returns an error while querying its value is omitted.
*   The timestamp sent is the current OPDI time in nanoseconds (with milliseconds precision).
*   You can specify an interval and a timeout. The interval must be greater than the
*   timeout.
*   An optional name for a fallback file can be specified that is written to in case of errors.
*   The content of this file can later be sent to InfluxDB to complete the missing data points.
*/
class InfluxDBPort : public opdi::DigitalPort, Poco::Runnable {
protected:
	openhat::AbstractOpenHAT* openhat;

	uint64_t intervalMs;	// milliseconds
	uint64_t timeoutMs;		// milliseconds
	std::string host;
	uint16_t tcpPort;
	std::string database;
	std::string retentionPoint;
	std::string measurement;
	std::string tags;		// InfluxDB tags, not port tags
	std::string portStr;	// specification for the findPortIDs function
	std::string fallbackFile;

	uint64_t lastLogTime;
	opdi::PortList ports;
	std::string dbData;		// the data to send asynchronously
	Poco::Thread postThread;

	virtual uint8_t doWork(uint8_t canSend) override;

	virtual void run();
public:
	InfluxDBPort(AbstractOpenHAT* openhat, const char* id);

	virtual void configure(ConfigurationView* portConfig);

	virtual void prepare() override;
};

}		// namespace openhat