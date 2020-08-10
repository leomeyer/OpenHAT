#include "Ports.h"

#include <bitset>
#include <cmath>
#include <numeric>
#include <functional>

#include "Poco/String.h"
#include "Poco/Timezone.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/NumberParser.h"
#include "Poco/Format.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/BasicEvent.h"
#include "Poco/Delegate.h"
#include "Poco/ScopedLock.h"
#include "Poco/FileStream.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPClientSession.h"

#include "opdi_platformfuncs.h"

namespace openhat {

///////////////////////////////////////////////////////////////////////////////
// Logic Port
///////////////////////////////////////////////////////////////////////////////

LogicPort::LogicPort(AbstractOpenHAT* openhat, const char* id) : opdi::DigitalPort(id, OPDI_PORTDIRCAP_OUTPUT, 0) {
	this->opdi = this->openhat = openhat;
	this->function = UNKNOWN;
	this->funcN = -1;
	this->negate = false;

	opdi::DigitalPort::setMode(OPDI_DIGITAL_MODE_OUTPUT);
	// set the line to an invalid state
	// this will trigger the detection logic in the first call of doWork
	// so that all dependent output ports will be set to a defined state
	this->line = -1;
}

void LogicPort::configure(ConfigurationView* config) {
	this->openhat->configureDigitalPort(config, this, false);
	this->logVerbosity = this->openhat->getConfigLogVerbosity(config, opdi::LogVerbosity::UNKNOWN);

	std::string function = config->getString("Function", "OR");

	try {
		if (function == "OR")
			this->function = OR;
		else
		if (function == "AND")
			this->function = AND;
		else
		if (function == "XOR")
			this->function = XOR;
		else {
			std::string prefix("ATLEAST");
			if (!function.compare(0, prefix.size(), prefix)) {
				this->function = ATLEAST;
				this->funcN = atoi(function.substr(prefix.size() + 1).c_str());
			} else {
				std::string prefix("ATMOST");
				if (!function.compare(0, prefix.size(), prefix)) {
					this->function = ATMOST;
					this->funcN = atoi(function.substr(prefix.size() + 1).c_str());
				} else {
					std::string prefix("EXACTLY");
					if (!function.compare(0, prefix.size(), prefix)) {
						this->function = EXACTLY;
						this->funcN = atoi(function.substr(prefix.size() + 1).c_str());
					}
				}
			}
		}
	} catch (...) {
		this->openhat->throwSettingException(std::string("Syntax error for LogicPort ") + this->getID() + ", Function setting: Expected OR, AND, XOR, ATLEAST <n>, ATMOST <n> or EXACT <n>");
	}

	if (this->function == UNKNOWN)
		this->openhat->throwSettingException("Unknown function specified for LogicPort: " + function);
	if ((this->function == ATLEAST) && (this->funcN <= 0))
		this->openhat->throwSettingException("Expected positive integer for function ATLEAST of LogicPort: " + function);
	if ((this->function == ATMOST) && (this->funcN <= 0))
		this->openhat->throwSettingException("Expected positive integer for function ATMOST of LogicPort: " + function);
	if ((this->function == EXACTLY) && (this->funcN <= 0))
		this->openhat->throwSettingException("Expected positive integer for function EXACT of LogicPort: " + function);

	this->negate = config->getBool("Negate", false);

	this->inputPortStr = config->getString("InputPorts", "");
	if (this->inputPortStr == "")
		this->openhat->throwSettingException("Expected at least one input port for LogicPort");

	this->outputPortStr = config->getString("OutputPorts", "");
	this->inverseOutputPortStr = config->getString("InverseOutputPorts", "");
}

void LogicPort::setLine(uint8_t /*line*/, ChangeSource /*changeSource*/) {
	throw PortError(this->ID() + ": The line of a LogicPort cannot be set directly");
}

void LogicPort::prepare() {
	this->logDebug("Preparing port");
	opdi::DigitalPort::prepare();

	if (this->function == UNKNOWN)
		throw PortError(std::string("Logic function is unknown; port not configured correctly: ") + this->id);

	// find ports; throws errors if something required is missing
	this->findDigitalPorts(this->getID(), "InputPorts", this->inputPortStr, this->inputPorts);
	this->findDigitalPorts(this->getID(), "OutputPorts", this->outputPortStr, this->outputPorts);
	this->findDigitalPorts(this->getID(), "InverseOutputPorts", this->inverseOutputPortStr, this->inverseOutputPorts);
}

uint8_t LogicPort::doWork(uint8_t canSend)  {
	opdi::DigitalPort::doWork(canSend);

	// count how many input ports are High
	size_t highCount = 0;
	auto it = this->inputPorts.begin();
	auto ite = this->inputPorts.end();
	while (it != ite) {
		uint8_t mode;
		uint8_t line;
		try {
			(*it)->getState(&mode, &line);
		} catch (Poco::Exception &e) {
			this->logNormal(std::string("Error querying port ") + (*it)->getID() + ": " + this->openhat->getExceptionMessage(e));
		}
		highCount += line;
		++it;
	}

	// evaluate function
	uint8_t newLine = (this->negate ? 1 : 0);

	switch (this->function) {
	case UNKNOWN:
		return OPDI_STATUS_OK;
	case OR: if (highCount > 0)
				 newLine = (this->negate ? 0 : 1);
		break;
	case AND: if (highCount == this->inputPorts.size())
				  newLine = (this->negate ? 0 : 1);
		break;
	// XOR is implemented as modulo; meaning an odd number of inputs must be high
	case XOR: if (highCount % 2 == 1)
				newLine = (this->negate ? 0 : 1);
		break;
	case ATLEAST: if (highCount >= this->funcN)
					  newLine = (this->negate ? 0 : 1);
		break;
	case ATMOST: if (highCount <= this->funcN)
					 newLine = (this->negate ? 0 : 1);
		break;
	case EXACTLY: if (highCount == this->funcN)
					 newLine = (this->negate ? 0 : 1);
		break;
	}

	// change detected?
	if (newLine != this->line) {
		this->logDebug("Detected line change (" + this->to_string(highCount) + " of " + this->to_string(this->inputPorts.size()) + " inputs port are High)");

		opdi::DigitalPort::setLine(newLine);

		// regular output ports
		auto it = this->outputPorts.begin();
		auto ite = this->outputPorts.end();
		while (it != ite) {
			try {
				uint8_t mode;
				uint8_t line;
				// get current output port state
				(*it)->getState(&mode, &line);
				// changed?
				if (line != newLine) {
					this->logDebug("Changing line of port " + (*it)->ID() + " to " + (newLine == 0 ? "Low" : "High"));
					(*it)->setLine(newLine);
				}
			} catch (Poco::Exception &e) {
				this->logNormal(std::string("Error changing port ") + (*it)->ID() + ": " + this->openhat->getExceptionMessage(e));
			}
			++it;
		}
		// inverse output ports
		it = this->inverseOutputPorts.begin();
		ite = this->inverseOutputPorts.end();
		while (it != ite) {
			try {
				uint8_t mode;
				uint8_t line;
				// get current output port state
				(*it)->getState(&mode, &line);
				// changed?
				if (line == newLine) {
					this->logDebug("Changing line of inverse port " + (*it)->ID() + " to " + (newLine == 0 ? "High" : "Low"));
					(*it)->setLine((newLine == 0 ? 1 : 0));
				}
			} catch (Poco::Exception &e) {
				this->logNormal("Error changing port " + (*it)->ID() + ": " + this->openhat->getExceptionMessage(e));
			}
			++it;
		}
	}

	return OPDI_STATUS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Pulse Port
///////////////////////////////////////////////////////////////////////////////

PulsePort::PulsePort(AbstractOpenHAT* openhat, const char* id) : opdi::DigitalPort(id, OPDI_PORTDIRCAP_OUTPUT, 0), period(openhat), dutyCycle(openhat) {
	this->opdi = this->openhat = openhat;
	this->negate = false;

	opdi::DigitalPort::setMode(OPDI_DIGITAL_MODE_OUTPUT);
	// set the pulse state to an invalid state
	// this will trigger the detection logic in the first call of doWork
	// so that all dependent output ports will be set to a defined state
	this->pulseState = -1;
	this->lastStateChangeTime = 0;
	this->disabledState = -1;
}

void PulsePort::configure(ConfigurationView* config) {
	this->openhat->configureDigitalPort(config, this, false);
	this->logVerbosity = this->openhat->getConfigLogVerbosity(config, opdi::LogVerbosity::UNKNOWN);

	this->negate = config->getBool("Negate", false);

	std::string disabledState = config->getString("DisabledState", "");
	if (disabledState == "High") {
		this->disabledState = 1;
	} else if (disabledState == "Low") {
		this->disabledState = 0;
	} else if (disabledState != "")
		this->openhat->throwSettingException(this->ID() + ": Unsupported DisabledState; expected 'Low' or 'High': " + disabledState);

	this->enablePortStr = config->getString("EnablePorts", "");
	this->outputPortStr = config->getString("OutputPorts", "");
	this->inverseOutputPortStr = config->getString("InverseOutputPorts", "");

	std::string periodStr = this->openhat->getConfigString(config, this->ID(), "Period", "", true);
	this->period.initialize(this, "Period", periodStr);
	if (!this->period.validate(1, INT_MAX))
		this->openhat->throwSettingException(this->ID() + ": Specify a positive integer value for the Period setting of a PulsePort: " + periodStr);

	// duty cycle is specified in percent
	std::string dutyCycleStr = this->openhat->getConfigString(config, this->ID(), "DutyCycle", "", true);
	this->dutyCycle.initialize(this, "DutyCycle", dutyCycleStr);
	if (!this->dutyCycle.validate(0, 100))
		this->openhat->throwSettingException(this->ID() + ": Specify a percentage value from 0 - 100 for the DutyCycle setting of a PulsePort: " + dutyCycleStr);
}

void PulsePort::prepare() {
	this->logDebug("Preparing port");
	opdi::DigitalPort::prepare();

	// find ports; throws errors if something required is missing
	this->findDigitalPorts(this->getID(), "EnablePorts", this->enablePortStr, this->enablePorts);
	this->findDigitalPorts(this->getID(), "OutputPorts", this->outputPortStr, this->outputPorts);
	this->findDigitalPorts(this->getID(), "InverseOutputPorts", this->inverseOutputPortStr, this->inverseOutputPorts);
}

uint8_t PulsePort::doWork(uint8_t canSend)  {
	opdi::DigitalPort::doWork(canSend);

	// default: pulse is enabled if the line is High
	bool enabled = (this->line == 1);

	if (!enabled && (this->enablePorts.size() > 0)) {
		int highCount = 0;
		auto it = this->enablePorts.begin();
		auto ite = this->enablePorts.end();
		while (it != ite) {
			try {
				highCount += (*it)->getLine() == 1;
			} catch (Poco::Exception &e) {
				this->logExtreme("Error querying port " + (*it)->ID() + ": " + this->openhat->getExceptionMessage(e));
			}
			if (highCount > 0)
				break;
			++it;
		}
		// pulse is enabled if there is at least one EnablePort with High
		enabled |= highCount > 0;
	}

	int32_t period = 0;
	double dutyCycle = 0;
	bool error = false;

	try {
		period = this->period.value();
		if (period < 1) {
			this->logWarning("Period may not be zero or less: " + to_string(period));
			error = true;
		}
	}
	catch (Poco::Exception e) {
		this->logExtreme("Error resolving period value: " + this->openhat->getExceptionMessage(e));
		error = true;
	}

	try {
		dutyCycle = this->dutyCycle.value();
		if (dutyCycle < 0) {
			this->logWarning("DutyCycle may not be negative: " + to_string(dutyCycle));
			error = true;
		}
		if (dutyCycle > 100) {
			this->logWarning("DutyCycle may not exceed 100%: " + to_string(dutyCycle));
			error = true;
		}
	}
	catch (Poco::Exception e) {
		this->logExtreme("Error resolving duty cycle value: " + this->openhat->getExceptionMessage(e));
		error = true;
	}

	// determine new line level
	uint8_t newState = this->pulseState;

	if (enabled && !error) {
		// a duty cycle of 0 means always off
		if (dutyCycle == 0)
			// switch to (logical) Low
			newState = (this->negate ? 1 : 0);
		// a duty cycle of 100 means always off
		else if (dutyCycle == 100)
			// switch to (logical) High
			newState = (this->negate ? 0 : 1);
		else {
			// check whether the time for state change has been reached
			uint64_t timeDiff = opdi_get_time_ms() - this->lastStateChangeTime;
			// current state (logical) Low?
			if (this->pulseState == (this->negate ? 1 : 0)) {
				// time up to High reached?
				if (timeDiff > period * (1.0 - dutyCycle / 100.0))
					// switch to (logical) High
					newState = (this->negate ? 0 : 1);
			}
			else {
				// time up to Low reached?
				if (timeDiff > period * dutyCycle / 100.0)
					// switch to (logical) Low
					newState = (this->negate ? 1 : 0);
			}
		}
	} else {
		// if the port is not enabled, and an inactive state is specified, set it
		if (this->disabledState > -1)
			newState = this->disabledState;
	}

	// evaluate function

	// change detected?
	if (newState != this->pulseState) {
		this->logDebug(std::string("Changing pulse to ") + (newState == 1 ? "High" : "Low") + " (dTime: " + to_string(opdi_get_time_ms() - this->lastStateChangeTime) + " ms)");

		this->lastStateChangeTime = opdi_get_time_ms();

		// set the new state
		this->pulseState = newState;

		// regular output ports
		auto it = this->outputPorts.begin();
		auto ite = this->outputPorts.end();
		while (it != ite) {
			try {
				(*it)->setLine(newState);
			} catch (Poco::Exception &e) {
				this->logNormal("Error setting output port state: " + (*it)->ID() + ": " + this->openhat->getExceptionMessage(e));
			}
			++it;
		}
		// inverse output ports
		it = this->inverseOutputPorts.begin();
		ite = this->inverseOutputPorts.end();
		while (it != ite) {
			try {
				(*it)->setLine((newState == 0 ? 1 : 0));
			} catch (Poco::Exception &e) {
				this->logNormal("Error setting inverse output port state: " + (*it)->ID() + ": " + this->openhat->getExceptionMessage(e));
			}
			++it;
		}
	}

	return OPDI_STATUS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Selector Port
///////////////////////////////////////////////////////////////////////////////

SelectorPort::SelectorPort(AbstractOpenHAT* openhat, const char* id) : opdi::DigitalPort(id, OPDI_PORTDIRCAP_OUTPUT, 0) {
	this->opdi = this->openhat = openhat;

	opdi::DigitalPort::setMode(OPDI_DIGITAL_MODE_OUTPUT);
	// set the line to an invalid state
	this->line = -1;
	this->errorState = -1;		// undefined
}

void SelectorPort::configure(ConfigurationView* config) {
	this->openhat->configureDigitalPort(config, this, false);
	this->logVerbosity = this->openhat->getConfigLogVerbosity(config, opdi::LogVerbosity::UNKNOWN);

	this->selectPortStr = config->getString("SelectPort", "");
	if (this->selectPortStr == "")
		this->openhat->throwSettingException(this->ID() + ": You have to specify the SelectPort");

	this->outputPortStr = config->getString("OutputPorts", "");

	int pos = config->getInt("Position", -1);
	if ((pos < 0) || (pos > 65535))
		this->openhat->throwSettingException(this->ID() + ": You have to specify a SelectPort position that is greater than -1 and lower than 65536");
	this->position = pos;

	std::string errState = config->getString("ErrorState", "");
	if (errState == "High") {
		this->errorState = 1;
	}
	else if (errState == "Low") {
		this->errorState = 0;
	}
	else if (errState != "")
		this->openhat->throwSettingException(this->ID() + ": Unsupported ErrorState; expected 'Low' or 'High': " + errState);
}

void SelectorPort::setLine(uint8_t line, ChangeSource changeSource) {
	opdi::DigitalPort::setLine(line, changeSource);
	if (this->line == 1) {
		this->logDebug("Setting Port '" + this->selectPort->ID() + "' to position " + to_string(this->position));
		// set the specified select port to the specified position
		this->selectPort->setPosition(this->position);
	} else {
		this->logDebug("Warning: Setting Selector port line to Low has no effect");
	}
	// set output ports' lines
	auto it = this->outputPorts.cbegin();
	while (it != this->outputPorts.cend()) {
		(*it)->setLine(line);
		++it;
	}
}

void SelectorPort::prepare() {
	this->logDebug("Preparing port");
	opdi::DigitalPort::prepare();

	// find port; throws errors if something required is missing
	this->selectPort = this->findSelectPort(this->getID(), "SelectPort", this->selectPortStr, true);
	this->findDigitalPorts(this->getID(), "OutputPorts", this->outputPortStr, this->outputPorts);

	// check position range
	if (this->position > this->selectPort->getMaxPosition())
		this->openhat->throwSettingException(this->ID() + ": The specified selector position exceeds the maximum of port " + this->selectPort->ID() + ": " + to_string(this->selectPort->getMaxPosition()));
}

uint8_t SelectorPort::doWork(uint8_t canSend)  {
	opdi::DigitalPort::doWork(canSend);

	// check whether the select port is in the specified position
	uint16_t pos;
	try {
		// get current position (may result in an error)
		this->selectPort->getState(&pos);
	}
	catch (const Poco::Exception& e) {
		this->logExtreme("Error querying Select port '" + this->selectPort->ID() + "': " + this->openhat->getExceptionMessage(e));
		// error state specified?
		if (this->errorState >= 0)
			this->setLine(this->errorState);
		return OPDI_STATUS_OK;
	}

	if (pos == this->position) {
		if (this->line != 1) {
			this->logDebug("Port " + this->selectPort->ID() + " is in position " + to_string(this->position) + ", switching SelectorPort to High");
			opdi::DigitalPort::setLine(1);
			// set output ports' lines
			auto it = this->outputPorts.cbegin();
			while (it != this->outputPorts.cend()) {
				(*it)->setLine(1);
				++it;
			}
		}
	} else {
		if (this->line != 0) {
			this->logDebug("Port " + this->selectPort->ID() + " is in position " + to_string(this->position) + ", switching SelectorPort to Low");
			opdi::DigitalPort::setLine(0);
			// set output ports' lines
			auto it = this->outputPorts.cbegin();
			while (it != this->outputPorts.cend()) {
				(*it)->setLine(0);
				++it;
			}
		}
	}

	return OPDI_STATUS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Error Detector Port
///////////////////////////////////////////////////////////////////////////////

ErrorDetectorPort::ErrorDetectorPort(AbstractOpenHAT* openhat, const char* id) : opdi::DigitalPort(id, OPDI_PORTDIRCAP_INPUT, 0) {
	this->opdi = this->openhat = openhat;

	opdi::DigitalPort::setMode(OPDI_DIGITAL_MODE_INPUT_FLOATING);

	// make sure to trigger state change at first doWork
	this->line = -1;
}

void ErrorDetectorPort::configure(ConfigurationView* config) {
	this->openhat->configureDigitalPort(config, this);	
	this->logVerbosity = this->openhat->getConfigLogVerbosity(config, opdi::LogVerbosity::UNKNOWN);

	this->inputPortStr = this->openhat->getConfigString(config, this->ID(), "InputPorts", "", true);
	this->negate = config->getBool("Negate", false);
}

void ErrorDetectorPort::prepare() {
	this->logDebug("Preparing port");
	opdi::DigitalPort::prepare();

	// find ports; throws errors if something required is missing
	this->findPorts(this->getID(), "InputPorts", this->inputPortStr, this->inputPorts);
}

uint8_t ErrorDetectorPort::doWork(uint8_t canSend)  {
	opdi::DigitalPort::doWork(canSend);

	int8_t newLine = 0;

	// if any port has an error, set the line state to 1
	auto it = this->inputPorts.begin();
	auto ite = this->inputPorts.end();
	while (it != ite) {
		if ((*it)->hasError()) {
			this->logExtreme("Detected error on port: " + (*it)->ID());
			newLine = 1;
			break;
		}
		++it;
	}

	if (negate)
		newLine = (newLine == 0 ? 1 : 0);

	opdi::DigitalPort::setLine(newLine);

	return OPDI_STATUS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Serial Streaming Port
///////////////////////////////////////////////////////////////////////////////

SerialStreamingPort::SerialStreamingPort(AbstractOpenHAT* openhat, const char* id) : opdi::StreamingPort(id) {
	this->opdi = this->openhat = openhat;
	this->mode = PASS_THROUGH;
	this->device = nullptr;
	this->serialPort = new ctb::SerialPort();
}

SerialStreamingPort::~SerialStreamingPort() {
	if (device != nullptr)
		this->device->Close();
}

uint8_t SerialStreamingPort::doWork(uint8_t canSend)  {
	opdi::StreamingPort::doWork(canSend);

	if (this->device == nullptr)
		return OPDI_STATUS_OK;

	if (this->mode == LOOPBACK) {
		// byte available?
		if (this->available(0) > 0) {
			char result;
			if (this->read(&result) > 0) {
				this->logDebug("Looping back received serial data byte: " + this->openhat->to_string((int)result));

				// echo
				this->write(&result, 1);
			}
		}
	}

	return OPDI_STATUS_OK;
}

void SerialStreamingPort::configure(ConfigurationView* config) {
	this->openhat->configureStreamingPort(config, this);
	this->logVerbosity = this->openhat->getConfigLogVerbosity(config, opdi::LogVerbosity::UNKNOWN);

	std::string serialPortName = this->openhat->getConfigString(config, this->ID(), "SerialPort", "", true);
	int baudRate = config->getInt("BaudRate", 9600);
	std::string protocol = config->getString("Protocol", "8N1");
	// int timeout = config->getInt("Timeout", 100);

	this->logVerbose("Opening serial port " + serialPortName + " with " + this->openhat->to_string(baudRate) + " baud and protocol " + protocol);

	// try to lock the port name as a resource
	this->openhat->lockResource(serialPortName, this->getID());

	if (this->serialPort->Open(serialPortName.c_str(), baudRate, 
							protocol.c_str(), 
							ctb::SerialPort::NoFlowControl) >= 0) {
		this->device = this->serialPort;
	} else {
		this->openhat->throwSettingException(this->ID() + ": Unable to open serial port: " + serialPortName);
	}

	this->logVerbose("Serial port " + serialPortName + " opened successfully");

	std::string modeStr = config->getString("Mode", "");
	if (modeStr == "Loopback") {
		this->mode = LOOPBACK;
	} else
	if (modeStr == "Passthrough") {
		this->mode = PASS_THROUGH;
	} else
		if (modeStr != "")
			this->openhat->throwSettingException(this->ID() + ": Invalid mode specifier; expected 'Passthrough' or 'Loopback': " + modeStr);
}

int SerialStreamingPort::write(char* bytes, size_t length) {
	return this->device->Write(bytes, length);
}

int SerialStreamingPort::available(size_t /*count*/) {
	// count has no meaning in this implementation

	char buf;
	int code = this->device->Read(&buf, 1);
	// error?
	if (code < 0)
		return code;
	// byte read?
	if (code > 0) {
		this->device->PutBack(buf);
		return 1;
	}
	// nothing available
	return 0;
}

int SerialStreamingPort::read(char* result) {
	char buf;
	int code = this->device->Read(&buf, 1);
	// error?
	if (code < 0)
		return code;
	// byte read?
	if (code > 0) {
		*result = buf;
		return 1;
	}
	// nothing available
	return 0;
}

bool SerialStreamingPort::hasError(void) const {
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// Logger Streaming Port
///////////////////////////////////////////////////////////////////////////////

LoggerPort::LoggerPort(AbstractOpenHAT* openhat, const char* id) : opdi::StreamingPort(id) {
	this->opdi = this->openhat = openhat;
	this->logPeriod = 10000;		// default: 10 seconds
	this->writeHeader = true;
	this->lastEntryTime = opdi_get_time_ms();		// wait until writing first record
	this->format = CSV;
	this->separator = ";";
}

LoggerPort::~LoggerPort() {
	if (this->outFile.is_open())
		this->outFile.close();
}

std::string LoggerPort::getPortStateStr(opdi::Port* port) {
	return this->openhat->getPortStateStr(port);
}

void LoggerPort::prepare() {
	this->logDebug("Preparing port");
	opdi::StreamingPort::prepare();

	// find ports; throws errors if something required is missing
	this->findPorts(this->getID(), "Ports", this->portsToLogStr, this->portsToLog);
}

uint8_t LoggerPort::doWork(uint8_t canSend)  {
	opdi::StreamingPort::doWork(canSend);

	// check whether the time for a new entry has been reached
	uint64_t timeDiff = opdi_get_time_ms() - this->lastEntryTime;
	if (timeDiff < this->logPeriod)
		return OPDI_STATUS_OK;

	this->lastEntryTime = opdi_get_time_ms();

	// build log entry
	std::string entry;

	if (format == CSV) {
		if (this->writeHeader) {
			entry = "Timestamp" + this->separator;
			// go through port list, build header
			auto it = this->portsToLog.begin();
			auto ite = this->portsToLog.end();
			while (it != ite) {
				entry += (*it)->getID();
				// separator necessary?
				if (it != ite - 1) 
					entry += this->separator;
				++it;
			}
			this->outFile << entry << std::endl;
			this->writeHeader = false;
		}
		entry = this->openhat->getTimestampStr() + this->separator;
		// go through port list
		auto it = this->portsToLog.begin();
		auto ite = this->portsToLog.end();
		while (it != ite) {
			entry += this->getPortStateStr(*it);
			// separator necessary?
			if (it != ite - 1) 
				entry += this->separator;
			++it;
		}
	}

	if (!this->outFile.is_open())
		return OPDI_STATUS_OK;

	// write to output
	this->outFile << entry << std::endl;

	return OPDI_STATUS_OK;
}

void LoggerPort::configure(ConfigurationView* config) {
	this->openhat->configureStreamingPort(config, this);
	this->logVerbosity = this->openhat->getConfigLogVerbosity(config, opdi::LogVerbosity::UNKNOWN);

	this->logPeriod = config->getInt("Period", this->logPeriod);
	this->separator = config->getString("Separator", this->separator);

	std::string formatStr = config->getString("Format", "CSV");
	if (formatStr != "CSV")
		this->openhat->throwSettingException(this->ID() + ": Formats other than CSV are currently not supported");

	std::string outFileStr = config->getString("OutputFile", "");
	if (outFileStr != "") {
		// try to lock the output file name as a resource
		this->openhat->lockResource(outFileStr, this->getID());

		this->logVerbose("Opening output log file " + outFileStr);

		// open the stream in append mode
		this->outFile.open(outFileStr, std::ios_base::app);
	} else
		this->openhat->throwSettingException(this->ID() + ": The OutputFile setting must be specified");

	this->portsToLogStr = this->openhat->getConfigString(config, this->ID(), "Ports", "", true);
}

int LoggerPort::write(char* /*bytes*/, size_t /*length*/) {
	return 0;
}

int LoggerPort::available(size_t /*count*/) {
	// count has no meaning in this implementation
	// nothing available
	return 0;
}

int LoggerPort::read(char* /*result*/) {
	// nothing available
	return 0;
}

bool LoggerPort::hasError(void) const {
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// Fader Port
///////////////////////////////////////////////////////////////////////////////

FaderPort::FaderPort(AbstractOpenHAT* openhat, const char* id) : opdi::DigitalPort(id, OPDI_PORTDIRCAP_OUTPUT, 0), leftValue(openhat), rightValue(openhat), durationMsValue(openhat) {
	this->opdi = this->openhat = openhat;
	this->mode = LINEAR;
	this->lastValue = -1;
	this->invert = false;
	this->switchOffAction = NONE;
	this->actionToPerform = NONE;

	opdi::DigitalPort::setMode(OPDI_DIGITAL_MODE_OUTPUT);
}

void FaderPort::configure(ConfigurationView* config) {
	this->logVerbosity = this->openhat->getConfigLogVerbosity(config, opdi::LogVerbosity::UNKNOWN);

	std::string modeStr = openhat->getConfigString(config, this->ID(), "FadeMode", "", true);
	if (modeStr == "Linear")
		this->mode = LINEAR;
	else if (modeStr == "Exponential")
		this->mode = EXPONENTIAL;
	else if (modeStr != "")
		this->openhat->throwSettingException(this->ID() + ": Invalid FadeMode setting specified; expected 'Linear' or 'Exponential': " + modeStr);

	this->leftValue.initialize(this, "Left", config->getString("Left", "-1"));
	if (!this->leftValue.validate(0.0, 100.0))
		this->openhat->throwSettingException(this->ID() + ": Value for 'Left' must be between 0 and 100 percent");
	this->rightValue.initialize(this, "Right", config->getString("Right", "-1"));
	if (!this->rightValue.validate(0.0, 100.0))
		this->openhat->throwSettingException(this->ID() + ": Value for 'Right' must be between 0 and 100 percent");
	this->durationMsValue.initialize(this, "Duration", config->getString("Duration", "-1"));
	if (!this->durationMsValue.validate(0, INT_MAX))
		this->openhat->throwSettingException(this->ID() + ": 'Duration' must be a positive non-zero value (in milliseconds)");

	if (this->mode == EXPONENTIAL) {
		this->expA = config->getDouble("ExpA", 1);
		if (this->expA < 0.0)
			this->openhat->throwSettingException(this->ID() + ": Value for 'ExpA' must be greater than 0 (default: 1)");
		this->expB = config->getDouble("ExpB", 10);
		if (this->expB < 0.0)
			this->openhat->throwSettingException(this->ID() + ": Value for 'ExpB' must be greater than 0 (default: 10)");

		// determine maximum result of the exponentiation, depending on the coefficients expA and expB
		this->expMax = this->expA * (exp(this->expB) - 1);
	}
	this->invert = config->getBool("Invert", this->invert);
	std::string switchOffActionStr = config->getString("SwitchOffAction", "None");
	if (switchOffActionStr == "SetToLeft") {
		this->switchOffAction = SET_TO_LEFT;
	} else
	if (switchOffActionStr == "SetToRight") {
		this->switchOffAction = SET_TO_RIGHT;
	} else
	if (switchOffActionStr == "None") {
		this->switchOffAction = NONE;
	} else
		this->openhat->throwSettingException(this->ID() + ": Illegal value for 'SwitchOffAction', expected 'SetToLeft', 'SetToRight', or 'None': " + switchOffActionStr);

	this->outputPortStr = openhat->getConfigString(config, this->ID(), "OutputPorts", "", true);
	this->endSwitchesStr = openhat->getConfigString(config, this->ID(), "EndSwitches", "", false);

	this->openhat->configurePort(config, this, 0);
}

void FaderPort::setLine(uint8_t line, ChangeSource changeSource) {
	uint8_t oldline = this->line;
	if ((oldline == 0) && (line == 1)) {
		// store current values on start (might be resolved by ValueResolvers, and we don't want them to change during fading
		// because each value might again refer to the output port - not an uncommon scenario for e.g. dimmers)
		this->left = this->leftValue.value();
		this->right = this->rightValue.value();
		this->durationMs = this->durationMsValue.value();

		// don't fade if the duration is impracticably low
		if (this->durationMs < 5) {
			this->logDebug("Refusing to fade because duration is impracticably low: " + to_string(this->durationMs));
		} else {
			opdi::DigitalPort::setLine(line, changeSource);
			this->startTime = Poco::Timestamp();
			// cause correct log output
			this->lastValue = -1;
			this->logDebug("Start fading at " + to_string(this->left) + "% with a duration of " + to_string(this->durationMs) + " ms");
		}
	} else {
		opdi::DigitalPort::setLine(line, changeSource);
		// switched off?
		if (oldline != this->line) {
			this->logDebug("Stopped fading at " + to_string(this->lastValue * 100.0) + "%");
			this->actionToPerform = this->switchOffAction;
			// resolve values again for the switch off action
			this->left = this->leftValue.value();
			this->right = this->rightValue.value();
		}
	}
}

void FaderPort::prepare() {
	this->logDebug("Preparing port");
	opdi::DigitalPort::prepare();

	// find ports; throws errors if something required is missing
	this->findPorts(this->ID(), "OutputPorts", this->outputPortStr, this->outputPorts);
	this->findDigitalPorts(this->ID(), "EndSwitches", this->endSwitchesStr, this->endSwitches);
}

uint8_t FaderPort::doWork(uint8_t canSend)  {
	opdi::DigitalPort::doWork(canSend);

	// active, or a switch off action needs to be performed?
	if ((this->line == 1) || (this->actionToPerform != NONE)) {
		
		Poco::Timestamp::TimeVal elapsedMs(0);

		if (this->line == 1) {
			if (this->durationMs < 0) {
				this->logWarning("Duration may not be negative; disabling fader: " + to_string(this->durationMs));
				// disable the fader immediately
				opdi::DigitalPort::setLine(0);
				this->refreshRequired = true;
				return OPDI_STATUS_OK;
			}

			// calculate time difference
			Poco::Timestamp now;
			elapsedMs = (now.epochMicroseconds() - this->startTime.epochMicroseconds()) / 1000;

			// end reached?
			if (elapsedMs > this->durationMs) {
				this->setLine(0);
				this->refreshRequired = true;

				// set end switches if specified
				auto it = this->endSwitches.begin();
				auto ite = this->endSwitches.end();
				while (it != ite) {
					try {
						this->logDebug("Setting line of end switch port " + (*it)->ID() + " to High");
						(*it)->setLine(1);
					} catch (Poco::Exception &e) {
						this->logWarning("Error changing port " + (*it)->ID() + ": " + this->openhat->getExceptionMessage(e));
					}
					++it;
				}

				return OPDI_STATUS_OK;
			}
		}

		// calculate current value (linear first) within the range [0, 1]
		double value = 0.0;

		// if the port has been switched off, and a switch action needs to be performed,
		// do it here
		if (this->actionToPerform != NONE) {
			if (this->actionToPerform == SET_TO_LEFT)
				value = this->left / 100.0;
			else
			if (this->actionToPerform == SET_TO_RIGHT)
				value = this->right / 100.0;
			// action has been handled
			this->actionToPerform = NONE;
			this->logDebug("Switch off action handled; setting value to: " + this->to_string(value) + "%");
		} else {
			// regular fader operation
			if (this->mode == LINEAR) {
				if (this->invert)
					value = (this->right - (double)elapsedMs / (double)this->durationMs * (this->right - this->left)) / 100.0;
				else
					value = (this->left + (double)elapsedMs / (double)this->durationMs * (this->right - this->left)) / 100.0;
			} else
			if (this->mode == EXPONENTIAL) {
				// calculate exponential value; start with value relative to the range
				if (this->invert)
					value = 1.0 - (double)elapsedMs / (double)this->durationMs;
				else
					value = (double)elapsedMs / (double)this->durationMs;

				// exponentiate value; map within [0..1]
				value = (this->expMax <= 0 ? 1 : (this->expA * (exp(this->expB * value)) - 1) / this->expMax);

				if (value < 0.0)
					value = 0.0;

				// map back to the target range
				value = (this->left + value * (this->right - this->left)) / 100.0;
			} else
				value = 0.0;
		}

		this->logExtreme("Setting current fader value to " + to_string(value * 100.0) + "%");

		// regular output ports
		auto it = this->outputPorts.begin();
		auto ite = this->outputPorts.end();
		while (it != ite) {
			try {
				if (0 == strcmp((*it)->getType(), OPDI_PORTTYPE_ANALOG)) {
					((opdi::AnalogPort*)(*it))->setRelativeValue(value);
				} else
				if (0 == strcmp((*it)->getType(), OPDI_PORTTYPE_DIAL)) {
					opdi::DialPort* port = (opdi::DialPort*)(*it);
					double pos = port->getMin() + (port->getMax() - port->getMin()) * value;
					port->setPosition((int64_t)pos);
				} else
					throw Poco::Exception("The port " + (*it)->ID() + " is neither an AnalogPort nor a DialPort");
			} catch (Poco::Exception& e) {
				this->logNormal("Error changing port " + (*it)->ID() + ": " + this->openhat->getExceptionMessage(e));
			}
			++it;
		}

		this->lastValue = value;
	}

	return OPDI_STATUS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// SceneSelectPort
///////////////////////////////////////////////////////////////////////////////

SceneSelectPort::SceneSelectPort(AbstractOpenHAT* openhat, const char* id) : opdi::SelectPort(id) {
	this->opdi = this->openhat = openhat;
	this->positionSet = false;
}

void SceneSelectPort::configure(ConfigurationView* config, ConfigurationView* parentConfig) {
	this->logVerbosity = this->openhat->getConfigLogVerbosity(config, opdi::LogVerbosity::UNKNOWN);

	// remember configuration file path (scene files are always relative to the configuration file)
	this->configFilePath = config->getString(OPENHAT_CONFIG_FILE_SETTING);

	// the scene select port requires a prefix or section "<portID>.Scenes"
	Poco::AutoPtr<ConfigurationView> portItems = this->openhat->createConfigView(config, "Scenes");
	config->addUsedKey("Scenes");

	// get ordered list of items
	ConfigurationView::Keys itemKeys;
	portItems->keys("", itemKeys);

	typedef Poco::Tuple<int, std::string> Item;
	typedef std::vector<Item> ItemList;
	ItemList orderedItems;

	// create ordered list of items (by priority)
	for (auto it = itemKeys.begin(), ite = itemKeys.end(); it != ite; ++it) {
		int itemNumber;
		if (!Poco::NumberParser::tryParse(*it, itemNumber) || (itemNumber < 0)) {
			this->openhat->throwSettingException(this->ID() + ": Scene identifiers must be numeric integer values greater or equal than 0; got: " + this->to_string(itemNumber));
		}
		// insert at the correct position to create a sorted list of items
		auto nli = orderedItems.begin();
		auto nlie = orderedItems.end();
		while (nli != nlie) {
			if (nli->get<0>() > itemNumber)
				break;
			++nli;
		}
		Item item(itemNumber, portItems->getString(*it));
		orderedItems.insert(nli, item);
	}

	if (orderedItems.size() == 0)
		this->openhat->throwSettingException(this->ID() + ": A scene select port requires at least one scene in its config section", this->ID() + ".Scenes");

	// go through items, create ordered list of char* items
	auto nli = orderedItems.begin();
	auto nlie = orderedItems.end();
	while (nli != nlie) {
		this->fileList.push_back(nli->get<1>());
		++nli;
	}

	this->openhat->configureSelectPort(config, parentConfig, this, 0);

	// check whether port items and file match in numbers
	if ((uint32_t)(this->getMaxPosition() + 1) != this->fileList.size()) 
		this->openhat->throwSettingException(this->ID() + ": The number of scenes (" + this->to_string(this->fileList.size()) + ")"
			+ " must match the number of items (" + this->to_string(this->getMaxPosition() + 1) + ")");
}

void SceneSelectPort::prepare() {
	this->logDebug("Preparing port");
	opdi::SelectPort::prepare();

	// check files
	auto fi = this->fileList.begin();
	auto fie = this->fileList.begin();
	while (fi != fie) {
		std::string sceneFile = *fi;

		this->logDebug("Checking scene file "+ sceneFile + " relative to configuration file: " + this->configFilePath);

		Poco::Path filePath(this->configFilePath);
		Poco::Path absPath(filePath.absolute());
		Poco::Path parentPath = absPath.parent();
		// append or replace scene file path to path of config file
		Poco::Path finalPath = parentPath.resolve(sceneFile);
		sceneFile = finalPath.toString();
		Poco::File file(finalPath);

		if (!file.exists())
			throw Poco::ApplicationException(this->ID() + ": The scene file does not exist: " + sceneFile);

		// store absolute scene file path
		*fi = sceneFile;

		++fi;
	}
}

uint8_t SceneSelectPort::doWork(uint8_t canSend)  {
	opdi::SelectPort::doWork(canSend);

	// position changed?
	if (this->positionSet) {
		this->logVerbose(std::string("Scene selected: ") + this->getPositionLabel(this->position));

		std::string sceneFile = this->fileList[this->position];

		// prepare scene file parameters (environment, ports, ...)
		std::map<std::string, std::string> parameters;
		this->openhat->getEnvironment(parameters);

		if (this->logVerbosity >= opdi::LogVerbosity::DEBUG) {
			this->logDebug("Scene file parameters:");
			auto it = parameters.begin();
			auto ite = parameters.end();
			while (it != ite) {
				this->logDebug("  " + (*it).first + " = " + (*it).second);
				++it;
			}
		}

		// open the config file
		ConfigurationView config(this->openhat, new OpenHATConfigurationFile(sceneFile, parameters), "", "", false);

		// go through sections of the scene file
		ConfigurationView::Keys sectionKeys;
		config.keys("", sectionKeys);

		if (sectionKeys.size() == 0)
			this->logWarning("Scene file " + sceneFile + " does not contain any scene information, is this intended?");
		else
			this->logDebug("Applying settings from scene file: " + sceneFile);

		for (auto it = sectionKeys.begin(), ite = sectionKeys.end(); it != ite; ++it) {
			// find port corresponding to this section
			opdi::Port* port = this->openhat->findPortByID((*it).c_str());
			if (port == nullptr)
				this->logWarning("In scene file " + sceneFile + ": Port with ID " + (*it) + " not present in current configuration");
			else {
				this->logDebug("Applying settings to port: " + *it);

				// configure port according to settings 
				Poco::AutoPtr<ConfigurationView> portConfig = this->openhat->createConfigView(&config, *it);

				// configure only state - not the general setup
				try {
					if (port->getType()[0] == OPDI_PORTTYPE_DIGITAL[0]) {
						this->openhat->configureDigitalPort(portConfig, (opdi::DigitalPort*)port, true);
					} else
					if (port->getType()[0] == OPDI_PORTTYPE_ANALOG[0]) {
						this->openhat->configureAnalogPort(portConfig, (opdi::AnalogPort*)port, true);
					} else
					if (port->getType()[0] == OPDI_PORTTYPE_SELECT[0]) {
						this->openhat->configureSelectPort(portConfig, &config, (opdi::SelectPort*)port, true);
					} else
					if (port->getType()[0] == OPDI_PORTTYPE_DIAL[0]) {
						this->openhat->configureDialPort(portConfig, (opdi::DialPort*)port, true);
					} else
						this->logWarning("In scene file " + sceneFile + ": Port with ID " + (*it) + " has an unknown type");
				} catch (Poco::Exception &e) {
					this->logWarning("In scene file " + sceneFile + ": Error configuring port " + (*it) + ": " + this->openhat->getExceptionMessage(e));
				}
			}
		}

		// refresh all ports of a connected master
		this->openhat->refresh(nullptr);

		this->positionSet = false;
	}

	return OPDI_STATUS_OK;
}

void SceneSelectPort::setPosition(uint16_t position, ChangeSource changeSource) {
	opdi::SelectPort::setPosition(position, changeSource);

	this->positionSet = true;
}

///////////////////////////////////////////////////////////////////////////////
// FilePort
///////////////////////////////////////////////////////////////////////////////

uint8_t FilePort::doWork(uint8_t canSend) {
	uint8_t result = opdi::DigitalPort::doWork(canSend);
	if (result != OPDI_STATUS_OK)
		return result;

	Poco::Mutex::ScopedLock(this->mutex);

	// expiry time over?
	if ((this->expiryMs > 0) && (this->lastReloadTime > 0) && (opdi_get_time_ms() - lastReloadTime > (uint64_t)this->expiryMs)) {
		// only if the port's value is ok
		if (this->valuePort->getError() == Error::VALUE_OK) {
			this->logWarning(ID() + ": Value of port '" + this->valuePort->ID() + "' has expired");
			this->valuePort->setError(Error::VALUE_EXPIRED);
		}
	}

	if (this->line == 0) {
		// always clear flag if not active (to avoid reloading when set to High)
		this->needsReload = false;
		return OPDI_STATUS_OK;
	}

	// if a delay is specified, ignore reloads until it's up
	if ((this->reloadDelayMs > 0) && (this->lastReloadTime > 0) && (opdi_get_time_ms() - lastReloadTime < (uint64_t)this->reloadDelayMs))
		return OPDI_STATUS_OK;

	if (this->needsReload) {
		this->logDebug("Reloading file: " + this->filePath);

		this->lastReloadTime = opdi_get_time_ms();

		this->needsReload = false;

		// read file and parse content
		try {
			std::string content;
			Poco::FileInputStream fis(this->filePath);
			fis >> content;
			fis.close();

			content = Poco::trim(content);
			if (content == "")
				// This case may happen frequently when files are copied.
				// Apparently, on Linux, cp clears the file first or perhaps
				// creates an empty file before filling it with content.
				// To avoid generating too many log warnings, this case
				// is being silently ignored.
				// When the file is being modified, the DirectoryWatcher will
				// hopefully catch this change so data is not lost.
				// So, instead of:
				// throw Poco::DataFormatException("File is empty");
				// do:
				return OPDI_STATUS_OK;

			switch (this->portType) {
			case DIGITAL_PORT: {
				uint8_t line;
				if (content == "0")
					line = 0;
				else
				if (content == "1")
					line = 1;
				else {
					std::string errorContent = (content.length() > 50 ? content.substr(0, 50) + "..." : content);
					throw Poco::DataFormatException("Expected '0' or '1' but got: " + errorContent);
				}
				this->logDebug("Setting line of digital port '" + this->valuePort->ID() + "' to " + this->to_string((int)line));
				((opdi::DigitalPort*)this->valuePort)->setLine(line);
				break;
			}
			case ANALOG_PORT: {
				double value;
				if (!Poco::NumberParser::tryParseFloat(content, value) || (value < 0) || (value > 1)) {
					std::string errorContent = (content.length() > 50 ? content.substr(0, 50) + "..." : content);
					throw Poco::DataFormatException("Expected decimal value between 0 and 1 but got: " + errorContent);
				}
				value = value * this->numerator / this->denominator;
				this->logDebug("Setting value of analog port '" + this->valuePort->ID() + "' to " + this->to_string(value));
				((opdi::AnalogPort*)this->valuePort)->setRelativeValue(value);
				break;
			}
			case DIAL_PORT: {
				int64_t value;
				int64_t min = ((opdi::DialPort*)this->valuePort)->getMin();
				int64_t max = ((opdi::DialPort*)this->valuePort)->getMax();
				if (!Poco::NumberParser::tryParse64(content, value)) {
					std::string errorContent = (content.length() > 50 ? content.substr(0, 50) + "..." : content);
					throw Poco::DataFormatException("Expected integer value but got: " + errorContent);
				}
				value = value * this->numerator / this->denominator;
				if ((value < min) || (value > max)) {
					std::string errorContent = (content.length() > 50 ? content.substr(0, 50) + "..." : content);
					throw Poco::DataFormatException("Expected integer value between " + this->to_string(min) + " and " + this->to_string(max) + " but got: " + errorContent);
				}
				this->logDebug("Setting position of dial port '" + this->valuePort->ID() + "' to " + this->to_string(value));
				((opdi::DialPort*)this->valuePort)->setPosition(value);
				break;
			}
			case SELECT_PORT: {
				int32_t value;
				uint16_t min = 0;
				uint16_t max = ((opdi::SelectPort*)this->valuePort)->getMaxPosition();
				if (!Poco::NumberParser::tryParse(content, value) || (value < min) || (value > max)) {
					std::string errorContent = (content.length() > 50 ? content.substr(0, 50) + "..." : content);
					throw Poco::DataFormatException("Expected integer value between " + this->to_string(min) + " and " + this->to_string(max) + " but got: " + errorContent);
				}
				this->logDebug("Setting position of select port '" + this->valuePort->ID() + "' to " + this->to_string(value));
				((opdi::SelectPort*)this->valuePort)->setPosition(value);
				break;
			}
			default:
				throw Poco::ApplicationException("Port type is unknown or not supported");
			}
		} catch (Poco::Exception &e) {
			this->logWarning("Error setting port state from file '" + this->filePath + "': " + this->openhat->getExceptionMessage(e));
		}
		if (this->deleteAfterRead) {
			Poco::File file(this->filePath);
			try {
				this->logDebug("Trying to delete file: " + this->filePath);
				file.remove();
			}
			catch (Poco::Exception &e) {
				this->logWarning("Unable to delete file '" + this->filePath + "': " + this->openhat->getExceptionMessage(e));
			}
		}
	}

	return OPDI_STATUS_OK;
}

void FilePort::fileChangedEvent(const void*, const Poco::DirectoryWatcher::DirectoryEvent& evt) {
	if (evt.item.path() == this->filePath) {
		this->logDebug("Detected file modification: " + this->filePath);
		
		Poco::Mutex::ScopedLock(this->mutex);
		this->needsReload = true;
	}
}

void FilePort::writeContent() {
	if (this->line == 0)
		return;

	// create file content
	std::string content;
	try {
		switch (this->portType) {
		case DIGITAL_PORT: {
			uint8_t line;
			uint8_t mode;
	//			this->logDebug("Setting line of digital port '" + this->valuePort->ID() + "' to " + this->to_string((int)line));
			((opdi::DigitalPort*)this->valuePort)->getState(&mode, &line);
			content += (line == 1 ? "1" : "0");
			break;
		}
		case ANALOG_PORT: {
			double value = ((opdi::AnalogPort*)this->valuePort)->getRelativeValue();
			value = value / this->numerator * this->denominator;
			//this->logDebug("Setting value of analog port '" + this->valuePort->ID() + "' to " + this->to_string(value));
			content += this->to_string(value);
			break;
		}
		case DIAL_PORT: {
			int64_t value;
			((opdi::DialPort*)this->valuePort)->getState(&value);
			value = value / this->numerator * this->denominator;
			// this->logDebug("Setting position of dial port '" + this->valuePort->ID() + "' to " + this->to_string(value));
			content += this->to_string(value);
			break;
		}
		case SELECT_PORT: {
			uint16_t value;
			((opdi::SelectPort*)this->valuePort)->getState(&value);
			// this->logDebug("Setting position of select port '" + this->valuePort->ID() + "' to " + this->to_string(value));
			content += this->to_string(value);
			break;
		}
		default:
			throw Poco::ApplicationException("Port type is unknown or not supported");
		}
	}
	catch (Poco::Exception &e) {
		this->logWarning("Error getting port state to write to file '" + this->filePath + "': " + this->openhat->getExceptionMessage(e));
	}

	// write content to file
	Poco::FileOutputStream fos(this->filePath);
	fos << content;
	fos.close();
}

FilePort::FilePort(AbstractOpenHAT* openhat, const char* id) : 
	// a File port is presented as an output (being High means that file IO is active)
	opdi::DigitalPort(id, OPDI_PORTDIRCAP_OUTPUT, 0) {
	this->opdi = this->openhat = openhat;
	this->directoryWatcher = nullptr;
	this->reloadDelayMs = 0;
	this->expiryMs = 0;
	this->deleteAfterRead = false;
	this->lastReloadTime = 0;
	this->needsReload = false;
	this->numerator = 1;
	this->denominator = 1;
	this->valuePort = nullptr;
	this->portType = UNKNOWN;

	// output by default
	this->setMode(OPDI_DIGITAL_MODE_OUTPUT);
	// file IO is active by default
	this->setLine(1);
}

FilePort::~FilePort() {
	if (this->directoryWatcher != nullptr)
		delete this->directoryWatcher;
}

void FilePort::configure(ConfigurationView* config, ConfigurationView* parentConfig) {
	this->openhat->configureDigitalPort(config, this);

	this->filePath = this->openhat->resolveRelativePath(config, this->ID(), openhat->getConfigString(config, this->ID(), "File", "", true), "Config");

	// read port node, create configuration view and setup the port according to the specified type
	std::string portNode = openhat->getConfigString(config, this->ID(), "PortNode", "", true);
	Poco::AutoPtr<ConfigurationView> nodeConfig = this->openhat->createConfigView(parentConfig, portNode);
	std::string portType = openhat->getConfigString(nodeConfig, portNode, "Type", "", true);
	if (portType == "DigitalPort") {
		this->portType = DIGITAL_PORT;
		this->valuePort = new opdi::DigitalPort(portNode.c_str(), OPDI_PORTDIRCAP_INPUT, 0);
		this->openhat->configureDigitalPort(nodeConfig, (opdi::DigitalPort*)valuePort);
		// validate setup
		if (((opdi::DigitalPort*)valuePort)->getMode() != OPDI_DIGITAL_MODE_INPUT_FLOATING)
			this->openhat->throwSettingException(this->ID() + ": Modes other than 'Input' are not supported for a digital file port: " + portNode);
	} else
	if (portType == "AnalogPort") {
		this->portType = ANALOG_PORT;
		this->valuePort = new opdi::AnalogPort(portNode.c_str(), OPDI_PORTDIRCAP_INPUT, 0);
		this->openhat->configureAnalogPort(nodeConfig, (opdi::AnalogPort*)valuePort);
		// validate setup
		if (((opdi::AnalogPort*)valuePort)->getMode() != OPDI_ANALOG_MODE_INPUT)
			this->openhat->throwSettingException(this->ID() + ": Modes other than 'Input' are not supported for a analog file port: " + portNode);
	} else
	if (portType == "DialPort") {
		this->portType = DIAL_PORT;
		this->valuePort = new opdi::DialPort(portNode.c_str());
		this->openhat->configureDialPort(nodeConfig, (opdi::DialPort*)valuePort);
	} else
	if (portType == "SelectPort") {
		this->portType = SELECT_PORT;
		this->valuePort = new opdi::SelectPort(portNode.c_str());
		this->openhat->configureSelectPort(nodeConfig, parentConfig, (opdi::SelectPort*)valuePort);
	} else
	if (portType == "StreamingPort") {
		this->portType = STREAMING_PORT;
		throw Poco::NotImplementedException("Streaming port support not yet implemented");
	} else
		this->openhat->throwSettingException(this->ID() + ": Node " + portNode + ": Type unsupported, expected 'DigitalPort', 'AnalogPort', 'DialPort', 'SelectPort', or 'StreamingPort': " + portType);

	this->openhat->addPort(this->valuePort);

	// if the port is not read-only, we react to state changes
	if (!this->valuePort->isReadonly()) {
		// create a dummy DigitalPort that handles the value port's state changes
		ChangeHandlerPort* chp = new ChangeHandlerPort((this->ID() + "_ChangeHandlerPort").c_str(), this);
		// ports of this type are always hidden
		chp->setHidden(true);
		this->openhat->addPort(chp);

		// user changes to the value port are handled by the change handler
		// the name will be resolved to the actual port in prepare()
		this->valuePort->onChangeUserPortsStr += " " + chp->ID();
	}

	this->reloadDelayMs = config->getInt("ReloadDelay", this->reloadDelayMs);
	if (this->reloadDelayMs < 0) {
		this->openhat->throwSettingException(this->ID() + ": If ReloadDelay is specified it must be greater than 0 (ms): " + this->to_string(this->reloadDelayMs));
	}

	this->expiryMs = config->getInt("Expiry", this->expiryMs);
	if (this->expiryMs < 0) {
		this->openhat->throwSettingException(this->ID() + ": If Expiry is specified it must be greater than 0 (ms): " + this->to_string(this->expiryMs));
	}
	this->deleteAfterRead = config->getBool("DeleteAfterRead", this->deleteAfterRead);

	this->numerator = nodeConfig->getInt("Numerator", this->numerator);
	this->denominator = nodeConfig->getInt("Denominator", this->denominator);
	if (this->denominator == 0) 
		throw Poco::InvalidArgumentException(this->ID() + ": The Denominator may not be 0");

	// determine directory and filename
	Poco::Path path(filePath);
	Poco::Path absPath(path.absolute());
	// std::cout << absPath << std::endl;
	
	this->filePath = absPath.toString();
	this->directory = absPath.parent();

	this->logDebug("Preparing DirectoryWatcher for folder '" + this->directory.path() + "'");

	this->directoryWatcher = new Poco::DirectoryWatcher(this->directory, 
			Poco::DirectoryWatcher::DW_ITEM_MODIFIED | Poco::DirectoryWatcher::DW_ITEM_ADDED | Poco::DirectoryWatcher::DW_ITEM_MOVED_TO, 
			Poco::DirectoryWatcher::DW_DEFAULT_SCAN_INTERVAL);
	this->directoryWatcher->itemModified += Poco::delegate(this, &FilePort::fileChangedEvent);
	this->directoryWatcher->itemAdded += Poco::delegate(this, &FilePort::fileChangedEvent);
	this->directoryWatcher->itemMovedTo += Poco::delegate(this, &FilePort::fileChangedEvent);

	// can the file be loaded initially?
	Poco::File file(this->filePath);
	if (!file.exists())
		this->valuePort->setError(Error::VALUE_NOT_AVAILABLE);
	else {
		// expiry time specified?
		if (this->expiryMs > 0) {
			// determine whether the file is younger than the expiry time
			Poco::Timestamp now;
			// timestamp difference is in microseconds
			if ((now - file.getLastModified()) / 1000 < this->expiryMs)
				// file is not yet expired, reload
				this->needsReload = true;
			else
				this->valuePort->setError(Error::VALUE_NOT_AVAILABLE);
		}
		else
			// load initial state
			this->needsReload = true;
	}
}


///////////////////////////////////////////////////////////////////////////////
// AggregatorPort
///////////////////////////////////////////////////////////////////////////////

AggregatorPort::Calculation::Calculation(std::string id) : opdi::DialPort(id.c_str()) {
	this->algorithm = UNKNOWN;
	this->allowIncomplete = false;
}

void AggregatorPort::Calculation::calculate(AggregatorPort* aggregator) {
	aggregator->logExtreme("Calculating new value");
	if ((aggregator->values.size() < aggregator->totalValues) && !this->allowIncomplete)
		aggregator->logDebug("Cannot compute result because not all values have been collected and AllowIncomplete is false");
	else {
		// copy values, multiply if necessary
		std::vector<int64_t> values = aggregator->values;
		if (aggregator->multiplier != 1) {
			std::transform(values.begin(), values.end(), values.begin(), std::bind1st(std::multiplies<int64_t>(), aggregator->multiplier));
		}
		switch (this->algorithm) {
		case DELTA: {
			int64_t newValue = values.at(values.size() - 1) - values.at(0);
			aggregator->logDebug(std::string() + "New value according to Delta algorithm: " + this->to_string(newValue));
			if ((newValue >= this->getMin()) && (newValue <= this->getMax()))
				this->setPosition(newValue);
			else
				aggregator->logWarning(std::string() + "Cannot set new position: Calculated delta value is out of range: " + this->to_string(newValue));
			break;
		}
		case ARITHMETIC_MEAN: {
			int64_t sum = std::accumulate(values.begin(), values.end(), (int64_t)0);
			int64_t mean = sum / values.size();
			aggregator->logDebug(std::string() + "New value according to ArithmeticMean algorithm: " + this->to_string(mean));
			if ((mean >= this->getMin()) && (mean <= this->getMax()))
				this->setPosition(mean);
			else
				aggregator->logWarning(std::string() + "Cannot set new position: Calculated average value is out of range: " + this->to_string(mean));
			break;
		}
		case MINIMUM: {
			auto minimum = std::min_element(values.begin(), values.end());
			if (minimum == values.end())
				this->setError(Error::VALUE_NOT_AVAILABLE);
			int64_t min = *minimum;
			aggregator->logDebug(std::string() + "New value according to Minimum algorithm: " + this->to_string(min));
			if ((min >= this->getMin()) && (min <= this->getMax()))
				this->setPosition(min);
			else
				aggregator->logWarning(std::string() + "Cannot set new position: Calculated minimum value is out of range: " + this->to_string(min));
			break;
		}
		case MAXIMUM: {
			auto maximum = std::max_element(values.begin(), values.end());
			if (maximum == values.end())
				this->setError(Error::VALUE_NOT_AVAILABLE);
			int64_t max = *maximum;
			aggregator->logDebug(std::string() + "New value according to Maximum algorithm: " + this->to_string(max));
			if ((max >= this->getMin()) && (max <= this->getMax()))
				this->setPosition(max);
			else
				aggregator->logWarning(std::string() + "Cannot set new position: Calculated minimum value is out of range: " + this->to_string(max));
			break;
		}
		default:
			throw Poco::ApplicationException("Algorithm not supported");
		}
	}
}

// AggregatorPort class implementation

void AggregatorPort::persist() {
	// update persistent storage?
	if (this->isPersistent() && (this->openhat->persistentConfig != nullptr)) {
		try {
			if (this->openhat->shutdownRequested)
				this->logVerbose("Trying to persist aggregator values on shutdown");
			else
				this->logDebug("Trying to persist aggregator values");
			if (this->values.size() == 0) {
				this->openhat->persistentConfig->remove(this->ID() + ".Time");
				this->openhat->persistentConfig->remove(this->ID() + ".Values");
			}
			else {
				// use current time as persistence timestamp
				this->openhat->persistentConfig->setUInt64(this->ID() + ".Time", opdi_get_time_ms());
				std::stringstream ss;
				auto vit = this->values.cbegin();
				auto vitb = this->values.cbegin();
				auto vite = this->values.cend();
				while (vit != vite) {
					if (vit != vitb)
						ss << ",";
					ss << this->to_string(*vit);
					++vit;
				}
				this->openhat->persistentConfig->setString(this->ID() + ".Values", ss.str());
			}
			// save configuration only if not already shutting down
			// the config will be saved on shutdown automatically
			if (!this->openhat->shutdownRequested)
				this->openhat->savePersistentConfig();
		}
		catch (Poco::Exception& e) {
			this->logWarning("Error persisting aggregator values: " + this->openhat->getExceptionMessage(e));
		}
		catch (std::exception& e) {
			this->logWarning(std::string("Error persisting aggregator values: ") + e.what());
		}
	}
}

void AggregatorPort::resetValues(std::string reason, opdi::LogVerbosity logVerbosity, bool clearPersistent) {
	switch (logVerbosity) {
	case opdi::LogVerbosity::EXTREME:
		this->logExtreme("Resetting aggregator; values are now unavailable because: " + reason); break;
	case opdi::LogVerbosity::DEBUG:
		this->logDebug("Resetting aggregator; values are now unavailable because: " + reason); break;
	case opdi::LogVerbosity::VERBOSE:
		this->logVerbose("Resetting aggregator; values are now unavailable because: " + reason); break;
	case opdi::LogVerbosity::NORMAL:
		this->logNormal("Resetting aggregator; values are now unavailable because: " + reason); break;
	default: break;
	}
	// indicate errors on all calculations
	auto it = this->calculations.begin();
	auto ite = this->calculations.end();
	while (it != ite) {
		(*it)->setError(Error::VALUE_NOT_AVAILABLE);
		++it;
	}
	this->values.clear();
	// remove values from persistent storage
	if (clearPersistent && this->isPersistent() && (this->openhat->persistentConfig != nullptr)) {
		this->openhat->persistentConfig->remove(this->ID() + ".Time");
		this->openhat->persistentConfig->remove(this->ID() + ".Values");
		this->openhat->savePersistentConfig();
	}
	// clear history of associated port
	if (this->setHistory && this->historyPort != nullptr)
		this->historyPort->clearHistory();
}

uint8_t AggregatorPort::doWork(uint8_t canSend) {
	uint8_t result = opdi::DigitalPort::doWork(canSend);
	if (result != OPDI_STATUS_OK)
		return result;

	// disabled?
	if (this->line != 1) {
		// this->logExtreme("Aggregator is disabled");
		return OPDI_STATUS_OK;
	}

	bool valuesAvailable = false;

	if (this->firstRun) {
		this->firstRun = false;

		// try to read values from persistent storage?
		if (this->isPersistent() && (this->openhat->persistentConfig != nullptr)) {
			this->logVerbose("Trying to read persisted aggregator values with current time being " + this->to_string(opdi_get_time_ms()));
			// read timestamp
			uint64_t persistTime = this->openhat->persistentConfig->getUInt64(this->ID() + ".Time", 0);
			// timestamp acceptable? must be in the past and within the query interval
			int64_t elapsed = opdi_get_time_ms() - persistTime;
			if ((elapsed > 0) && (elapsed < this->queryInterval * 1000)) {
				// remember persistent time as last query time
				this->lastQueryTime = persistTime;
				// read values
				std::string persistedValues = this->openhat->persistentConfig->getString(this->ID() + ".Values", "");
				// tokenize along commas
				std::stringstream ss(persistedValues);
				std::string item;
				while (std::getline(ss, item, ',')) {
					// parse item and store it
					try {
						int64_t value = Poco::NumberParser::parse64(item);
						this->values.push_back(value);
					}
					catch (Poco::Exception& e) {
						// any error causes a reset and aborts processing
						this->lastQueryTime = 0;
						this->resetValues("An error occurred deserializing persisted values: " + this->openhat->getExceptionMessage(e), opdi::LogVerbosity::NORMAL);
						break;
					}
				}	// read values
				valuesAvailable = true;
				this->logVerbose("Total persisted aggregator values read: " + this->to_string(this->values.size()));
			}	// timestamp valid
			else
				this->logVerbose("Persisted aggregator values not found or outdated, timestamp was: " + to_string(persistTime));
		}	// persistence enabled
	}

	// time to read the next value?
	if (opdi_get_time_ms() - this->lastQueryTime > (uint64_t)this->queryInterval * 1000) {
		this->lastQueryTime = opdi_get_time_ms();

		double value;
		try {
			// get the port's value
			value = this->openhat->getPortValue(this->sourcePort);
			// reset error counter
			this->errors = 0;
		}
		catch (Poco::Exception &e) {
			this->logDebug("Error querying source port " + this->sourcePort->ID() + ": " + this->openhat->getExceptionMessage(e));
			// error occurred; check whether there's a last value and an error tolerance
			if ((this->values.size() > 0) && (this->allowedErrors > 0) && (this->errors < this->allowedErrors)) {
				++errors;
				// fallback to last value
				value = (double)this->values.at(this->values.size() - 1);
				this->logDebug("Fallback to last read value, remaining allowed errors: " + this->to_string(this->allowedErrors - this->errors));
			}
			else {
				// avoid logging too many messages
				if (this->values.size() > 0) {
					this->resetValues("Querying the source port " + this->sourcePort->ID() + " resulted in an error: " + this->openhat->getExceptionMessage(e), opdi::LogVerbosity::VERBOSE);
				}
				return OPDI_STATUS_OK;
			}
		}

		int64_t longValue = (int64_t)(value);

		this->logDebug("Newly aggregated value: " + this->to_string(longValue));

		// use first value without check
		if (this->values.size() > 0) {
			// vector overflow?
			if (this->values.size() >= this->totalValues) {
				this->values.erase(this->values.begin());
			}
			// compare against last element
			int64_t diff = this->values.at(this->values.size() - 1) - longValue;
			// diff may not exceed deltas
			if ((diff < this->minDelta) || (diff > this->maxDelta)) {
				this->logWarning("The new source port value of " + this->to_string(longValue) + " is outside of the specified limits (diff = " + this->to_string(diff) + ")");
				// error occurred; check whether there's a last value and an error tolerance
				if ((this->values.size() > 0) && (this->allowedErrors > 0) && (this->errors < this->allowedErrors)) {
					++errors;
					// fallback to last value
					value = (double)this->values.at(this->values.size() - 1);
					this->logDebug("Fallback to last read value, remaining allowed errors: " + this->to_string(this->allowedErrors - this->errors));
				}
				else {
					// an invalid value invalidates the whole calculation
					// avoid logging too many messages
					if (this->values.size() > 0) {
						this->resetValues("The value was outside of the specified limits", opdi::LogVerbosity::VERBOSE);
					}
					return OPDI_STATUS_OK;
				}
			}
			// value is ok
		}
		this->values.push_back(longValue);
		// persist values
		this->persist();
		valuesAvailable = true;
	}

	if (valuesAvailable) {
		if (this->setHistory && this->historyPort != nullptr)
			this->historyPort->setHistory(this->queryInterval, this->totalValues, this->values);
		// perform all calculations
		auto it = this->calculations.begin();
		auto ite = this->calculations.end();
		while (it != ite) {
			(*it)->calculate(this);
			++it;
		}
	}
	return OPDI_STATUS_OK;
}

AggregatorPort::AggregatorPort(AbstractOpenHAT* openhat, const char* id) : 
	// an aggregator is an output only port
	opdi::DigitalPort(id, OPDI_PORTDIRCAP_OUTPUT, 0) {
	this->opdi = this->openhat = openhat;
	this->multiplier = 1;
	// default: allow all values (set absolute limits very high)
	this->minDelta = LLONG_MIN;
	this->maxDelta = LLONG_MAX;
	this->lastQueryTime = 0;
	this->sourcePort = nullptr;
	this->queryInterval = 0;
	this->totalValues = 0;
	this->setHistory = true;
	this->historyPort = nullptr;
	this->allowedErrors = 0;		// default setting allows no errors
	// an aggregator is enabled by default
	this->setLine(1);
	this->errors = 0;
	this->firstRun = true;
}

void AggregatorPort::configure(ConfigurationView* config, ConfigurationView* parentConfig) {
	this->openhat->configureDigitalPort(config, this);

	this->sourcePortID = this->openhat->getConfigString(config, this->ID(), "SourcePort", "", true);

	this->queryInterval = config->getInt("Interval", 0);
	if (this->queryInterval <= 0) {
		this->openhat->throwSettingException(this->ID() + ": Please specify a positive value for Interval (in seconds): " + this->to_string(this->queryInterval));
	}

	this->totalValues = config->getInt("Values", 0);
	if (this->totalValues <= 1) {
		this->openhat->throwSettingException(this->ID() + ": Please specify a number greater than 1 for Values: " + this->to_string(this->totalValues));
	}

	this->multiplier = config->getInt("Multiplier", this->multiplier);
	this->minDelta = config->getInt64("MinDelta", this->minDelta);
	this->maxDelta = config->getInt64("MaxDelta", this->maxDelta);
	this->allowedErrors = config->getInt("AllowedErrors", this->allowedErrors);

	this->setHistory = config->getBool("SetHistory", this->setHistory);
	this->historyPortID = this->openhat->getConfigString(config, this->ID(), "HistoryPort", "", false);

	// enumerate calculations
	this->logVerbose(std::string("Enumerating Aggregator calculations: ") + this->ID() + ".Calculations");

	Poco::AutoPtr<ConfigurationView> nodes = this->openhat->createConfigView(config, "Calculations");
	config->addUsedKey("Calculations");

	// get list of calculations
	ConfigurationView::Keys calculations;
	nodes->keys("", calculations);

	typedef Poco::Tuple<int, std::string> Item;
	typedef std::vector<Item> ItemList;
	ItemList orderedItems;

	// create ordered list of calculations keys (by priority)
	for (auto it = calculations.begin(), ite = calculations.end(); it != ite; ++it) {

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

	if (!this->setHistory && orderedItems.size() == 0) {
		this->logWarning(std::string("No calculations configured in node ") + this->ID() + ".Calculations and history is disabled; is this intended?");
	}

	// go through items, create calculation objects
	auto nli = orderedItems.begin();
	auto nlie = orderedItems.end();
	while (nli != nlie) {
		std::string nodeName = nli->get<1>();
		this->logVerbose("Setting up aggregator calculation for node: " + nodeName);

		// get calculation section from the configuration
		Poco::AutoPtr<ConfigurationView> calculationConfig = this->openhat->createConfigView(parentConfig, nodeName);

		// get type (required)
		std::string type = this->openhat->getConfigString(calculationConfig, nodeName, "Type", "", true);

		// type must be "DialPort"
		if (type != "DialPort")
			this->openhat->throwSettingException(this->ID() + ": Invalid type for calculation section, must be 'DialPort': " + nodeName);

		// creat the dial port for the calculation
		Calculation* calc = new Calculation(nodeName);
		// initialize the port default values (taken from this port)
		calc->setGroup(this->group);
		calc->setRefreshMode(this->refreshMode);

		// configure the dial port
		this->openhat->configureDialPort(calculationConfig, calc);
		// these ports are always readonly (because they contain the result of a calculation)
		calc->setReadonly(true);

		std::string algStr = calculationConfig->getString("Algorithm", "");

		if (algStr == "Delta") {
			calc->algorithm = DELTA;
			calc->allowIncomplete = false;
		} else
		if (algStr == "ArithmeticMean" || algStr == "Average") {
			calc->algorithm = ARITHMETIC_MEAN;
			calc->allowIncomplete = true;
		} else
		if (algStr == "Minimum") {
			calc->algorithm = MINIMUM;
			calc->allowIncomplete = true;
		} else
		if (algStr == "Maximum") {
			calc->algorithm = MAXIMUM;
			calc->allowIncomplete = true;
		} else
			this->openhat->throwSettingException(this->ID() + ": Algorithm unsupported or not specified; expected 'Delta', 'ArithmeticMean'/'Average', 'Minimum', or 'Maximum': " + algStr);

		// override allowIncomplete flag if specified
		calc->allowIncomplete = calculationConfig->getBool("AllowIncomplete", calc->allowIncomplete);

		this->calculations.push_back(calc);
		
		// add port to OpenHAT
		this->openhat->addPort(calc);

		++nli;
	}

	// allocate vector
	this->values.reserve(this->totalValues);

	// set initial state
	this->resetValues("Setting initial state", opdi::LogVerbosity::VERBOSE, false);
}

void AggregatorPort::prepare() {
	this->logDebug("Preparing port");
	opdi::DigitalPort::prepare();

	// find source port; throws errors if something required is missing
	this->sourcePort = this->findPort(this->getID(), "SourcePort", this->sourcePortID, true);
	this->historyPort = this->sourcePort;
	if (!this->historyPortID.empty())
		this->historyPort = this->findPort(this->getID(), "HistoryPort", this->historyPortID, true);
}

void AggregatorPort::setLine(uint8_t newLine, ChangeSource changeSource) {
	// if being deactivated, reset values and ports to error
	if ((this->line == 1) && (newLine == 0))
		this->resetValues("Aggregator was deactivated", opdi::LogVerbosity::VERBOSE);
	opdi::DigitalPort::setLine(newLine, changeSource);
}

///////////////////////////////////////////////////////////////////////////////
// CounterPort
///////////////////////////////////////////////////////////////////////////////

CounterPort::CounterPort(AbstractOpenHAT* openhat, const char* id) : opdi::DialPort(id), increment(openhat, 1), period(openhat, 1) {
	this->opdi = this->openhat = openhat;
	this->setTypeGUID(TypeGUID);
	this->setMin(LLONG_MIN);
	this->setMax(LLONG_MAX);
	this->setPosition(0);
	this->setReadonly(true);

	this->timeBase = TimeBase::SECONDS;
	this->lastCountTime = 0;
}

void CounterPort::configure(ConfigurationView* nodeConfig) {
	this->openhat->configureDialPort(nodeConfig, this);

	std::string timeBaseStr = nodeConfig->getString("TimeBase", "");
	if (timeBaseStr == "Seconds")
		this->timeBase = TimeBase::SECONDS;
	else
	if (timeBaseStr == "Milliseconds")
		this->timeBase = TimeBase::MILLISECONDS;
	else
	if (timeBaseStr == "Frames")
		this->timeBase = TimeBase::FRAMES;
	else
	if (timeBaseStr != "")
		this->openhat->throwSettingException(this->ID() + ": Invalid value for TimeBase, expected 'Seconds', 'Milliseconds', or 'Frames': " + timeBaseStr);

	this->incrementStr = nodeConfig->getString("Increment", "");
	this->periodStr = nodeConfig->getString("Period", "");

	this->underflowPortStr = nodeConfig->getString("UnderflowPorts", "");
	this->overflowPortStr = nodeConfig->getString("OverflowPorts", "");
}

void CounterPort::prepare() {
	this->logDebug("Preparing port");
	opdi::DialPort::prepare();

	// initialize value resolvers if definitions have been specified
	if (!this->incrementStr.empty())
		this->increment.initialize(this, "Increment", this->incrementStr);
	if (!this->periodStr.empty())
		this->period.initialize(this, "Period", this->periodStr);
	// resolve port lists
	this->findDigitalPorts(this->ID(), "UnderflowPorts", this->underflowPortStr, this->underflowPorts);
	this->findDigitalPorts(this->ID(), "OverflowPorts", this->overflowPortStr, this->overflowPorts);
}

void CounterPort::doIncrement(int64_t increment) {
	int64_t newPosition = this->position + increment;

	// detect underflow
	if ((increment < 0) && (newPosition < this->minValue)) {
		// underflow detected, wrap around
		newPosition = this->maxValue - (this->minValue - newPosition);
		// set underflow ports to High
		auto it = this->underflowPorts.begin();
		auto ite = this->underflowPorts.end();
		while (it != ite) {
			(*it)->setLine(1);
			++it;
		}
	} else if (increment < 0) {
		// no underflow, reset underflow ports
		auto it = this->underflowPorts.begin();
		auto ite = this->underflowPorts.end();
		while (it != ite) {
			(*it)->setLine(0);
			++it;
		}
	}
	// detect overflow
	if ((increment > 0) && (newPosition > this->maxValue)) {
		// overflow detected, wrap around
		newPosition = this->minValue + (newPosition - this->maxValue);
		// set overflow ports to High
		auto it = this->overflowPorts.begin();
		auto ite = this->overflowPorts.end();
		while (it != ite) {
			(*it)->setLine(1);
			++it;
		}
	} else if (increment > 0) {
		// no overflow, reset overflow ports
		auto it = this->overflowPorts.begin();
		auto ite = this->overflowPorts.end();
		while (it != ite) {
			(*it)->setLine(0);
			++it;
		}
	}

	this->setPosition(newPosition);

	switch (this->timeBase) {
	case TimeBase::SECONDS:
			this->lastCountTime = opdi_get_time_ms() / 1000;
			break;
	case TimeBase::MILLISECONDS:
			this->lastCountTime = opdi_get_time_ms();
			break;
	case TimeBase::FRAMES:
			this->lastCountTime = this->openhat->getCurrentFrame();
			break;
	}
}

uint8_t CounterPort::doWork(uint8_t canSend) {
	opdi::DialPort::doWork(canSend);

	int64_t period = 0;
	try {
		// get current period from value resolver
		period = this->period.value();
	}
	catch (const Poco::Exception& e) {
		this->logExtreme("Error resolving period value from '" + this->periodStr + "': " + this->openhat->getExceptionMessage(e));
	}

	// a period of 0 or less does not modify the counter
	if (period <= 0)
		return OPDI_STATUS_OK;

	// remember whether we're setting lastExecution for the first time
	bool setLastExecutionOnly = (this->lastCountTime == 0);

	// check whether the period is up, return if not
	switch (this->timeBase) {
	case TimeBase::SECONDS:
		if (opdi_get_time_ms() / 1000 - period > this->lastCountTime) {
			this->lastCountTime = opdi_get_time_ms() / 1000;
			break;
		}
		else
			return OPDI_STATUS_OK;
	case TimeBase::MILLISECONDS:
		if (opdi_get_time_ms() - period > this->lastCountTime) {
			this->lastCountTime = opdi_get_time_ms();
			break;
		}
		else
			return OPDI_STATUS_OK;
	case TimeBase::FRAMES:
		if (this->openhat->getCurrentFrame() - period > this->lastCountTime) {
			this->lastCountTime = this->openhat->getCurrentFrame();
			break;
		}
		else
			return OPDI_STATUS_OK;
	}

	// During the first run, only set the last execution time to the current time.
	// Avoid testing at the first iteration because this would cause the program
	// to exit if ExitAfterTest is true which would totally disregard the test interval.
	if (setLastExecutionOnly)
		return OPDI_STATUS_OK;

	try {
		// get current increment from value resolver
		this->doIncrement(this->increment.value());
	}
	catch (const Poco::Exception& e) {
		this->logExtreme("Error resolving increment value from '" + this->periodStr + "': " + this->openhat->getExceptionMessage(e));
	}

	return OPDI_STATUS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// TriggerPort
///////////////////////////////////////////////////////////////////////////////

TriggerPort::TriggerPort(AbstractOpenHAT* openhat, const char* id) : opdi::DigitalPort(id, OPDI_PORTDIRCAP_OUTPUT, 0) {
	this->opdi = this->openhat = openhat;

	opdi::DigitalPort::setMode(OPDI_DIGITAL_MODE_OUTPUT);
	this->line = 1;	// default: active

	this->counterPort = nullptr;
}

void TriggerPort::configure(ConfigurationView* config) {
	this->openhat->configurePort(config, this, 0);
	this->logVerbosity = this->openhat->getConfigLogVerbosity(config, opdi::LogVerbosity::UNKNOWN);

	std::string triggerTypeStr = config->getString("Trigger", "RisingEdge");
	if (triggerTypeStr == "RisingEdge")
		this->triggerType = RISING_EDGE;
	else
	if (triggerTypeStr == "FallingEdge")
		this->triggerType = FALLING_EDGE;
	else
	if (triggerTypeStr == "Both")
		this->triggerType = BOTH;
	else
		if (triggerTypeStr != "")
			this->openhat->throwSettingException(this->ID() + ": Invalid specification for 'Trigger', expected 'RisingEdge', 'FallingEdge', or 'Both'");

	std::string changeTypeStr = config->getString("Change", "Toggle");
	if (changeTypeStr == "SetHigh")
		this->changeType = SET_HIGH;
	else
	if (changeTypeStr == "SetLow")
		this->changeType = SET_LOW;
	else
	if (changeTypeStr == "Toggle")
		this->changeType = TOGGLE;
	else
		if (changeTypeStr != "")
			this->openhat->throwSettingException(this->ID() + ": Invalid specification for 'Change', expected 'SetHigh', 'SetLow', or 'Toggle'");

	this->inputPortStr = config->getString("InputPorts", "");
	if (this->inputPortStr == "")
		this->openhat->throwSettingException("Expected at least one input port for TriggerPort");

	this->outputPortStr = config->getString("OutputPorts", "");
	this->inverseOutputPortStr = config->getString("InverseOutputPorts", "");
	this->counterPortStr = config->getString("CounterPort", "");
}

void TriggerPort::setLine(uint8_t line, ChangeSource changeSource) {
	opdi::DigitalPort::setLine(line, changeSource);

	// deactivated?
	if (line == 0) {
		// reset all input port states to "unknown"
		auto it = this->portDataList.begin();
		auto ite = this->portDataList.end();
		while (it != ite) {
			(*it).set<1>(UNKNOWN);
			++it;
		}
	}
}

void TriggerPort::prepare() {
	this->logDebug("Preparing port");
	opdi::DigitalPort::prepare();

	// find ports; throws errors if something required is missing
	opdi::DigitalPortList inputPorts;
	this->findDigitalPorts(this->getID(), "InputPorts", this->inputPortStr, inputPorts);
	// go through input ports, build port state list
	auto it = inputPorts.begin();
	auto ite = inputPorts.end();
	while (it != ite) {
		PortData pd(*it, UNKNOWN);
		this->portDataList.push_back(pd);
		++it;
	}

	this->findDigitalPorts(this->getID(), "OutputPorts", this->outputPortStr, this->outputPorts);
	this->findDigitalPorts(this->getID(), "InverseOutputPorts", this->inverseOutputPortStr, this->inverseOutputPorts);
	if (!this->counterPortStr.empty()) {
		// find port
		Port* cPort = this->findPort(this->getID(), "CounterPort", this->counterPortStr, false);
		// check type by comparing the typeGUID
		if (cPort->getTypeGUID() != CounterPort::TypeGUID)
			throw Poco::InvalidArgumentException(this->ID() + ": The specified ID for setting CounterPort (" + this->counterPortStr + ") does not denote a valid CounterPort");
		// cast to CounterPort; if type does not match this will be nullptr
		this->counterPort = dynamic_cast<CounterPort*>(cPort);
	}
}

uint8_t TriggerPort::doWork(uint8_t canSend)  {
	opdi::DigitalPort::doWork(canSend);

	if (this->line == 0)
		return OPDI_STATUS_OK;

	bool changeDetected = false;
	auto it = this->portDataList.begin();
	auto ite = this->portDataList.end();
	while (it != ite) {
		uint8_t mode;
		uint8_t line;
		try {
			(*it).get<0>()->getState(&mode, &line);

			if ((*it).get<1>() != UNKNOWN) {
				// state change?
				switch (this->triggerType) {
				case RISING_EDGE: 
					if (((*it).get<1>() == LOW) && (line == 1))
						changeDetected = true;
					break;
				case FALLING_EDGE: 
					if (((*it).get<1>() == HIGH) && (line == 0)) 
						changeDetected = true;
					break;
				case BOTH: 
					if ((*it).get<1>() != (line == 1 ? HIGH : LOW)) 
						changeDetected = true;
					break;
				}
			}
			// remember current state
			(*it).set<1>(line == 1 ? HIGH : LOW);
			// do not exit the loop even if a change has been detected
			// always go through all ports
		} catch (Poco::Exception&) {
			// port has an error, set it to unknown
			(*it).set<1>(UNKNOWN);
		}
		++it;
	}

	// change detected?
	if (changeDetected) {
		this->logDebug("Detected triggering change");

		// regular output ports
		auto it = this->outputPorts.begin();
		auto ite = this->outputPorts.end();
		while (it != ite) {
			try {
				if (this->changeType == SET_HIGH) {
					(*it)->setLine(1);
				} else
				if (this->changeType == SET_LOW) {
					(*it)->setLine(0);
				} else {
					// toggle
					uint8_t mode;
					uint8_t line;
					// get current output port state
					(*it)->getState(&mode, &line);
					(*it)->setLine(line == 1 ? 0 : 1);
				}
			} catch (Poco::Exception &e) {
				this->logNormal(std::string("Error changing port ") + (*it)->getID() + ": " + this->openhat->getExceptionMessage(e));
			}
			++it;
		}
		// inverse output ports
		it = this->inverseOutputPorts.begin();
		ite = this->inverseOutputPorts.end();
		while (it != ite) {
			try {
				if (this->changeType == SET_HIGH) {
					(*it)->setLine(0);
				} else
				if (this->changeType == SET_LOW) {
					(*it)->setLine(1);
				} else {
					// toggle
					uint8_t mode;
					uint8_t line;
					// get current output port state
					(*it)->getState(&mode, &line);
					(*it)->setLine(line == 1 ? 0 : 1);
				}
			} catch (Poco::Exception &e) {
				this->logNormal(std::string("Error changing port ") + (*it)->getID() + ": " + this->openhat->getExceptionMessage(e));
			}
			++it;
		}
		// increment optional counter port
		if (this->counterPort != nullptr)
			this->counterPort->doIncrement(1);
	}

	return OPDI_STATUS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// InfluxDBPort
///////////////////////////////////////////////////////////////////////////////

InfluxDBPort::InfluxDBPort(AbstractOpenHAT * openhat, const char * id) : opdi::DigitalPort(id, OPDI_PORTDIRCAP_OUTPUT, 0) {
	this->opdi = this->openhat = openhat;

	opdi::DigitalPort::setMode(OPDI_DIGITAL_MODE_OUTPUT);
	this->line = 1;	// default: active

	this->tcpPort = 8086;
	this->intervalMs = 60000;	// default: once a minute
	this->timeoutMs = 5000;		// default: five seconds
	this->lastLogTime = 0;
}

uint8_t InfluxDBPort::doWork(uint8_t canSend) {

	// need to log data?
	if (!this->postThread.isRunning() && (opdi_get_time_ms() - this->lastLogTime > this->intervalMs)) {
		this->logDebug("Preparing InfluxDB data write");

		// build data string
		this->dbData.clear();

		this->dbData.append(this->measurement);
		if (!this->tags.empty())
			this->dbData.append("," + this->tags);
		this->dbData.append(" ");

		bool hasFields = false;
		auto ite = this->ports.cend();
		for (auto it = this->ports.cbegin(); it != ite; ++it) {
			try {
				double value = this->openhat->getPortValue(*it);
				this->dbData.append((*it)->ID() + "=" + this->to_string(value));
				if (it + 1 != ite)
					this->dbData.append(",");
				hasFields = true;
			} catch (Poco::Exception& e) {
				this->logDebug("Error querying value of port " + (*it)->ID() + ": " + this->openhat->getExceptionMessage(e));
			}
		}
		if (!hasFields)
			// add dummy field (required by influxDB)
			this->dbData.append("@@dummy@@=1");

		// append timestamp (nanoseconds)
		this->dbData.append(" ");
		this->dbData.append(this->to_string(Poco::Timestamp().epochMicroseconds() * 1000));

		// data is complete, start thread to post data to the InfluxDB instance
		this->postThread.start(*this);

		this->lastLogTime = opdi_get_time_ms();
	}

	return OPDI_STATUS_OK;
}

void InfluxDBPort::run() {
	// runs in separate thread

	try {
		Poco::Net::HTTPClientSession session(this->host, this->tcpPort);
		session.setTimeout(Poco::Timespan(this->timeoutMs * 1000));

		// build the HTTP post
		std::string postUrl = "/write?db=" + this->database + (this->retentionPoint.empty() ? "" : "&rp=" + this->retentionPoint);
		Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST, postUrl);
		request.setContentLength(this->dbData.length());

		if (this->logVerbosity >= opdi::LogVerbosity::DEBUG) {
			std::string fullUrl = "http://" + this->host + ":" + this->to_string(this->tcpPort) + postUrl;
			this->logDebug("Sending InfluxDB data via POST to: " + fullUrl);
			this->logDebug("InfluxDB data: " + this->dbData);
		}
		std::ostream& myOStream = session.sendRequest(request);
		myOStream << this->dbData;

		Poco::Net::HTTPResponse res;
		std::istream& iStr = session.receiveResponse(res);
		std::stringstream ss;
		ss << iStr.rdbuf();

		if (res.getStatus() >= 500)
			throw Poco::Exception(this->to_string(res.getStatus()) + " Internal server error: " + ss.str());
		if (res.getStatus() == 404)
			throw Poco::Exception(this->to_string(res.getStatus()) + " Not found");
		if (res.getStatus() >= 400)
			throw Poco::Exception(this->to_string(res.getStatus()) + " The server did not understand the request: " + ss.str());
		if (res.getStatus() != 204) {
			throw Poco::Exception(this->to_string(res.getStatus()) + " Unable to process the request: " + ss.str());
		}

		this->logDebug("204 InfluxDB POST successful");

		return;
	} catch (Poco::Exception& e) {
		this->openhat->logError(this->ID() + ": Error sending data to " + this->host + ": " + this->openhat->getExceptionMessage(e));
	}
	
	// error case
	// fallback filename specified?
	if (!this->fallbackFile.empty()) {
		try {
			// open the stream in append mode
			Poco::FileOutputStream fos(this->fallbackFile, std::ios_base::app);
			fos.write(this->dbData.c_str(), this->dbData.length());
			fos.write("\n", 1);	// InfluxDB line separator, not platform dependent
			fos.close();
		} catch (Poco::Exception& e) {
			this->openhat->logError(this->ID() + ": Error writing to fallback file " + this->fallbackFile + ": " + this->openhat->getExceptionMessage(e));
		}
	}
}

void InfluxDBPort::configure(ConfigurationView * portConfig) {
	this->openhat->configureDigitalPort(portConfig, this);

	this->host = openhat->getConfigString(portConfig, this->ID(), "Host", "", true);
	this->tcpPort = (uint16_t)portConfig->getInt("TCPPort", this->tcpPort);
	this->database = openhat->getConfigString(portConfig, this->ID(), "Database", "", true);
	this->measurement = openhat->getConfigString(portConfig, this->ID(), "Measurement", "", true);
	this->retentionPoint = openhat->getConfigString(portConfig, this->ID(), "RetentionPoint", "", false);
	this->fallbackFile = openhat->getConfigString(portConfig, this->ID(), "FallbackFile", "", false);

	this->intervalMs = portConfig->getInt64("Interval", this->intervalMs);
	if (this->intervalMs < 1000)
		throw Poco::InvalidArgumentException(this->ID() + ": Please specify an interval in milliseconds, at least 1000");
	this->timeoutMs = portConfig->getInt64("Timeout", this->timeoutMs);
	if (this->timeoutMs > this->intervalMs)
		throw Poco::InvalidArgumentException(this->ID() + ": Timeout may not exceed log interval of " + this->to_string(this->intervalMs) + " ms: " + this->to_string(this->timeoutMs));

	this->portStr = openhat->getConfigString(portConfig, this->ID(), "Ports", "", true);
}

void InfluxDBPort::prepare() {
	this->logDebug("Preparing port");
	opdi::DigitalPort::prepare();

	this->lastLogTime = opdi_get_time_ms();

	this->openhat->findPorts(this->ID(), "Ports", this->portStr, this->ports);
}

///////////////////////////////////////////////////////////////////////////////
// TestPort
///////////////////////////////////////////////////////////////////////////////

uint8_t TestPort::doWork(uint8_t canSend) {
	// test only if High and interval is specified
	if ((this->line == 0) || (this->interval <= 0)) {
		// reset execution time (to start over when it's being switched on again)
		this->lastExecution = 0;
		return OPDI_STATUS_OK;
	}

	// remember whether we're setting lastExecution for the first time
	bool setLastExecutionOnly = (this->lastExecution == 0);

	// check whether the interval time is up, return if not
	switch (this->timeBase) {
	case TimeBase::SECONDS:
		if (opdi_get_time_ms() / 1000 - this->interval > this->lastExecution) {
			this->lastExecution = opdi_get_time_ms() / 1000;
			break;
		}
		else
			return OPDI_STATUS_OK;
	case TimeBase::MILLISECONDS:
		if (opdi_get_time_ms() - this->interval > this->lastExecution) {
			this->lastExecution = opdi_get_time_ms();
			break;
		}
		else
			return OPDI_STATUS_OK;
	case TimeBase::FRAMES:
		if (this->openhat->getCurrentFrame() - this->interval > this->lastExecution) {
			this->lastExecution = this->openhat->getCurrentFrame();
			break;
		}
		else
			return OPDI_STATUS_OK;
	}

	// During the first run, only set the last execution time to the current time.
	// Avoid testing at the first iteration because this would cause the program
	// to exit if ExitAfterTest is true which would totally disregard the test interval.
	if (setLastExecutionOnly)
		return OPDI_STATUS_OK;

	this->logVerbose("Running test cases...");
	bool testPassed = true;

	// test is due, go through test values
	auto it = this->testValues.begin();
	auto ite = this->testValues.end();
	opdi::Port* port = nullptr;
	while (it != ite) {
		// split test property name (expect portID.property)
		std::vector<std::string> argList;
		std::stringstream ss(it->first);
		std::string item;
		while (std::getline(ss, item, ':')) {
			if (!item.empty())
				argList.push_back(item);
		}
		// test case definitions are assumed to be validated in configure(), so no need to check again
		// if (argList.size() != 2) ...

		// check port ID; retrieve port if different
		if ((port == nullptr) || (port->ID() != argList[0])) {
			port = this->openhat->findPort(this->id, "test case definition", argList[0], true);
		}
		this->logDebug("Testing: " + argList[1] + " == " + it->second);
		try {
			// test property against its value
			port->testValue(argList[1], it->second);
		}
		catch (const TestValueMismatchException& tvme) {
			testPassed = false;
			// test has failed
			if (this->warnOnFailure)
				this->logWarning("Test failed: " + this->openhat->getExceptionMessage(tvme));
			else {
				this->openhat->logError(this->ID() + ": Test failed: " + this->openhat->getExceptionMessage(tvme));
				return OPENHATD_TEST_FAILED;
			}
		}
		catch (const Poco::Exception& pe) {
			testPassed = false;
			if (this->warnOnFailure)
				this->logWarning("Test failed due to: " + this->openhat->getExceptionMessage(pe));
			else {
				this->openhat->logError(this->ID() + ": Test failed due to : " + this->openhat->getExceptionMessage(pe));
				return OPENHATD_TEST_FAILED;
			}
		}

		++it;
	}

	if (testPassed)
		this->logVerbose("Test passed");
	else
		this->logVerbose("Test failed");

	if (this->exitAfterTest) {
		this->logVerbose("Exiting after test. Shutting down...");
		this->openhat->shutdown(OPENHATD_TEST_EXIT);
	}

	return OPDI_STATUS_OK;
}

TestPort::TestPort(AbstractOpenHAT* openhat, const char *id) : opdi::DigitalPort(id) {
	this->opdi = this->openhat = openhat;
	this->timeBase = TimeBase::SECONDS;
	this->interval = 0;
	this->lastExecution = 0;
	this->warnOnFailure = false;
	this->exitAfterTest = false;
	// tests are active and hidden by default
	this->line = 1;
	this->hidden = true;
}

void TestPort::configure(ConfigurationView* portConfig, ConfigurationView* parentConfig) {
	this->openhat->configureDigitalPort(portConfig, this);

	std::string timeBaseStr = portConfig->getString("TimeBase", "");
	if (timeBaseStr == "Seconds")
		this->timeBase = TimeBase::SECONDS;
	else
	if (timeBaseStr == "Milliseconds")
		this->timeBase = TimeBase::MILLISECONDS;
	else
	if (timeBaseStr == "Frames")
		this->timeBase = TimeBase::FRAMES;
	else
	if (timeBaseStr != "")
		this->openhat->throwSettingException(this->ID() + ": Invalid value for TimeBase, expected 'Seconds', 'Milliseconds', or 'Frames': " + timeBaseStr);

	this->interval = portConfig->getInt64("Interval", this->interval);
	if (this->interval <= 0)
		this->logWarning("Interval not specified or less than 1, test is disabled");
	this->warnOnFailure = portConfig->getBool("WarnOnFailure", this->warnOnFailure);
	this->exitAfterTest = portConfig->getBool("ExitAfterTest", this->exitAfterTest);

	// enumerate test cases
	this->logVerbose(std::string("Enumerating test cases: ") + this->ID() + ".Cases");

	Poco::AutoPtr<ConfigurationView> nodes = this->openhat->createConfigView(portConfig, "Cases");
	portConfig->addUsedKey("Cases");

	// get list of cases
	ConfigurationView::Keys cases;
	nodes->keys("", cases);

	// create ordered list of cases (std::map internally orders by keys)
	for (auto it = cases.begin(), ite = cases.end(); it != ite; ++it) {
		// split test property name (expect portID.property)
		std::vector<std::string> argList;
		std::stringstream ss(*it);
		std::string item;
		while (std::getline(ss, item, ':')) {
			if (!item.empty())
				argList.push_back(item);
		}
		if (argList.size() != 2)
			throw Poco::InvalidArgumentException(this->ID() + ": Test case definition error, expected <targetPortID>:<property>, found: '" + *it + "'");
		if (nodes->getString(*it).empty())
			throw Poco::InvalidArgumentException(this->ID() + ": Test case definition error, empty expected value for property: '" + *it + "'");

		this->testValues[*it] = nodes->getString(*it);
	}

	if (this->testValues.size() == 0)
		this->logWarning(std::string("No test cases configured in node ") + this->ID() + ".Cases; is this intended?");
	else
		this->logVerbose("Found " + this->to_string(this->testValues.size()) + " test cases.");
}

///////////////////////////////////////////////////////////////////////////////
// AssignmentPort
///////////////////////////////////////////////////////////////////////////////

AssignmentPort::AssignmentPort(AbstractOpenHAT* openhat, const char* id) : opdi::DigitalPort(id)  {
	this->opdi = this->openhat = openhat;
	this->setMode(OPDI_DIGITAL_MODE_OUTPUT);
}

void AssignmentPort::configure(ConfigurationView* portConfig, ConfigurationView* parentConfig) {
	this->openhat->configureDigitalPort(portConfig, this);

	// enumerate assignments
	this->logVerbose(std::string("Enumerating assignments: ") + this->ID() + ".Assignments");

	Poco::AutoPtr<ConfigurationView> nodes = this->openhat->createConfigView(portConfig, "Assignments");
	portConfig->addUsedKey("Assignments");

	// get list of assignments
	ConfigurationView::Keys assignments;
	nodes->keys("", assignments);

	// create ordered list of cases (std::map internally orders by keys)
	for (auto it = assignments.begin(), ite = assignments.end(); it != ite; ++it) {
		// initialize ValueResolver (a bit cumbersome to avoid the need for a standard constructor)
		this->assignmentValues.emplace(std::map<std::string, opdi::ValueResolver<double>>::value_type(*it, opdi::ValueResolver<double>(this->openhat)));
		this->assignmentValues.find(*it)->second.initialize(this, *it, nodes->getString(*it));
	}

	if (this->assignmentValues.size() == 0)
		this->logWarning(std::string("No assignments configured in node ") + this->ID() + ".Assignments; is this intended?");
	else
		this->logVerbose("Found " + this->to_string(this->assignmentValues.size()) + " assignments.");
}

void AssignmentPort::setLine(uint8_t line, ChangeSource changeSource) {
	// Important: this method does not call the base method to avoid
	// refreshes and OnChange* handler processing. They are not necessary
	// because the line is immediately returned to Low.
	// It is necessary to remember the value internally to avoid recursive
	// calls due to cycles in the assignment configuration, however.
	
	// state changed to High?
	if ((this->line == 0) && (line == 1)) {
		this->line = 1;

		// process assignments
		auto it = this->assignmentValues.begin();
		auto ite = this->assignmentValues.end();
		while (it != ite) {

			try {
				// find port
				opdi::Port* port = this->openhat->findPort(this->id, "assignment definition", it->first, true);
				if (port == nullptr)
					throw Poco::Exception("Port not found");

				// resolve current assignment value
				double value = this->assignmentValues.find(it->first)->second.value();

				// assignment depends on port type
				if (port->getType()[0] == OPDI_PORTTYPE_DIGITAL[0]) {
					((opdi::DigitalPort*)port)->setLine(value == 0 ? 0 : 1, changeSource);
				}
				else
				if (port->getType()[0] == OPDI_PORTTYPE_ANALOG[0]) {
					// analog port: relative value (0..1)
					((opdi::AnalogPort*)port)->setRelativeValue(value, changeSource);
				}
				else
				if (port->getType()[0] == OPDI_PORTTYPE_DIAL[0]) {
					// dial port: absolute value
					int64_t position = (int64_t)value;
					((opdi::DialPort*)port)->setPosition(position, changeSource);
				}
				else
				if (port->getType()[0] == OPDI_PORTTYPE_SELECT[0]) {
					// select port: current position number
					uint16_t position = (uint16_t)value;
					((opdi::SelectPort*)port)->setPosition(position, changeSource);
				}
				else
					// port type not supported
					throw Poco::Exception("Port type not supported");

				this->logDebug("Assigned value to " + port->ID() + ": " + this->to_string(value));
			}
			catch (const Poco::Exception& pe) {
				this->logExtreme(this->ID() + ": Assignment to port '" + it->first + "' failed due to : " + this->openhat->getExceptionMessage(pe));
			}

			++it;
		}
	}

	// the line is always Low
	this->line = 0;
}

}		// namespace openhat
