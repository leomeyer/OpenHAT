
// OpenHAT plugin that supports 433 MHz radio controlled power sockets on Raspberry Pi

#include "Poco/Tuple.h"
#include "Poco/RegularExpression.h"

#include "opdi_constants.h"
#include "opdi_platformfuncs.h"

#include "wiringPi.h"
#include "RCSwitch.h"

#include "../rpi.h"

#include "LinuxOpenHAT.h"
#include "OpenHAT_PortFunctions.h"

///////////////////////////////////////////////////////////////////////////////
// RemoteSwitch: Plugin for remote power outlet control
///////////////////////////////////////////////////////////////////////////////

class RemoteSwitchPlugin : public IOpenHATPlugin, public IOpenHATConnectionListener {

protected:
	AbstractOpenHAT* openhat;
	std::string nodeID;

	int gpioPin;

	RCSwitch rcSwitch;

public:
	virtual void setupPlugin(AbstractOpenHAT* openhat, std::string node, Poco::Util::AbstractConfiguration* nodeConfig);

	virtual void masterConnected(void) override;
	virtual void masterDisconnected(void) override;
};

///////////////////////////////////////////////////////////////////////////////
// RemoteSwitchPort: Represents a remote-controlled power outlet
// The RemoteSwitchPort is a SelectPort that will never report its state.
// This is because radio power outlets do not have a way to query their state;
// thus we don't know what's really going on with the device.
///////////////////////////////////////////////////////////////////////////////

class RemoteSwitchPort : public OPDI_SelectPort, protected OpenHAT_PortFunctions {
protected:
	RCSwitch* rcSwitch;

	std::string systemCode;
	int unitCode;

public:
	RemoteSwitchPort(AbstractOpenHAT* openhat, const char* ID, RCSwitch* rcSwitch, std::string systemCode, int unitCode);
	virtual ~RemoteSwitchPort(void);
	virtual void setPosition(uint16_t position) override;
	virtual void getState(uint16_t* position) override;
};

RemoteSwitchPort::RemoteSwitchPort(AbstractOpenHAT* openhat, const char* ID, RCSwitch* rcSwitch, std::string systemCode, int unitCode) : OPDI_SelectPort(ID) {
	this->openhat = openhat;
	this->rcSwitch = rcSwitch;
	this->systemCode = systemCode;
	this->unitCode = unitCode;
	this->setLabel((this->ID() + "@" + systemCode + "/" + to_string(unitCode)).c_str());

	openhat->logVerbose("Setup complete: RemoteSwitchPort " + to_string(this->getLabel()));
}

RemoteSwitchPort::~RemoteSwitchPort(void) {
	// release resources; configure as floating input
}

void RemoteSwitchPort::setPosition(uint16_t position)  {
	OPDI_SelectPort::setPosition(position);

	if (position == 0) {
		this->logDebug(this->ID() + ": Switching off");
		this->rcSwitch->switchOff((char*)this->systemCode.c_str(), this->unitCode);
	} else {
		this->logDebug(this->ID() + ": Switching on");
		this->rcSwitch->switchOn((char*)this->systemCode.c_str(), this->unitCode);
	}
}

void RemoteSwitchPort::getState(uint16_t* position) {
	// the position is always unknown
	throw AccessDenied("Cannot read a RemoteSwitch");
}


///////////////////////////////////////////////////////////////////////////////
// RemoteSwitchPlugin
///////////////////////////////////////////////////////////////////////////////

