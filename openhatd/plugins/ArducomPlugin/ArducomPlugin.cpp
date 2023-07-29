#include <sstream>

#include "Poco/Tuple.h"
#include "Poco/Runnable.h"
#include "Poco/Exception.h"
#include "Poco/Notification.h"
#include "Poco/NotificationQueue.h"
#include "Poco/AutoPtr.h"
#include "Poco/NumberParser.h"
#include "Poco/RegularExpression.h"

#include "OPDI_Ports.h"
#include "AbstractOpenHAT.h"
#include "ExpressionPort.h"

#include "TypeGUIDs.h"

// requires Arducom files, see https://github.com/leomeyer/Arducom
#include "ArducomMaster.h"
#include "ArducomMasterSerial.h"
// I2C is not supported on Windows
#ifndef _WINDOWS
#include "ArducomMasterI2C.h"
#endif

#define DEFAULT_QUERY_INTERVAL	5
#define DEFAULT_TIMEOUT			3
#define DEFAULT_RETRIES			3

namespace {

	class ArducomPort;

	class ActionNotification : public Poco::Notification {
	public:
		typedef Poco::AutoPtr<ActionNotification> Ptr;

		enum ActionType {
			READ,
			WRITE
		};

		ActionType type;
		ArducomPort* port;

		ActionNotification(ActionType type, ArducomPort* port) {
			this->type = type;
			this->port = port;
		}
	};

	////////////////////////////////////////////////////////////////////////
	// Plugin main class
	////////////////////////////////////////////////////////////////////////

	class ArducomPlugin : public IOpenHATPlugin, public openhat::IConnectionListener, public Poco::Runnable
	{
	protected:
		std::vector<ArducomPort*> myPorts;

	public:
		std::string nodeID;
		std::string type;
		std::string device;
		int address;
		int baudrate;	// only for serial connections
		int retries;
		int initDelay;
		uint32_t timeoutSeconds;

		int errorCount;
		std::string lastErrorMessage;

		opdi::LogVerbosity logVerbosity;

		Poco::NotificationQueue queue;
		Poco::Thread workThread;

		ArducomBaseParameters parameters;
		ArducomMasterTransport* transport;
		ArducomMaster* arducom;

		bool terminateRequested;

		void errorOccurred(const std::string& message);

		openhat::AbstractOpenHAT* openhat;

		virtual void masterConnected(void) override;
		virtual void masterDisconnected(void) override;

		virtual void run(void);

		virtual void setupPlugin(openhat::AbstractOpenHAT* abstractOpenHAT, const std::string& node, openhat::ConfigurationView::Ptr nodeConfig, openhat::ConfigurationView::Ptr parentConfig, const std::string& driverPath) override;

		virtual void terminate(void) override;
	};

	////////////////////////////////////////////////////////////////////////
	// Plugin ports
	////////////////////////////////////////////////////////////////////////

	class ArducomPort {
		friend class ArducomPlugin;
	protected:
		ArducomPlugin* plugin;
		bool valueSet;
		std::string newValue;
		uint64_t lastQueryTime;
		int32_t timeoutSeconds;
		uint32_t queryInterval;
		uint64_t timeoutCounter;
		Poco::Mutex mutex;
		opdi::LogVerbosity logVerbosity;

#if !defined(__CYGWIN__) && !defined(WIN32)
		int semkey;
#endif
		int8_t readCommand;
		std::string readParameters;
		Arducom::Format readParameterFormat;
		char readParameterSeparator;
		Arducom::Format readType;
		uint8_t readLength;
		uint8_t readOffset;
		std::string inputExpression;

		int8_t writeCommand;
		std::string writeParameters;
		Arducom::Format writeParameterFormat;
		char writeParameterSeparator;
		Arducom::Format writeType;
		std::string outputExpression;
		bool writeReturnsValue;

		openhat::ExpressionPort* expr;

	public:
		std::string pid;

		ArducomPort(ArducomPlugin* plugin, const std::string& id, opdi::LogVerbosity logVerbosity) {
			this->plugin = plugin;
			this->pid = id;
			this->valueSet = true;		// cause error state in doWork
			this->lastQueryTime = 0;
			this->timeoutSeconds = DEFAULT_TIMEOUT;
			this->queryInterval = DEFAULT_QUERY_INTERVAL;
			this->timeoutCounter = 0;
			this->logVerbosity = logVerbosity;

			this->readCommand = -1;
			this->readLength = 0;
			this->readOffset = 0;
			this->writeCommand = -1;
			this->writeReturnsValue = false;
		};

