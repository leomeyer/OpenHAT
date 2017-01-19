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

/// This port calculates a logic function over the Line values of Digital ports.
/// <a href="../../ports/logic_port">See the Logic port documentation.</a>
class LogicPort : public opdi::DigitalPort {
protected:
	enum LogicFunction {
		UNKNOWN,
		OR,
		AND,
		XOR,
		ATLEAST,
		ATMOST,
		EXACTLY
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

	/// Evaluates the logic function.
	///
	virtual uint8_t doWork(uint8_t canSend) override;

public:
	/// Creates a Logic port with the specified ID.
	///
	LogicPort(AbstractOpenHAT* openhat, const char* id);

	/// Configures the port.
	///
	virtual void configure(ConfigurationView* config);

	/// This method ensures that the Line of a Logic port cannot be set directly.
	///
	virtual void setLine(uint8_t line, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;

	/// Prepares the port for operation.
	///
	virtual void prepare() override;
};

///////////////////////////////////////////////////////////////////////////////
// Pulse Port
///////////////////////////////////////////////////////////////////////////////

/// This port generates a digital pulse with a defined period and duty cycle.
/// <a href="../../ports/pulse_port">See the Pulse port documentation.</a>
class PulsePort : public opdi::DigitalPort {
protected:
	openhat::AbstractOpenHAT* openhat;
	bool negate;
	opdi::ValueResolver<int32_t> period;
	opdi::ValueResolver<double> dutyCycle;
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

	/// This method generates the pulse if the Line is High.
	///
	virtual uint8_t doWork(uint8_t canSend) override;

public:
	/// Creates a Pulse port with the specified ID.
	///
	PulsePort(AbstractOpenHAT* openhat, const char* id);

	/// Configures the port.
	///
	virtual void configure(ConfigurationView* config);

	/// Prepares the port for operation.
	///
	virtual void prepare() override;
};

///////////////////////////////////////////////////////////////////////////////
// Selector Port
///////////////////////////////////////////////////////////////////////////////

/// This port implements a DigitalPort that is High when a specified Select port
/// is in a certain position and Low otherwise.
/// <a href="../../ports/selector_port">See the Selector port documentation.</a>
class SelectorPort : public opdi::DigitalPort {
protected:
	openhat::AbstractOpenHAT* openhat;
	std::string selectPortStr;
	opdi::SelectPort* selectPort;
	std::string outputPortStr;
	opdi::DigitalPortList outputPorts;
	int8_t errorState;
	uint16_t position;

	/// This method checks the target Select port.
	///
	virtual uint8_t doWork(uint8_t canSend) override;

public:
	/// Creates a Selector port with the specified ID.
	///
	SelectorPort(AbstractOpenHAT* openhat, const char* id);

	/// Configures the port.
	///
	virtual void configure(ConfigurationView* config);

	/// Sets the specified Select port to the defined position if line is 1.
	///
	virtual void setLine(uint8_t line, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;

	/// Prepares the port for operation.
	///
	virtual void prepare();
};

///////////////////////////////////////////////////////////////////////////////
// Error Detector Port
///////////////////////////////////////////////////////////////////////////////

/// This port implements a DigitalPort that is High when any of a number of 
/// specified ports is in an error state.
/// <a href="../../ports/error_detector_port">See the ErrorDetector port documentation.</a>
class ErrorDetectorPort : public opdi::DigitalPort {
protected:
	openhat::AbstractOpenHAT* openhat;
	bool negate;
	std::string inputPortStr;
	opdi::PortList inputPorts;

	/// This method checks whether any of the input ports has an error.
	///
	virtual uint8_t doWork(uint8_t canSend) override;

public:
	/// Creates an ErrorDetector port with the specified ID.
	///
	ErrorDetectorPort(AbstractOpenHAT* openhat, const char* id);

	/// Configures the port.
	///
	virtual void configure(ConfigurationView* config);

	/// Prepares the port for operation.
	///
	virtual void prepare() override;
};

///////////////////////////////////////////////////////////////////////////////
// Serial Streaming Port
///////////////////////////////////////////////////////////////////////////////

/// Defines a serial streaming port that supports streaming from and to a serial port device.
///
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
	/// Creates a SerialStreaming port with the specified ID.
	///
	SerialStreamingPort(AbstractOpenHAT* openhat, const char* id);

	/// Virtual destructor.
	///
	virtual ~SerialStreamingPort();

	/// Configures the port.
	///
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

/// This port collects values over time and performs statistical calculations.
/// <a href="../../ports/aggregator_port">See the Aggregator port documentation.</a>
class AggregatorPort : public opdi::DigitalPort {
friend class AbstractOpenHAT;
protected:
	/// Contains the algorithm types that can be used for calculations.
	///
	enum Algorithm {
		UNKNOWN,
		DELTA,
		ARITHMETIC_MEAN,
		MINIMUM,
		MAXIMUM
	};