void RemoteSwitchPlugin::setupPlugin(AbstractOpenHAT* openhat, std::string node, Poco::Util::AbstractConfiguration* config) {
	this->openhat = openhat;
	this->nodeID = node;

	Poco::Util::AbstractConfiguration* nodeConfig = config->createView(node);

	int pin = nodeConfig->getInt("Pin", -1);
	if (pin < 0)
		throw Poco::DataException("You have to specify a pin for the 433 MHz remote control module data line");
	// TODO validate pin
	this->gpioPin = pin;

	this->openhat->lockResource(RPI_GPIO_PREFIX + this->openhat->to_string(this->gpioPin), node);

	// setup wiringPi
	if (wiringPiSetup() == -1)
		throw Poco::ApplicationException("Unable to initialize wiringPi");

	this->rcSwitch.setPulseLength(300);
	this->rcSwitch.enableTransmit(this->gpioPin);

	// the remote switch plugin node expects a list of node names that determine the ports that this plugin provides

	// enumerate keys of the plugin's nodes (in specified order)
	this->openhat->logVerbose("Enumerating RemoteSwitch nodes: " + node + ".Nodes");

	Poco::Util::AbstractConfiguration* nodes = config->createView(node + ".Nodes");

	// store main node's group (will become the default of ports)
	std::string group = nodeConfig->getString("Group", "");

	// get ordered list of ports
	Poco::Util::AbstractConfiguration::Keys portKeys;
	nodes->keys("", portKeys);

	typedef Poco::Tuple<int, std::string> Item;
	typedef std::vector<Item> ItemList;
	ItemList orderedItems;

	// create ordered list of port keys (by priority)
	for (Poco::Util::AbstractConfiguration::Keys::const_iterator it = portKeys.begin(); it != portKeys.end(); ++it) {
		int itemNumber = nodes->getInt(*it, 0);
		// check whether the item is active
		if (itemNumber < 0)
			continue;

		// insert at the correct position to create a sorted list of items
		ItemList::iterator nli = orderedItems.begin();
		while (nli != orderedItems.end()) {
			if (nli->get<0>() > itemNumber)
				break;
			nli++;
		}
		Item item(itemNumber, *it);
		orderedItems.insert(nli, item);
	}

	if (orderedItems.size() == 0) {
		this->openhat->logNormal("Warning: No ports configured in node " + node + ".Nodes; is this intended?");
	}

	// go through items, create ports in specified order
	ItemList::const_iterator nli = orderedItems.begin();
	while (nli != orderedItems.end()) {

		std::string nodeName = nli->get<1>();

		this->openhat->logVerbose("Setting up RemoteSwitchPlugin port for node: " + nodeName);

		// get port section from the configuration
		Poco::Util::AbstractConfiguration* portConfig = config->createView(nodeName);

		// get port type (required)
		std::string portType = openhat->getConfigString(portConfig, "Type", "", true);

		if (portType == "RemoteSwitch") {
			// read system code (DIP switch)
			std::string systemCode = openhat->getConfigString(portConfig, "SystemCode", "", true);
			// validate; must be a string of type xxxxx with x = [1, 0]
			Poco::RegularExpression re("^[01][01][01][01][01]$");
			if (!re.match(systemCode))
				throw Poco::DataException("The 'SystemCode' must be a string of 0's and 1's of length 5");
			// read unit code
			int unitCode = portConfig->getInt("UnitCode", -1);
			// check whether the code is valid
			if ((unitCode < 1) || (unitCode > 4))
				throw Poco::DataException("A 'UnitCode' between 1 and 4 must be specified for a RemoteSwitch port");

			// setup the port instance and add it
			RemoteSwitchPort* port = new RemoteSwitchPort(openhat, nodeName.c_str(), &this->rcSwitch, systemCode, unitCode);
			// set default group: RemoteSwitchPlugin node's group
			port->setGroup(group);
			openhat->configureSelectPort(portConfig, config, port);
			openhat->addPort(port);
		} else
			throw Poco::DataException("This plugin does not support the port type", portType);

		nli++;
	}

	this->openhat->logVerbose("RemoteSwitchPlugin setup completed successfully as node " + node);
}

void RemoteSwitchPlugin::masterConnected() {
}

void RemoteSwitchPlugin::masterDisconnected() {
}


// plugin factory function
extern "C" IOpenHATPlugin* GetPluginInstance(int majorVersion, int minorVersion, int patchVersion) {

	// check whether the version is supported
	if ((majorVersion > 0) || (minorVersion > 1))
		throw Poco::Exception("This version of the RemoteSwitchPlugin supports only OpenHAT versions up to 0.1");

	// return a new instance of this plugin
	return new RemoteSwitchPlugin();
}