		virtual void configure(openhat::ConfigurationView::Ptr config) {
#if !defined(__CYGWIN__) && !defined(WIN32)
			this->semkey = config->getInt("SemaphoreKey", 0);
#endif

			this->logVerbosity = this->plugin->openhat->getConfigLogVerbosity(config, this->logVerbosity);

			std::string sRead = this->plugin->openhat->getConfigString(config, this->pid, "ReadCommand", "", false);
			if (!sRead.empty()) {
				int iRead = -1;
				try {
					iRead = Poco::NumberParser::parse(sRead);
				}
				catch (Poco::Exception&) {
					throw Poco::InvalidArgumentException(this->pid + ": Value for 'ReadCommand' must be an integer; got: '" + sRead + "'");
				}
				if (iRead < 0 || iRead > 127)
					throw Poco::InvalidArgumentException(this->pid + ": Value for 'ReadCommand' must be in range 0...127; got: '" + sRead + "'");
				else
					this->readCommand = iRead;

				this->readParameters = this->plugin->openhat->getConfigString(config, this->pid, "ReadParameters", "", false);

				std::string sReadParameterFormat = this->plugin->openhat->getConfigString(config, this->pid, "ReadParameterFormat", "Hex", false);
				this->readParameterFormat = Arducom::parseFormat(sReadParameterFormat, "ReadParameterFormat");

				std::string sReadParameterSeparator = this->plugin->openhat->getConfigString(config, this->pid, "ReadParameterSeparator", std::string(1, ARDUCOM_DEFAULT_SEPARATOR), false);
				if (sReadParameterSeparator.empty() || sReadParameterSeparator.size() > 1) {
					throw Poco::InvalidArgumentException(this->pid + ": ReadParameterSeparator must be a non-space string of length 1");
				}
				this->readParameterSeparator = sReadParameterSeparator[0];

				std::string sReadType = this->plugin->openhat->getConfigString(config, this->pid, "ReadType", "", true);
				this->readType = Arducom::parseFormat(sReadType, "ReadType");
				switch (this->readType) {
				case Arducom::FMT_BYTE: this->readLength = 1; break;
				case Arducom::FMT_INT16: this->readLength = 2; break;
				case Arducom::FMT_INT32: this->readLength = 4; break;
				case Arducom::FMT_INT64: this->readLength = 8; break;
				case Arducom::FMT_FLOAT:  this->readLength = 4; break;
				default:
					throw Poco::InvalidArgumentException(this->pid + ": ReadType must be one of: Byte, Int16, Int32, Int64, Float");
				}

				std::string sReadOffset = this->plugin->openhat->getConfigString(config, this->pid, "ReadOffset", "0", false);
				try {
					this->readOffset = Poco::NumberParser::parse(sReadOffset);
				}
				catch (Poco::Exception&) {
					throw Poco::InvalidArgumentException(this->pid + ": Value for 'ReadOffset' must be an integer; got: '" + sReadOffset + "'");
				}
				if (this->readOffset > 63)
					throw Poco::InvalidArgumentException(this->pid + ": Value for 'ReadOffset' must be in range 0...63; got: '" + sReadOffset + "'");

				this->inputExpression = config->getString("InputExpression", "");
				if (this->inputExpression.empty())
					this->inputExpression = "$value";
			}	// if (!sRead.empty())
				
			std::string sWrite = this->plugin->openhat->getConfigString(config, this->pid, "WriteCommand", "", false);
			if (!sWrite.empty()) {
				int iWrite = -1;
				try {
					iWrite = Poco::NumberParser::parse(sWrite);
				}
				catch (Poco::Exception&) {
					throw Poco::InvalidArgumentException(this->pid + ": Value for 'WriteCommand' must be an integer; got: '" + sWrite + "'");
				}
				if (iWrite < 0 || iWrite > 127)
					throw Poco::InvalidArgumentException(this->pid + ": Value for 'WriteCommand' must be in range 0...127; got: '" + sWrite + "'");
				this->writeCommand = iWrite;

				this->writeParameters = this->plugin->openhat->getConfigString(config, this->pid, "WriteParameters", "", false);

				std::string sWriteParameterFormat = this->plugin->openhat->getConfigString(config, this->pid, "WriteParameterFormat", "Hex", false);
				this->writeParameterFormat = Arducom::parseFormat(sWriteParameterFormat, "WriteParameterFormat");

				std::string sWriteParameterSeparator = this->plugin->openhat->getConfigString(config, this->pid, "WriteParameterSeparator", std::string(1, ARDUCOM_DEFAULT_SEPARATOR), false);
				if (sWriteParameterSeparator.empty() || sWriteParameterSeparator.size() > 1) {
					throw Poco::InvalidArgumentException(this->pid + ": WriteParameterSeparator must be a non-space string of length 1");
				}
				this->writeParameterSeparator = sWriteParameterSeparator[0];

				std::string sWriteType = this->plugin->openhat->getConfigString(config, this->pid, "WriteType", "", true);
				this->writeType = Arducom::parseFormat(sWriteType, "WriteType");
				switch (this->writeType) {
				case Arducom::FMT_BYTE:  break;
				case Arducom::FMT_INT16: break;
				case Arducom::FMT_INT32: break;
				case Arducom::FMT_INT64: break;
				case Arducom::FMT_FLOAT: break;
				default:
					throw Poco::InvalidArgumentException(this->pid + ": WriteType must be one of: Byte, Int16, Int32, Int64, Float");
				}

				this->writeReturnsValue = config->getBool("WriteReturnsValue", false);

				this->outputExpression = config->getString("OutputExpression", "");
				if (this->outputExpression.empty())
					this->outputExpression = "$value";
			}	// if (!sWrite.empty())

			this->expr = new openhat::ExpressionPort(this->plugin->openhat, (this->pid + "_inputExpr").c_str());
		};

		std::size_t replace_all(std::string& inout, std::string what, std::string with)
		{
			std::size_t count{};
			for (std::string::size_type pos{};
				inout.npos != (pos = inout.find(what.data(), pos, what.length()));
				pos += with.length(), ++count) {
				inout.replace(pos, what.length(), with.data(), with.length());
			}
			return count;
		}