	/// Encapsulates a calculation result as a dial port.
	///
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

	/// Collects the values in the specified interval and performs calculations.
	///
	virtual uint8_t doWork(uint8_t canSend) override;

	/// Persists the values in the persistent file if applicable.
	///
	virtual void persist(void) override;

	/// Clears the list of collected values and sets all calculation output ports
	/// to the error state "value not available".
	/// If clearPersistent is true the persistent storage is also cleared.
	void resetValues(std::string reason, opdi::LogVerbosity logVerbosity, bool clearPersistent = true);

public:
	AggregatorPort(AbstractOpenHAT* openhat, const char* id);

	virtual void configure(ConfigurationView* portConfig, ConfigurationView* parentConfig);

	/// Resolves port IDs.
	///
	virtual void prepare() override;

	/// Resets all values if the line is set to Low.
	///
	virtual void setLine(uint8_t newLine, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;
};

///////////////////////////////////////////////////////////////////////////////
// Counter Port
///////////////////////////////////////////////////////////////////////////////

/// This port implements a counter that increments with time.
/// <a href="../../ports/counter_port">See the Counter port documentation.</a>
class CounterPort : public opdi::DialPort {
protected:
	enum class TimeBase {
		SECONDS,
		MILLISECONDS,
		FRAMES
	};

	openhat::AbstractOpenHAT* openhat;

	TimeBase timeBase;
	std::string incrementStr;
	opdi::ValueResolver<int64_t> increment;
	std::string periodStr;
	opdi::ValueResolver<int64_t> period;
	std::string underflowPortStr;
	opdi::DigitalPortList underflowPorts;
	std::string overflowPortStr;
	opdi::DigitalPortList overflowPorts;

	uint64_t lastCountTime;
public:
	/// The unique type GUID of a Counter port.
	///
	static constexpr const char* TypeGUID = "26f2710c-05ac-4e14-b6d3-a2ec79252890";

	/// Creates a Counter port with the specified ID.
	///
	CounterPort(AbstractOpenHAT* openhat, const char* id);

	/// Configures the port from the specified configuration view.
	///
	virtual void configure(ConfigurationView* nodeConfig);

	/// Prepares the port for operation.
	///
	virtual void prepare() override;

	/// Increments the port's position by the specified increment.
	/// Handles over- and underflow logic. This method may only be called
	/// from within the doWork() method on the main thread!
	virtual void doIncrement(int64_t increment);

	/// Checks whether the period is up and increments the position if neccessary.
	///
	virtual uint8_t doWork(uint8_t canSend) override;
};

///////////////////////////////////////////////////////////////////////////////
// Trigger Port
///////////////////////////////////////////////////////////////////////////////

/// This port can monitor Digital ports for state changes.
/// <a href="../../ports/trigger_port">See the Trigger port documentation.</a>
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

/// This port can be used to test the runtime values of other ports.
/// <a href="../../ports/test_port">See the Test port documentation.</a>
class TestPort : public opdi::DigitalPort {
protected:
	enum class TimeBase {
		SECONDS,
		MILLISECONDS,
		FRAMES
	};

	openhat::AbstractOpenHAT* openhat;

	TimeBase timeBase;
	uint64_t interval;
	bool warnOnFailure;
	bool exitAfterTest;

	uint64_t lastExecution;

	std::map<std::string, std::string> testValues;

	/// Performs the tests if necessary.
	///
	virtual uint8_t doWork(uint8_t canSend) override;
public:
	/// Creates a Test port with the specified ID.
	///
	TestPort(AbstractOpenHAT* openhat, const char* id);

	/// Configures the port.
	///
	virtual void configure(ConfigurationView* portConfig, ConfigurationView* parentConfig);
};

/// This port can be used to assign state to target ports.
/// <a href="../../ports/assignment_port">See the Assigment port documentation.</a>
class AssignmentPort : public opdi::DigitalPort {
protected:
	openhat::AbstractOpenHAT* openhat;

	std::map<std::string, opdi::ValueResolver<double>> assignmentValues;

public:
	/// Creates an Assignment port with the specified ID.
	///
	AssignmentPort(AbstractOpenHAT* openhat, const char* id);

	/// Configures the port.
	///
	virtual void configure(ConfigurationView* portConfig, ConfigurationView* parentConfig);

	/// This method assigns the specified values to the target ports if the line is changed to High.
	/// The changeSource is propagated to the target ports so as it looks as if the changeSource
	/// was the same as the one that changed this port.
	virtual void setLine(uint8_t line, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;
};

}		// namespace openhat