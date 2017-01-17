//    Copyright (C) 2011-2016 OpenHAT contributors (https://openhat.org, https://github.com/openhat-org)
//    All rights reserved.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/** OPDI C++ wrapper implementation
 */

#include "OPDI.h"

#include <cstdlib>
#include <algorithm>    // std::sort
#include <string.h>
#include <unordered_set>

#include "opdi_constants.h"
#include "opdi_protocol.h"
#include "opdi_slave_protocol.h"
#include "opdi_config.h"

#include "opdi_configspecs.h"
#include "opdi_platformfuncs.h"

namespace opdi {

//////////////////////////////////////////////////////////////////////////////////////////
// Main class for OPDI functionality
//////////////////////////////////////////////////////////////////////////////////////////

uint8_t OPDI::shutdownInternal(void) {
	// shutdown and free all ports
	auto it = this->ports.begin();
	auto ite = this->ports.end();
	while (it != ite) {
		// ignore any errors during this process
		try {
			(*it)->shutdown();
			delete *it;
		}
		catch (...) {}
		++it;
	}
	this->ports.clear();
	this->disconnect();
	return this->shutdownExitCode;
}

OPDI::OPDI(void) {
	this->shutdownRequested = false;
	this->shutdownExitCode = OPDI_STATUS_OK;
}

uint8_t OPDI::setup(const char* slaveName, int idleTimeout) {
	// initialize port list
	this->ports.clear();
	this->groups.clear();
	//this->first_portGroup = nullptr;
	//this->last_portGroup = nullptr;

	// copy slave name to internal buffer
	this->slaveName = slaveName;
	// set standard encoding to "utf-8"
	this->encoding = "utf-8";

	this->idle_timeout_ms = idleTimeout;

	return opdi_slave_init();
}

void OPDI::setIdleTimeout(uint32_t idleTimeoutMs) {
	this->idle_timeout_ms = idleTimeoutMs;
}

void OPDI::setEncoding(const std::string& encoding) {
	this->encoding = encoding;
}

std::string OPDI::getSlaveName(void) {
	return this->slaveName;
}

uint8_t OPDI::setMasterName(const std::string& masterName) {
	this->masterName = masterName;
	return OPDI_STATUS_OK;
}

std::string OPDI::getEncoding(void) {
	return this->encoding;
}

uint8_t OPDI::setLanguages(const std::string& languages) {
	this->languages = languages;
	return OPDI_STATUS_OK;
}

uint8_t OPDI::setUsername(const std::string& userName) {
	this->username = userName;
	return OPDI_STATUS_OK;
}

uint8_t OPDI::setPassword(const std::string& /*password*/) {
	return OPDI_STATUS_OK;
}

std::string OPDI::getExtendedDeviceInfo(void) {
	return "";
}

std::string OPDI::getExtendedPortInfo(char* portID, uint8_t* code) {
	opdi::Port* port = this->findPortByID(portID, false);
	if (port == nullptr) {
		*code = OPDI_PORT_UNKNOWN;
		return "";
	} else {
		*code = OPDI_STATUS_OK;
		return port->getExtendedInfo();
	}
}

std::string OPDI::getExtendedPortState(char* portID, uint8_t* code) {
	opdi::Port* port = this->findPortByID(portID, false);
	if (port == nullptr) {
		*code = OPDI_PORT_UNKNOWN;
		return "";
	} else {
		*code = OPDI_STATUS_OK;
		return port->getExtendedState();
	}
}

void OPDI::addPort(opdi::Port* port) {
	// associate port with this instance
	port->opdi = this;

	// first port?
	if (this->ports.size() == 0)
		this->currentOrderID = 0;

	this->ports.push_back(port);

	this->updatePortData(port);

	// do not use hidden ports for display sort order
	if (!port->isHidden()) {
		// order not defined?
		if (port->orderID < 0) {
			port->orderID = this->currentOrderID;
			this->currentOrderID++;
		} else
			if (this->currentOrderID < port->orderID)
				this->currentOrderID = port->orderID + 1;
	}
}

// possible race conditions here, if one thread updates port data while the other retrieves it
// - generally not a problem because slaves are usually single-threaded
void OPDI::updatePortData(opdi::Port* port) {
	// allocate port data structure if necessary
	opdi_Port* oPort = (opdi_Port*)port->data;
	if (oPort == nullptr) {
		oPort = (opdi_Port*)malloc(sizeof(opdi_Port));
		port->data = oPort;
		oPort->info.i = 0;
		oPort->info.ptr = nullptr;
		oPort->next = nullptr;
	}
	// update data
	oPort->id = (const char*)port->id;
	oPort->name = (const char*)port->label;
	oPort->type = (const char*)port->type;
	oPort->caps = (const char*)port->caps;
	oPort->flags = port->flags;

	// more complex ports require the pointer to contain additional information

	// check port type
	if (strcmp(port->type, OPDI_PORTTYPE_SELECT) == 0) {
		oPort->info.ptr = static_cast<opdi::SelectPort*>(port)->labels;
	} else
	if (strcmp(port->type, OPDI_PORTTYPE_DIAL) == 0) {
		// release additional data structure memory
		if (oPort->info.ptr != nullptr)
			free(oPort->info.ptr);
		// allocate additional data structure memory
		struct opdi_DialPortInfo* dialPortInfo = (struct opdi_DialPortInfo*)malloc(sizeof(opdi_DialPortInfo));
		oPort->info.ptr = dialPortInfo;
		dialPortInfo->min = static_cast<opdi::DialPort*>(port)->minValue;
		dialPortInfo->max = static_cast<opdi::DialPort*>(port)->maxValue;
		dialPortInfo->step = static_cast<opdi::DialPort*>(port)->step;
	} else
		oPort->info.ptr = port->ptr;
}

opdi::Port* OPDI::findPort(opdi_Port* port) {
	if (port == nullptr)
		return *this->ports.begin();
	auto it = this->ports.begin();
	auto ite = this->ports.end();
	while (it != ite) {
		if ((opdi_Port*)(*it)->data == port)
			return *it;
		++it;
	}
	// not found
	return nullptr;
}

opdi::PortList& OPDI::getPorts() {
	return this->ports;
}

opdi::Port* OPDI::findPortByID(const char* portID, bool caseInsensitive) {
	auto it = this->ports.begin();
	auto ite = this->ports.end();
	while (it != ite) {
		opdi_Port* oPort = (opdi_Port*)(*it)->data;
		if (caseInsensitive) {
#ifdef linux
			if (strcasecmp(oPort->id, portID) == 0)
#else
			if (strcmpi(oPort->id, portID) == 0)
#endif
				return *it;
		} else {
			if (strcmp(oPort->id, portID) == 0)
				return *it;
		}
		++it;
	}
	// not found
	return nullptr;
}

void OPDI::updatePortGroupData(opdi::PortGroup *group) {
	// allocate port group data structure if necessary
	opdi_PortGroup *oGroup = (opdi_PortGroup *)group->data;
	if (oGroup == nullptr) {
		oGroup = (opdi_PortGroup *)malloc(sizeof(opdi_PortGroup));
		group->data = oGroup;
		oGroup->next = nullptr;
	}
	// update data
	oGroup->id = (const char*)group->id;
	oGroup->label = (const char*)group->label;
	oGroup->parent = (const char*)group->parent;
	oGroup->flags = group->flags;
	oGroup->extendedInfo = group->extendedInfo;
}

void OPDI::addPortGroup(opdi::PortGroup *portGroup) {
	// associate port with this instance
	portGroup->opdi = this;
	/*
	// first added port?
	if (this->first_portGroup == nullptr) {
		this->first_portGroup = portGroup;
		this->last_portGroup = portGroup;
	} else {
		// subsequently added port group, add to list
		this->last_portGroup->next = portGroup;
		this->last_portGroup = portGroup;
	}
	*/
	this->groups.push_back(portGroup);
	this->updatePortGroupData(portGroup);

	opdi_add_portgroup((opdi_PortGroup*)portGroup->data);
}

opdi::PortGroupList& OPDI::getPortGroups(void) {
	return this->groups;
}


void OPDI::sortPorts(void) {
	std::sort(this->ports.begin(), this->ports.end(), [] (opdi::Port* i, opdi::Port* j) { return i->orderID < j->orderID; });
}

void OPDI::preparePorts(void) {
	auto it = this->ports.begin();
	auto ite = this->ports.end();
	while (it != ite) {
		(*it)->prepare();

		// add ports to the OPDI C subsystem; ignore hidden ports
		if (!(*it)->isHidden()) {
			int result = opdi_add_port((opdi_Port*)(*it)->data);
			if (result != OPDI_STATUS_OK)
				throw Poco::ApplicationException("Unable to add port: " + (*it)->ID() + "; code = " + (*it)->to_string(result));
		}
		++it;
	}
}

uint8_t OPDI::start() {
	opdi_Message message;
	uint8_t result;

	result = opdi_get_message(&message, OPDI_CANNOT_SEND);
	if (result != OPDI_STATUS_OK) {
		return result;
	}

	// reset idle timer
	this->last_activity = opdi_get_time_ms();

	// initiate handshake
	result = opdi_slave_start(&message, nullptr, nullptr);

	return result;
}

uint8_t OPDI::waiting(uint8_t canSend) {
	if (this->shutdownRequested) {
		return this->shutdownInternal();
	}

	// remember canSend flag
	this->canSend = canSend;

	// call ports' doWork function
	auto it = this->ports.begin();
	auto ite = this->ports.end();
	while (it != ite) {
		uint8_t result = (*it)->doWork(canSend);
		if (result != OPDI_STATUS_OK)
			return result;
		++it;
	}
	return OPDI_STATUS_OK;
}

uint8_t OPDI::isConnected() {
	return opdi_slave_connected();
}

uint8_t OPDI::disconnect() {
	if (!this->isConnected() || !this->canSend) {
		return OPDI_DISCONNECTED;
	}
	return opdi_disconnect();
}

uint8_t OPDI::reconfigure() {
	if (!this->isConnected() || !this->canSend)
		return OPDI_DISCONNECTED;
	return opdi_reconfigure();
}

uint8_t OPDI::refresh(opdi::Port** ports) {
	if (!this->isConnected() || !this->canSend)
		return OPDI_DISCONNECTED;
	opdi_Port* iPorts[OPDI_MAX_MESSAGE_PARTS + 1];
	iPorts[0] = nullptr;
	if (ports == nullptr)
		return opdi_refresh(iPorts);
	// target array of internal ports to refresh
	opdi::Port* port = ports[0];
	uint8_t i = 0;
	while (port != nullptr) {
		opdi_Port* oPort = (opdi_Port*)port->data;
		iPorts[i] = oPort;
		if (++i > OPDI_MAX_MESSAGE_PARTS)
			return OPDI_ERROR_PARTS_OVERFLOW;
		port = ports[i];
	}
	iPorts[i] = nullptr;
	return opdi_refresh(iPorts);
}

uint8_t OPDI::idleTimeoutReached() {
	if (this->isConnected() && this->canSend) {
		opdi_send_debug("Idle timeout!");
	}
	return this->disconnect();
}

uint8_t OPDI::messageHandled(channel_t channel, const char** /*parts*/) {
	// a complete message has been processed; it's now safe to send
	this->canSend = true;

	// idle timeout detection active?
	if (this->idle_timeout_ms > 0) {
		// Channels above a given threshold do reset the activity time.
		// Messages on channels below this do not reset it. Usually the
		// channel assignments are as follows:
		// 0: control channel
		// 1: refresh channel
		// 2 - 19: reserved for streaming ports
		// 20 and more: user interaction channels
		// We want to reset the activity only if a "real" user interaction occurs.
		// That means, ping messages and automatic refreshes caused by the device
		// should not cause the device to be connected indefinitely.
		// TODO find a better way to specify this (masters must respect this convention)
		if (channel >= 20) {
			// reset activity time
			this->last_activity = opdi_get_time_ms();
		} else {
			// non-resetting message

			// check idle timeout
			if (opdi_get_time_ms() - this->last_activity > this->idle_timeout_ms) {
				return this->idleTimeoutReached();
			}
		}
	}

	return OPDI_STATUS_OK;
}

void OPDI::shutdown(int exitCode) {
	// set flag to indicate that the message processing should stop
	// this method may be called asynchronously by signal handlers or threads,
	// so it is very important not to do any actual work here
	this->shutdownRequested = true;
	this->shutdownExitCode = exitCode;
}

void OPDI::persist(opdi::Port*  /*port*/) {
	throw Poco::NotImplementedException("This implementation does not support port state persistance");
}

void OPDI::findPortIDs(const std::string & spec, std::vector<std::string>& results) {
	/*
	Returns a list of currently registered port IDs that match the given specifications.
	Specifications are a space - separated list of the following possibilities :
		PortID (must match exactly)
		id = <PortID - Regex>
		group = <Group - Regex>
		tag = <Tag-Regex> (tags are split at spaces, one tag must match)
		A specification can be inverted by prepending !(e.g. !Port1 excludes Port1 should it be included elsewhere).
	*/

	std::unordered_set<std::string> toAdd;
	std::unordered_set<std::string> toRemove;

	// split spec at spaces
	std::stringstream ss(spec);
	std::string item;
	while (std::getline(ss, item, ' ')) {
		if (item.length() == 0)
			continue;
		if (item == "*") {
			// add all ports
			auto ite = this->getPorts().end();
			for (auto it = this->getPorts().begin(); it != ite; ++it)
				toAdd.insert((*it)->ID());
		}
		else {
			bool inverted = (item[0] == '!');
			if (inverted)
				item = item.substr(1);
			if (item.length() == 0)
				throw Poco::InvalidArgumentException("Port ID specification error: '!' must be followed by something");
			// select correct set
			std::unordered_set<std::string>& resultSet = (inverted ? toRemove : toAdd);
			if (item.find("id=") == 0) {
				std::string rSpec = item.substr(3);
				if (rSpec.length() == 0)
					throw Poco::InvalidArgumentException("Port ID specification error: 'id=' must be followed by something");
				// construct a regex
				try {
					Poco::RegularExpression regex(rSpec, Poco::RegularExpression::Options::RE_CASELESS);
					// go through all ports, check their IDs
					auto ite = this->getPorts().end();
					for (auto it = this->getPorts().begin(); it != ite; ++it)
						if (regex.match((*it)->ID()))
							resultSet.insert((*it)->ID());
				}
				catch (Poco::Exception& e) {
					throw Poco::InvalidArgumentException("Port ID specification error: Regex '" + rSpec + "': " + e.message());
				}
			}
			else
			if (item.find("group=") == 0) {
				std::string rSpec = item.substr(6);
				if (rSpec.length() == 0)
					throw Poco::InvalidArgumentException("Port ID specification error: 'group=' must be followed by something");
				// construct a regex
				try {
					Poco::RegularExpression regex(rSpec, Poco::RegularExpression::Options::RE_CASELESS);
					// go through all ports, check their groups
					auto ite = this->getPorts().end();
					for (auto it = this->getPorts().begin(); it != ite; ++it)
						if (regex.match((*it)->group))
							resultSet.insert((*it)->ID());
				}
				catch (Poco::Exception& e) {
					throw Poco::InvalidArgumentException("Port ID specification error: Regex '" + rSpec + "': " + e.message());
				}
			}
			else
			if (item.find("tag=") == 0) {
				std::string rSpec = item.substr(4);
				if (rSpec.length() == 0)
					throw Poco::InvalidArgumentException("Port ID specification error: 'tag=' must be followed by something");
				// construct a regex
				try {
					Poco::RegularExpression regex(rSpec, Poco::RegularExpression::Options::RE_CASELESS);
					// go through all ports, check their tags
					auto ite = this->getPorts().end();
					for (auto it = this->getPorts().begin(); it != ite; ++it) {
						std::stringstream sst((*it)->tags);
						std::string tag;
						while (std::getline(sst, tag, ' '))
							if (regex.match(tag)) {
								resultSet.insert((*it)->ID());
								// at least one match is enough
								break;
							}
					}
				}
				catch (Poco::Exception& e) {
					throw Poco::InvalidArgumentException("Port ID specification error: Regex '" + rSpec + "': " + e.message());
				}
			}
			else
				// add as port ID
				resultSet.insert(item);
		}
	}	// while

	auto iter = toRemove.cend();
	for (auto itr = toRemove.cbegin(); itr != iter; ++itr)
		toAdd.erase(*itr);

	results.clear();
	auto ite = toAdd.end();
	for (auto it = toAdd.begin(); it != ite; ++it)
		results.push_back((*it));
}

opdi::Port* OPDI::findPort(const std::string& configPort, const std::string& setting, const std::string& portID, bool required) {
	// locate port by ID
	opdi::Port* port = this->findPortByID(portID.c_str());
	// no found but required?
	if (port == nullptr) {
		if (required)
			throw Poco::DataException(configPort + ": Port required by setting " + setting + " not found: " + portID);
		return nullptr;
	}

	return port;
}

void OPDI::findPorts(const std::string& configPort, const std::string& setting, const std::string& portSpec, opdi::PortList &portList) {
	std::vector<std::string> portIDs;
	try {
		this->findPortIDs(portSpec, portIDs);
	}
	catch (const Poco::Exception& e) {
		// wrap execption, add details
		throw Poco::InvalidArgumentException(configPort + ": Error resolving port list of setting " + setting + " ('" + portSpec + "'): " + e.message());
	}	auto ite = portIDs.cend();
	for (auto it = portIDs.cbegin(); it != ite; ++it) {
		opdi::Port* port = this->findPort(configPort, setting, (*it), true);
		if (port != nullptr)
			portList.push_back(port);
	}
}

opdi::DigitalPort* OPDI::findDigitalPort(const std::string& configPort, const std::string& setting, const std::string& portID, bool required) {
	// locate port by ID
	opdi::Port* port = this->findPortByID(portID.c_str());
	// no found but required?
	if (port == nullptr) {
		if (required)
			throw Poco::DataException(configPort + ": Port required by setting " + setting + " not found: " + portID);
		return nullptr;
	}

	// port type must be "digital"
	if (port->getType()[0] != OPDI_PORTTYPE_DIGITAL[0])
		throw Poco::DataException(configPort + ": Port specified in setting " + setting + " is not a digital port: " + portID);

	return (opdi::DigitalPort*)port;
}

void OPDI::findDigitalPorts(const std::string& configPort, const std::string& setting, const std::string& portIDs, opdi::DigitalPortList& portList) {
	opdi::PortList pList;
	// find ports by port list specification
	this->findPorts(configPort, setting, portIDs, pList);
	// go through list, check for digital ports
	auto ite = pList.cend();
	for (auto it = pList.cbegin(); it != ite; ++it) {
		opdi::Port* port = *it;
		if (strcmp(port->getType(), OPDI_PORTTYPE_DIGITAL))
			throw Poco::InvalidArgumentException(configPort + ": Error resolving port list of setting " + setting + ": The port " + port->ID() + " is not a Digital port");
		portList.push_back(dynamic_cast<DigitalPort*>(port));
	}
}

opdi::AnalogPort* OPDI::findAnalogPort(const std::string& configPort, const std::string& setting, const std::string& portID, bool required) {
	// locate port by ID
	opdi::Port* port = this->findPortByID(portID.c_str());
	// no found but required?
	if (port == nullptr) {
		if (required)
			throw Poco::DataException(configPort + ": Port required by setting " + setting + " not found: " + portID);
		return nullptr;
	}

	// port type must be "analog"
	if (port->getType()[0] != OPDI_PORTTYPE_ANALOG[0])
		throw Poco::DataException(configPort + ": Port specified in setting " + setting + " is not an analog port: " + portID);

	return (opdi::AnalogPort*)port;
}

void OPDI::findAnalogPorts(const std::string& configPort, const std::string& setting, const std::string& portIDs, opdi::AnalogPortList& portList) {
	opdi::PortList pList;
	// find ports by port list specification
	this->findPorts(configPort, setting, portIDs, pList);
	// go through list, check for digital ports
	auto ite = pList.cend();
	for (auto it = pList.cbegin(); it != ite; ++it) {
		opdi::Port* port = *it;
		if (strcmp(port->getType(), OPDI_PORTTYPE_ANALOG))
			throw Poco::InvalidArgumentException(configPort + ": Error resolving port list of setting " + setting + ": The port " + port->ID() + " is not an Analog port");
		portList.push_back(dynamic_cast<AnalogPort*>(port));
	}
}

opdi::SelectPort* OPDI::findSelectPort(const std::string& configPort, const std::string& setting, const std::string& portID, bool required) {
	// locate port by ID
	opdi::Port* port = this->findPortByID(portID.c_str());
	// no found but required?
	if (port == nullptr) {
		if (required)
			throw Poco::DataException(configPort + ": Port required by setting " + setting + " not found: " + portID);
		return nullptr;
	}

	// port type must be "select"
	if (port->getType()[0] != OPDI_PORTTYPE_SELECT[0])
		throw Poco::DataException(configPort + ": Port specified in setting " + setting + " is not a select port: " + portID);

	return (opdi::SelectPort*)port;
}

LogVerbosity OPDI::getLogVerbosity(void)
{
	return this->logVerbosity;
}

void OPDI::logWarning(const std::string& message) {
	if (this->logVerbosity >= LogVerbosity::NORMAL) {
		this->logWarn(message);
	}
}

void OPDI::logError(const std::string& message) {
	this->logErr(message);
}

void OPDI::logNormal(const std::string& message, LogVerbosity verbosity) {
	// supplied log verbosity takes precedence
	LogVerbosity lv = (verbosity != LogVerbosity::UNKNOWN ? verbosity : this->logVerbosity);
	if (lv >= LogVerbosity::NORMAL) {
		this->log(message);
	}
}

void OPDI::logVerbose(const std::string& message, LogVerbosity verbosity) {
	// supplied log verbosity takes precedence
	LogVerbosity lv = (verbosity != LogVerbosity::UNKNOWN ? verbosity : this->logVerbosity);
	if (lv >= LogVerbosity::VERBOSE) {
		this->log(message);
	}
}

void OPDI::logDebug(const std::string& message, LogVerbosity verbosity) {
	// supplied log verbosity takes precedence
	LogVerbosity lv = (verbosity != LogVerbosity::UNKNOWN ? verbosity : this->logVerbosity);
	if (lv >= LogVerbosity::DEBUG) {
		this->log(message);
	}
}

void OPDI::logExtreme(const std::string& message, LogVerbosity verbosity) {
	// supplied log verbosity takes precedence
	LogVerbosity lv = (verbosity != LogVerbosity::UNKNOWN ? verbosity : this->logVerbosity);
	if (lv >= LogVerbosity::EXTREME) {
		this->log(message);
	}
}

double OPDI::getPortValue(opdi::Port* port) const {
	double value = 0;

	// evaluation depends on port type
	if (port->getType()[0] == OPDI_PORTTYPE_DIGITAL[0]) {
		// digital port: Low = 0; High = 1
		uint8_t mode;
		uint8_t line;
		((opdi::DigitalPort*)port)->getState(&mode, &line);
		value = line;
	}
	else
		if (port->getType()[0] == OPDI_PORTTYPE_ANALOG[0]) {
			// analog port: relative value (0..1)
			value = ((opdi::AnalogPort*)port)->getRelativeValue();
		}
		else
			if (port->getType()[0] == OPDI_PORTTYPE_DIAL[0]) {
				// dial port: absolute value
				int64_t position;
				((opdi::DialPort*)port)->getState(&position);
				value = (double)position;
			}
			else
				if (port->getType()[0] == OPDI_PORTTYPE_SELECT[0]) {
					// select port: current position number
					uint16_t position;
					((opdi::SelectPort*)port)->getState(&position);
					value = position;
				}
				else
					// port type not supported
					throw Poco::Exception("Port type not supported");

	return value;
}

}		// namespace opdi