		std::string get_what(const std::exception& e) {
			std::stringstream ss;
			ss << e.what();
			try {
				std::rethrow_if_nested(e);
			}
			catch (const std::exception& nested) {
				ss << ": ";
				ss << get_what(nested);
			}
			return ss.str();
		}

		int64_t getResult(uint8_t command, uint8_t* buffer, uint8_t size, std::string& expression) {
			// result data must contain at least (readOffset + readLength) bytes
			if (size < (this->readOffset + this->readLength))
				throw Poco::Exception(this->pid + ": The result of Arducom command " + this->plugin->openhat->to_string((int)command)
					+ " did not contain the minimum number of expected bytes (" + this->plugin->openhat->to_string((this->readOffset + this->readLength))
					+ ") but only " + this->plugin->openhat->to_string((int)size) + " byte(s)");

			this->plugin->openhat->logDebug(this->pid + ": Evaluating result of command "
				+ this->plugin->openhat->to_string((int)command) + "; payload size: " + this->plugin->openhat->to_string((int)size), this->logVerbosity);

			// convert the result into a string for expression evaluation
			std::string value;
			switch (this->readType) {
			case Arducom::FMT_BYTE: { uint8_t b = *((uint8_t*)buffer); value = this->plugin->openhat->to_string((int)b); break; }
			case Arducom::FMT_INT16: { int16_t s = *((uint16_t*)buffer); value = this->plugin->openhat->to_string(s); break; }
			case Arducom::FMT_INT32: { int32_t i = *((uint32_t*)buffer); value = this->plugin->openhat->to_string(i); break; }
			case Arducom::FMT_INT64: { int64_t l = *((uint64_t*)buffer); value = this->plugin->openhat->to_string(l); break; }
			case Arducom::FMT_FLOAT: { float f = *((float*)buffer); value = this->plugin->openhat->to_string(f); break; }
			default:
				throw Poco::InvalidArgumentException(this->pid + ": ReadType must be one of: Byte, Int16, Int32, Int64, Float");
			}

			// replace $value in expression with received value or use the value as it is as a fallback
			std::string valueExpression(expression);
			if (replace_all(valueExpression, "$value", value) == 0)
				valueExpression = value;

			this->expr->expressionStr = valueExpression;
			this->expr->prepare();
			double dResult = this->expr->apply();

			return (int64_t)dResult;
		}

		int64_t executeRead() {
			std::vector<uint8_t> payload;
			uint8_t size;
			uint8_t destBuffer[ARDUCOM_BUFFERSIZE];
			uint8_t errorInfo;

			if (!this->readParameters.empty())
				Arducom::parsePayload(this->readParameters, this->readParameterFormat, this->readParameterSeparator, payload);

			size = (uint8_t)payload.size();

			this->plugin->openhat->logDebug(this->pid + ": executeRead with command "
				+ this->plugin->openhat->to_string((int)this->readCommand) + "; payload size: " + this->plugin->openhat->to_string((int)size), this->logVerbosity);

			this->plugin->arducom->execute(this->plugin->parameters, this->readCommand, payload.data(), &size,
				this->plugin->transport->getDefaultExpectedBytes(), destBuffer, &errorInfo);

			return getResult(this->readCommand, destBuffer, size, this->inputExpression);
		}

		void checkRefresh() {
			// time for refresh?
			if ((this->queryInterval > 0) && (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000ul)) {
				this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::READ, this));
				this->lastQueryTime = opdi_get_time_ms();
			}
		}

		// called asynchronously by notification handler thread
		virtual void read() {
			Poco::Mutex::ScopedLock lock(this->mutex);
			this->lastQueryTime = opdi_get_time_ms();
			this->mutex.unlock();

			try {
				std::string value = this->plugin->openhat->to_string(this->executeRead());

				Poco::Mutex::ScopedLock lock(this->mutex);
				this->newValue = value;
				this->valueSet = true;
			}
			catch (Poco::Exception& e) {
				this->plugin->openhat->logWarning(this->pid + ": executeRead: " + e.message());
			}
			catch (std::exception& e) {
				this->plugin->openhat->logWarning(this->pid + ": executeRead: " + this->get_what(e));
			}
		};

		virtual int64_t getValueToWrite() = 0;

		int64_t executeWrite(double value) {
			std::vector<uint8_t> payload;
#define SHIFT_TO_BYTE(v, n)	(uint8_t)(v >> n)
			uint8_t size;
			uint8_t destBuffer[ARDUCOM_BUFFERSIZE];
			uint8_t errorInfo;

			if (!this->writeParameters.empty())
				Arducom::parsePayload(this->writeParameters, this->writeParameterFormat, this->writeParameterSeparator, payload);

			// append the bytes of the value using the provided writeFormat
			switch (this->writeType) {
			case Arducom::FMT_BYTE: {
				if (value < 0 || value > 255)
					throw Poco::InvalidArgumentException("Write value out of range for type 'Byte': " + this->plugin->openhat->to_string(value));
				uint8_t b = (uint8_t)value;
				payload.push_back(b); 
				break;
			}
			case Arducom::FMT_INT16: {
				if (value < (double)INT16_MIN || value > (double)INT16_MAX)
					throw Poco::InvalidArgumentException("Write value out of range for type 'Int16': " + this->plugin->openhat->to_string(value));
				int16_t s = (uint16_t)value;
				payload.push_back((uint8_t)s); payload.push_back(SHIFT_TO_BYTE(s, 8));
				break;
			}
			case Arducom::FMT_INT32: {
				if (value < (double)INT32_MIN || value >(double)INT32_MAX)
					throw Poco::InvalidArgumentException("Write value out of range for type 'Int32': " + this->plugin->openhat->to_string(value));
				int32_t i = (uint32_t)value;
				payload.push_back(i); payload.push_back(SHIFT_TO_BYTE(i, 8));
				payload.push_back(SHIFT_TO_BYTE(i, 16)); payload.push_back(SHIFT_TO_BYTE(i, 24));
				break;
			}
			case Arducom::FMT_INT64: {
				if (value < (double)INT64_MIN || value >(double)INT64_MAX)
					throw Poco::InvalidArgumentException("Write value out of range for type 'Int64': " + this->plugin->openhat->to_string(value));
				int64_t l = (uint64_t)value;
				payload.push_back((uint8_t)l); payload.push_back(SHIFT_TO_BYTE(l, 8));
				payload.push_back(SHIFT_TO_BYTE(l, 16)); payload.push_back(SHIFT_TO_BYTE(l, 24));
				payload.push_back(SHIFT_TO_BYTE(l, 32)); payload.push_back(SHIFT_TO_BYTE(l, 40));
				payload.push_back(SHIFT_TO_BYTE(l, 48)); payload.push_back(SHIFT_TO_BYTE(l, 56));
				break;
			}
			case Arducom::FMT_FLOAT: {
				if (value < std::numeric_limits<float>::min() || value > std::numeric_limits<float>::max())
					throw Poco::InvalidArgumentException("Write value out of range for type 'Float': " + this->plugin->openhat->to_string(value));
				float f = (float)value;
				uint8_t* buf = ((uint8_t*)&f);
				payload.push_back(buf[0]); payload.push_back(buf[1]);
				payload.push_back(buf[2]); payload.push_back(buf[3]);
				break;
			}
			default:
				throw Poco::InvalidArgumentException(this->pid + ": WriteType must be one of: Byte, Int16, Int32, Int64, Float");
			}

			size = (uint8_t)payload.size();

			this->plugin->openhat->logDebug(this->pid + ": executeWrite with command "
				+ this->plugin->openhat->to_string((int)this->writeCommand) + "; payload size: " + this->plugin->openhat->to_string((int)size), this->logVerbosity);

			this->plugin->arducom->execute(this->plugin->parameters, this->writeCommand, payload.data(), &size,
				this->plugin->transport->getDefaultExpectedBytes(), destBuffer, &errorInfo);

			if (this->writeReturnsValue)
				return getResult(this->writeCommand, destBuffer, size, this->inputExpression);
			else
				return 0;
		}

		// called asynchronously by notification handler thread
		virtual void write() {
			Poco::Mutex::ScopedLock lock(this->mutex);

			std::string value = this->plugin->openhat->to_string(this->getValueToWrite());
			this->mutex.unlock();

			// replace $value in expression with received value or use the value as it is as a fallback
			std::string valueExpression(this->outputExpression);
			if (replace_all(valueExpression, "$value", value) == 0)
				valueExpression = value;

			this->expr->expressionStr = valueExpression;
			this->expr->prepare();
			double dResult = this->expr->apply();
			try {
				int64_t result = this->executeWrite(dResult);
				if (this->writeReturnsValue) {
					// use returned value
					std::string value = this->plugin->openhat->to_string(result);

					Poco::Mutex::ScopedLock lock(this->mutex);
					this->newValue = value;
					this->valueSet = true;
					this->lastQueryTime = opdi_get_time_ms();
				}
				else {
					// immediately queue a read notification to reflect the changed value
					this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::READ, this));
				}
			}
			catch (Poco::Exception& e) {
				this->plugin->openhat->logWarning(this->pid + ": executeWrite: " + e.message());
				// immediately queue a read notification to read a valid value
				this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::READ, this));
			}
			catch (std::exception& e) {
				this->plugin->openhat->logWarning(this->pid + ": executeWrite: " + this->get_what(e));
				// immediately queue a read notification to read a valid value
				this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::READ, this));
			}
		}
	};

	class DigitalPort : public ArducomPort, public opdi::DigitalPort {
	protected:
		std::string inputValueLow;
		std::string inputValueHigh;
		std::string outputValueLow;
		std::string outputValueHigh;

	public:
		DigitalPort(ArducomPlugin* plugin, const std::string& id, opdi::LogVerbosity logVerbosity) : ArducomPort(plugin, id, logVerbosity), 
			opdi::DigitalPort(id.c_str()) {
			this->setMode(OPDI_DIGITAL_MODE_OUTPUT);
		}

		virtual void configure(openhat::ConfigurationView::Ptr config) {
			this->plugin->openhat->configureDigitalPort(config, this, false);

			ArducomPort::configure(config);

			// if there is no write command the port is input-only and readonly
			if (this->writeCommand < 0) {
				this->setDirCaps(OPDI_PORTDIRCAP_INPUT); // sets mode automatically
				setReadonly(true);
			}

			this->inputValueLow = config->getString("InputValueLow");
			this->inputValueHigh = config->getString("InputValueHigh");
			this->outputValueLow = config->getString("OutputValueLow", "");
			this->outputValueHigh = config->getString("OutputValueHigh", "");
		};

		virtual uint8_t doWork(uint8_t canSend) override {
			opdi::DigitalPort::doWork(canSend);

			Poco::Mutex::ScopedLock lock(this->mutex);

			// value received?
			if (this->valueSet) {
				this->plugin->openhat->logDebug(this->pid + ": Value received: '" + this->newValue + "'", opdi::DigitalPort::logVerbosity);

				try {
					// compare received value
					if (this->newValue == this->inputValueLow) {
						opdi::DigitalPort::setLine(OPDI_DIGITAL_LINE_LOW);
					}
					else
					if (this->newValue == this->inputValueHigh) {
						opdi::DigitalPort::setLine(OPDI_DIGITAL_LINE_HIGH);
					}
					else {
						this->plugin->openhat->logWarning(this->pid + ": Received unspecified value for digital port: " + this->newValue);
						this->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
					}
				}
				catch (std::exception& e) {
					this->plugin->openhat->logError(this->pid + ": Error setting DigitalPort line: " + get_what(e));
				}
				this->valueSet = false;
			}

			this->checkRefresh();

			return OPDI_STATUS_OK;
		};

		virtual bool setLine(uint8_t line, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT) override {
			if (!opdi::DigitalPort::setLine(line, changeSource))
				return false;

			this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::WRITE, this));

			return true;
		};

		virtual int64_t getValueToWrite() override {
			this->checkError();

			return this->getLine();
		};
	};

	class DialPort : public ArducomPort, public opdi::DialPort {
	public:
		DialPort(ArducomPlugin* plugin, const std::string& id, opdi::LogVerbosity logVerbosity) : ArducomPort(plugin, id, logVerbosity), 
			opdi::DialPort(id.c_str(), -999999999999, 999999999999, 1) {
		}

		virtual void configure(openhat::ConfigurationView::Ptr config) override {
			this->plugin->openhat->configureDialPort(config, this, false);

			ArducomPort::configure(config);

			// if there is no write command the port is input-only and readonly
			if (this->writeCommand < 0) {
				this->setDirCaps(OPDI_PORTDIRCAP_INPUT); // sets mode automatically
				setReadonly(true);
			}
		}

		virtual uint8_t doWork(uint8_t canSend) override {
			opdi::DialPort::doWork(canSend);

			Poco::Mutex::ScopedLock lock(this->mutex);

			// value received?
			if (this->valueSet) {
				this->plugin->openhat->logDebug(this->pid + ": Value received: '" + this->newValue + "'", opdi::DialPort::logVerbosity);

				try {
					if (!this->newValue.empty()) {
						int64_t pos = Poco::NumberParser::parse(this->newValue);
						opdi::DialPort::setPosition(pos);
					}
					else {
						this->plugin->openhat->logWarning(this->pid + ": Missing value for dial port");
						this->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
					}
				}
				catch (std::exception& e) {
					this->plugin->openhat->logError(this->pid + ": Error setting DialPort line: " + e.what());
				}
				this->valueSet = false;
			}

			this->checkRefresh();

			return OPDI_STATUS_OK;
		};

		virtual bool setPosition(int64_t position, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT) override {
			if (!opdi::DialPort::setPosition(position, changeSource))
				return false;

			this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::WRITE, this));

			return true;
		};

		virtual int64_t getValueToWrite() override {
			this->checkError();

			return this->getPosition();
		};
	};

	class SelectPort : public ArducomPort, public opdi::SelectPort {
	protected:
		std::string inputMap;
		std::vector<std::string> inputValues;
		std::string outputMap;
		std::vector<std::string> outputValues;

		void tokenize(std::string const& str, const char delim,
			std::vector<std::string>& out)
		{
			size_t start;
			size_t end = 0;

			while ((start = str.find_first_not_of(delim, end)) != std::string::npos) {
				end = str.find(delim, start);
				out.push_back(str.substr(start, end - start));
			}
		};

	public:
		SelectPort(ArducomPlugin* plugin, const std::string& id, opdi::LogVerbosity logVerbosity) : ArducomPort(plugin, id, logVerbosity), 
			opdi::SelectPort(id.c_str()) {
		};

		virtual void configure(openhat::ConfigurationView::Ptr config, openhat::ConfigurationView::Ptr parentConfig) {
			this->plugin->openhat->configureSelectPort(config, parentConfig, this, false);

			ArducomPort::configure(config);

			// if there is no write command the port is input-only and readonly
			if (this->writeCommand < 0) {
				this->setDirCaps(OPDI_PORTDIRCAP_INPUT); // sets mode automatically
				setReadonly(true);
			}

			this->inputMap = this->plugin->openhat->getConfigString(config, this->ID(), "InputMap", "", false);
			this->tokenize(this->inputMap, '|', this->inputValues);

			this->outputMap = this->plugin->openhat->getConfigString(config, this->ID(), "OutputMap", "", false);
			this->tokenize(this->outputMap, '|', this->outputValues);
		};

		virtual uint8_t doWork(uint8_t canSend) override {
			opdi::SelectPort::doWork(canSend);

			Poco::Mutex::ScopedLock lock(this->mutex);

			if (this->valueSet) {
				this->plugin->openhat->logDebug(this->pid + ": Value received: '" + this->newValue + "'", opdi::SelectPort::logVerbosity);

				this->valueSet = false;

				if (this->newValue.empty()) {
					this->plugin->openhat->logWarning(this->pid + ": Received value is empty");
					// set to error state
					this->setError(Error::VALUE_NOT_AVAILABLE);
					return OPDI_STATUS_OK;
				}

				uint16_t position;

				// input map specified?
				if (this->inputValues.size() > 0) {
					// locate input string in input map
					auto it = std::find(this->inputValues.begin(), this->inputValues.end(), this->newValue);

					// element not found
					if (it == this->inputValues.end()) {
						this->plugin->openhat->logWarning(this->pid + ": Received value not in InputMap: '" + this->newValue + "'");
						// set to error state
						this->setError(Error::VALUE_NOT_AVAILABLE);
						return OPDI_STATUS_OK;
					}

					int index = it - this->inputValues.begin();
					if (index > this->getMaxPosition()) {
						this->plugin->openhat->logWarning(this->pid + ": Received value index in InputMap exceeds maximum position " + this->plugin->openhat->to_string(this->getMaxPosition()) + ": '" + this->newValue + "'");
						// set to error state
						this->setError(Error::VALUE_NOT_AVAILABLE);
						return OPDI_STATUS_OK;
					}
					position = index;
				}
				else {
					// no input map specified, use the input value to match orderID of label to select the position
					int orderID;
					try {
						orderID = Poco::NumberParser::parse(this->newValue);
					}
					catch (Poco::Exception&) {
						this->plugin->openhat->logWarning(this->pid + ": Received value must be an integer; got: '" + this->newValue + "'");
						// set to error state
						this->setError(Error::VALUE_NOT_AVAILABLE);
						return OPDI_STATUS_OK;
					}

					try {
						position = this->getPositionByLabelOrderID(orderID);
					}
					catch (Poco::Exception& e) {
						this->plugin->openhat->logWarning(this->pid + ": Unable to find the specified position: " + this->plugin->openhat->getExceptionMessage(e));
						// set to error state
						this->setError(Error::VALUE_NOT_AVAILABLE);
						return OPDI_STATUS_OK;
					}
				}

				try {
					// use base method to avoid triggering an action!
					opdi::SelectPort::setPosition(position);
				}
				catch (std::exception& e) {
					this->plugin->openhat->logError(this->pid + ": Error setting SelectPort position: " + get_what(e));
				}
			}

			this->checkRefresh();

			return OPDI_STATUS_OK;
		};

		virtual bool setPosition(uint16_t value, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT) override {
			if (!opdi::SelectPort::setPosition(value, changeSource))
				return false;

			this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::WRITE, this));

			return true;
		};

		virtual int64_t getValueToWrite() override {
			this->checkError();

			// output map specified?
			if (this->outputValues.size() > 0) {
				if (this->outputValues.size() < this->getPosition() + 1)
					throw Poco::Exception("Not enough values in output map (position: " + this->plugin->openhat->to_string(this->getPosition()));

				std::string sValue = this->outputValues[this->getPosition()];

				return Poco::NumberParser::parse64(sValue);
			}
			else {
				// no output map; use orderID of selected label
				return this->getLabelAt(this->getPosition()).get<0>();
			}
		};
	};
/*
	class GenericPort : public opdi::CustomPort, public ArducomPort {
		friend class ArducomPlugin;
	protected:
		std::string myValue;
		std::string initialValue;
	public:
		GenericPort(ArducomPlugin* plugin, const std::string& id, const std::string& topic);

		virtual uint8_t doWork(uint8_t canSend) override;

		virtual void setValue(const std::string& value, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT) override;

		virtual void configure(Poco::Util::AbstractConfiguration::Ptr config) override;
	};
*/
} // end of anonymous namespace

////////////////////////////////////////////////////////
// Plugin implementation
////////////////////////////////////////////////////////

// Credits: Sir Slick, https://stackoverflow.com/questions/2589096/find-most-significant-bit-left-most-that-is-set-in-a-bit-array
unsigned int msb32(unsigned int x)
{
	static const unsigned int bval[] =
	{ 0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4 };

	unsigned int r = 0;
	if (x & 0xFFFF0000) { r += 16 / 1; x >>= 16 / 1; }
	if (x & 0x0000FF00) { r += 16 / 2; x >>= 16 / 2; }
	if (x & 0x000000F0) { r += 16 / 4; x >>= 16 / 4; }
	return r + bval[x];
}

void ArducomPlugin::errorOccurred(const std::string& message) {
	// This method is for errors that are usually logged in verbosity Normal.
	// It suppresses frequent occurrences of the same error.
	// identical error message?
	if (this->lastErrorMessage == message) {
		this->errorCount++;
	} else {
		// different error, reset counter
		this->errorCount = 1;
		this->lastErrorMessage = message;
	}
	// the error message is output if only the highest bit is set causing exponentially decreasing output frequency
	// get highest bit
	unsigned int msb = msb32(this->errorCount);
	// are the lower bits zero?
	if ((this->errorCount & ((1 << (msb - 1)) - 1)) == 0) {
		// add occurrence count if greater than 1
		this->openhat->logNormal((this->errorCount > 1 ? "(" + this->openhat->to_string(this->errorCount) + ") " : "") + message, this->logVerbosity);
	}
}

void ArducomPlugin::setupPlugin(openhat::AbstractOpenHAT* abstractOpenHAT, const std::string& node, openhat::ConfigurationView::Ptr nodeConfig, openhat::ConfigurationView::Ptr parentConfig, const std::string& /*driverPath*/) {
	this->openhat = abstractOpenHAT;
	this->nodeID = node;
	this->timeoutSeconds = DEFAULT_TIMEOUT;	// time without received payloads until the devices go into error mode

	this->errorCount = 0;

	// store main node's verbosity level (will become the default of ports)
	this->logVerbosity = this->openhat->getConfigLogVerbosity(nodeConfig, this->openhat->getLogVerbosity());

	// get Arducom connection details
	// initialize the parameters
	parameters.transportType = this->openhat->getConfigString(nodeConfig, node, "Transport", "", false);
	parameters.device = this->openhat->getConfigString(nodeConfig, node, "Device", "", true);
	parameters.deviceAddress = nodeConfig->getInt("Address", 0);
	parameters.baudrate = nodeConfig->getInt("Baudrate", ARDUCOM_DEFAULT_BAUDRATE);
	parameters.retries = nodeConfig->getInt("Retries", DEFAULT_RETRIES);
	parameters.initDelayMs = nodeConfig->getInt("InitDelayMs", 0);
	parameters.timeoutMs = nodeConfig->getInt("Timeout", this->timeoutSeconds) * 1000;

	parameters.debug = this->logVerbosity >= opdi::LogVerbosity::DEBUG;
	parameters.verbose = this->logVerbosity >= opdi::LogVerbosity::EXTREME;

	try {
		// validate parameters
		this->transport = parameters.validate();
	}
	catch (std::exception& e) {
		this->openhat->throwSettingException(node + ": Error initializing Arducom transport: " + e.what());
	}
	this->openhat->logDebug(this->nodeID + ": Initializing Arducom master", this->logVerbosity);
	this->arducom = new ArducomMaster(transport);

	this->terminateRequested = false;

	// store main node's group (will become the default of ports)
	std::string group = nodeConfig->getString("Group", "");

	// enumerate keys of the plugin's nodes (in specified order)
	this->openhat->logVerbose(node + ": Enumerating Arducom ports: " + node + ".Ports", this->logVerbosity);

	Poco::AutoPtr<openhat::ConfigurationView> nodes = this->openhat->createConfigView(nodeConfig, "Ports");
	nodeConfig->addUsedKey("Ports");

	// get ordered list of ports
	openhat::ConfigurationView::Keys portKeys;
	nodes->keys("", portKeys);

	typedef Poco::Tuple<int, std::string> Item;
	typedef std::vector<Item> ItemList;
	ItemList orderedItems;

	// create ordered list of port keys (by priority)
	for (auto it = portKeys.begin(), ite = portKeys.end(); it != ite; ++it) {

		int itemNumber = nodes->getInt(*it, 0);
		// check whether the item is active
		if (itemNumber < 0)
			continue;

		// insert at the correct position to create a sorted list of items
		auto nli = orderedItems.begin();
		auto nlie = orderedItems.end();
		while (nli != nlie) {
			if (nli->get<0>() > itemNumber)
				break;
			++nli;
		}
		Item item(itemNumber, *it);
		orderedItems.insert(nli, item);
	}

	if (orderedItems.size() == 0) {
		this->openhat->logWarning("No ports configured in " + node + ".Ports; is this intended?");
	}

	// go through items, create ports in specified order
	auto nli = orderedItems.begin();
	auto nlie = orderedItems.end();
	while (nli != nlie) {
		std::string portName = nli->get<1>();

		this->openhat->logVerbose(node + ": Setting up Arducom port: " + portName, this->logVerbosity);

		// get port section from the configuration>
		Poco::AutoPtr<openhat::ConfigurationView> portConfig = this->openhat->createConfigView(parentConfig, portName);

		// get port type (required)
		std::string portType = this->openhat->getConfigString(portConfig, portName, "Type", "", true);

		int timeout = portConfig->getInt("Timeout", this->timeoutSeconds);

		int queryInterval = portConfig->getInt("QueryInterval", DEFAULT_QUERY_INTERVAL);
		if (queryInterval < 0)
			this->openhat->throwSettingException(portName + ": Please specify a QueryInterval >= 0: " + this->openhat->to_string(queryInterval));

		if (portType == "DigitalPort") {
			this->openhat->logVerbose(node + ": Setting up Arducom digital port: " + portName, this->logVerbosity);
			// setup the port instance and add it
			DigitalPort* digitalPort = new DigitalPort(this, portName, this->logVerbosity);
			digitalPort->timeoutSeconds = timeout;
			digitalPort->queryInterval = queryInterval;
			// set default group: Arducom node's group
			digitalPort->setGroup(group);
			// set default log verbosity
			digitalPort->setLogVerbosity(this->logVerbosity);
			digitalPort->configure(portConfig);
			this->openhat->addPort(digitalPort);
			this->myPorts.push_back(digitalPort);
		}
		else if (portType == "DialPort") {
			this->openhat->logVerbose(node + ": Setting up Arducom dial port: " + portName, this->logVerbosity);
			// setup the port instance and add it
			DialPort* dialPort = new DialPort(this, portName, this->logVerbosity);
			dialPort->timeoutSeconds = timeout;
			dialPort->queryInterval = queryInterval;
			// set default group: Arducom node's group
			dialPort->setGroup(group);
			// set default log verbosity
			dialPort->setLogVerbosity(this->logVerbosity);
			dialPort->configure(portConfig);
			this->openhat->addPort(dialPort);
			this->myPorts.push_back(dialPort);
		}
		else if (portType == "SelectPort") {
			this->openhat->logVerbose(node + ": Setting up Arducom select port: " + portName, this->logVerbosity);
			// setup the port instance and add it
			SelectPort* selectPort = new SelectPort(this, portName, this->logVerbosity);
			selectPort->timeoutSeconds = timeout;
			selectPort->queryInterval = queryInterval;
			// set default group: Arducom node's group
			selectPort->setGroup(group);
			// set default log verbosity
			selectPort->setLogVerbosity(this->logVerbosity);
			selectPort->configure(portConfig, nodeConfig);
			this->openhat->addPort(selectPort);
			this->myPorts.push_back(selectPort);
		}
/*
		else if (portType == "Generic") {
			this->openhat->logVerbose(node + ": Setting up generic Arducom port: " + portName, this->logVerbosity);
			// get topic (required)
			std::string topic = this->openhat->getConfigString(portConfig, portName, "Topic", "", true);

			// setup the generic port instance and add it
			GenericPort* genericPort = new GenericPort(this, portName, topic);
			genericPort->timeoutSeconds = timeout;
			genericPort->queryInterval = queryInterval;
			// set default group: MQTT's node's group
			genericPort->setGroup(group);
			// set default log verbosity
			genericPort->logVerbosity = this->logVerbosity;
			this->openhat->configureCustomPort(portConfig, genericPort);
			this->openhat->addPort(genericPort);
			this->myPorts.push_back(genericPort);
		}
*/
		else
			this->openhat->throwSettingException(node + ": This plugin does not support the port type '" + portType + "'");

		++nli;
	}

	// this->openhat->addConnectionListener(this);

	this->workThread.setName(node + " work thread");
	this->workThread.start(*this);

	this->openhat->logVerbose(node + ": ArducomPlugin setup completed successfully", this->logVerbosity);
}

void ArducomPlugin::masterConnected() {
}

void ArducomPlugin::masterDisconnected() {
}

void ArducomPlugin::run(void) {

	this->openhat->logVerbose(this->nodeID + ": ArducomPlugin worker thread started", this->logVerbosity);

	while (!this->openhat->shutdownRequested && !this->terminateRequested) {
		if (this->arducom != nullptr) {
			try {
				Poco::Notification::Ptr notification = this->queue.waitDequeueNotification(100);
				if (!this->openhat->shutdownRequested && notification) {
					ActionNotification::Ptr workNf = notification.cast<ActionNotification>();
					if (workNf) {
						std::string action;
						switch (workNf->type) {
						case ActionNotification::READ: action = "READ"; break;
						case ActionNotification::WRITE: action = "WRITE"; break;
						}
						ArducomPort* port = (ArducomPort*)workNf->port;

						this->openhat->logExtreme(this->nodeID + ": Processing requested action: " + action + " for port: " + port->pid, this->logVerbosity);
						// inspect action and decide what to do
						switch (workNf->type) {
						case ActionNotification::READ: {
							workNf->port->read();
							break;
						}
						case ActionNotification::WRITE: {
							workNf->port->write();
							break;
						}
						}
					}
				}
			} catch (Poco::Exception &e) {
				this->openhat->logNormal(this->nodeID + ": Unhandled exception in worker thread: " + this->openhat->getExceptionMessage(e), this->logVerbosity);
			}
		} else
			this->openhat->logExtreme(this->nodeID + ": Client not connected", this->logVerbosity);
	}
	this->openhat->logVerbose(this->nodeID + ": ArducomPlugin worker thread terminated", this->logVerbosity);
}

void ArducomPlugin::terminate() {
//	this->terminateRequested = true;
//	while (this->workThread.isRunning())
//		Poco::Thread::current()->yield();
}


// plugin instance factory function

#ifdef _WINDOWS

extern "C" __declspec(dllexport) IOpenHATPlugin* __cdecl GetPluginInstance(int majorVersion, int minorVersion, int patchVersion)

#elif linux

extern "C" IOpenHATPlugin* GetPluginInstance(int majorVersion, int minorVersion, int /*patchVersion*/)

#else
#error "Unable to compile plugin instance factory function: Compiler not supported"
#endif
{
	// check whether the version is supported
	if ((majorVersion != openhat::OPENHAT_MAJOR_VERSION) || (minorVersion != openhat::OPENHAT_MINOR_VERSION))
		throw Poco::Exception("This plugin requires OpenHAT version "
			OPDI_QUOTE(openhat::OPENHAT_MAJOR_VERSION) "." OPDI_QUOTE(openhat::OPENHAT_MINOR_VERSION));

	// return a new instance of this plugin
	return new ArducomPlugin();
}
