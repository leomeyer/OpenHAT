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

#define DEFAULT_QUERY_INTERVAL	30
#define DEFAULT_TIMEOUT			5
#define DEFAULT_RETRIES			3

namespace {
	
class ArducomPlugin;

class ArducomPort  {
friend class ArducomPlugin;
protected:
	ArducomPlugin *plugin;
	bool valueSet;
	uint64_t lastQueryTime;
	int32_t timeoutSeconds;
	uint32_t queryInterval;
	uint64_t timeoutCounter;
	Poco::Mutex mutex;

	int8_t readCommand;
	std::string readParameters;
	int8_t writeCommand;
	std::string writeParameters;
	bool writeReturnsValue;

public:
	std::string pid;
	
	ArducomPort(ArducomPlugin *plugin, const std::string& id) {
		this->plugin = plugin;
		this->pid = id;
		this->valueSet = false;
		this->lastQueryTime = 0;
		this->queryInterval = DEFAULT_QUERY_INTERVAL;
		this->timeoutCounter = 0;

		readCommand = -1;
		writeCommand = -1;
		writeReturnsValue = false;
	};
	
	virtual void query(ArducomMaster *arducom) = 0;
};

class DigitalArducomPort : ArducomPort, opdi::DigitalPort {
protected:

public:
	DigitalArducomPort(ArducomPlugin* plugin, const std::string& id) : ArducomPort(plugin, id), DigitalPort(id.c_str()) {
	}


};

class ActionNotification : public Poco::Notification {
public:
	typedef Poco::AutoPtr<ActionNotification> Ptr;

	enum ActionType {
		QUERY
	};

	ActionType type;
	ArducomPort* port;

	ActionNotification(ActionType type, ArducomPort* port);
};

ActionNotification::ActionNotification(ActionType type, ArducomPort* port) {
	this->type = type;
	this->port = port;
}

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
	
	ArducomMasterTransport *transport;
	ArducomMaster *arducom;
	
	bool terminateRequested;
	
	void errorOccurred(const std::string& message);

public:
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

/*
class EventPort : public opdi::DigitalPort, public ArducomPort {
	friend class ArducomPlugin;
protected:
	
	class Output {
	public:
		std::string name;
		std::string pattern;
		std::string value;
		std::string outputPortStr;
		opdi::PortList outputPorts;
		openhat::ExpressionPort exprPort;
		
		Output(openhat::AbstractOpenHAT* openhat, std::string name): exprPort(openhat, (name + "_Expr").c_str()) {
		}
		
		uint8_t process(EventPort* myPort, std::string aValue) {
			std::string newValue = aValue;
			try {
				// pattern specified?
				if (pattern != "") {
					Poco::RegularExpression re(pattern);
					if (!re.match(newValue)) {
						myPort->plugin->openhat->logDebug(myPort->ID() + 
							": Event value '" + newValue + "', did not match the pattern '" + pattern + "'", myPort->logVerbosity);
						return OPDI_STATUS_OK;
					}
					// perform substitution
					re.subst(newValue, value);
				} else
					// use specified value directly
					newValue = value;
				
				exprPort.expressionStr = newValue;
				exprPort.prepare();
				exprPort.apply();

			} catch (const Poco::Exception& e) {
				myPort->plugin->openhat->logError(myPort->ID() + ": Error processing event value '" + value + "', evaluated to '" + newValue + "': " + e.message());
			}
			return OPDI_STATUS_OK;
		}
	};
	
	std::vector<Output*> outputs;
	
	std::string value;

public:
	EventPort(ArducomPlugin* plugin, const std::string& id, const std::string& topic);
	
	virtual void subscribe(mqtt::async_client *client) override;
	
	virtual void query(mqtt::async_client *client) override;
	
	virtual void handle_payload(std::string payload) override;

	virtual uint8_t doWork(uint8_t canSend) override;
	
	virtual void configure(openhat::ConfigurationView::Ptr config);

	virtual void prepare() override;
};


class HG06337Switch : public opdi::DigitalPort, public ArducomPort {
	friend class ArducomPlugin;
protected:
	int8_t switchState;
	int8_t initialLine;
	virtual void setSwitchState(int8_t line);

public:
	HG06337Switch(ArducomPlugin* plugin, const std::string& id, const std::string& topic);
	
	virtual void subscribe(mqtt::async_client *client) override;
	
	virtual void query(mqtt::async_client *client) override;
	
	virtual void handle_payload(std::string payload) override;

	virtual void setLine(uint8_t line, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;

	virtual uint8_t doWork(uint8_t canSend) override;
};


class TasmotaSwitch : public opdi::DigitalPort, public ArducomPort {
	friend class ArducomPlugin;
protected:
        std::string deviceID;
	int8_t switchState;
	int8_t initialLine;
	virtual void setSwitchState(int8_t line);

public:
	TasmotaSwitch(ArducomPlugin* plugin, const std::string& id, const std::string& deviceID);
	
	virtual void subscribe(mqtt::async_client *client) override;
	
	virtual void query(mqtt::async_client *client) override;
	
	virtual void handle_payload(std::string payload) override;

	virtual void setLine(uint8_t line, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;

	virtual uint8_t doWork(uint8_t canSend) override;
};

class TasmotaPower : public opdi::DialPort, public ArducomPort {
	friend class ArducomPlugin;
protected:
        std::string deviceID;
	int64_t newValue;
	virtual void setValue(int64_t value);

public:
	TasmotaPower(ArducomPlugin* plugin, const std::string& id, const std::string& deviceID);
	
	virtual void subscribe(mqtt::async_client *client) override;
	
	virtual void query(mqtt::async_client *client) override;
	
	virtual void handle_payload(std::string payload) override;

	virtual uint8_t doWork(uint8_t canSend) override;
};
*/

}	// end anonymous namespace

/*
GenericPort::GenericPort(ArducomPlugin* plugin, const std::string& id, const std::string& topic) :
	opdi::CustomPort(id, OPDI_CUSTOM_PORT_TYPEGUID), ArducomPort(plugin, id, topic) {
	this->myValue = "";
	this->valueSet = false;
}

void GenericPort::query(mqtt::async_client *client) {
	this->plugin->openhat->logDebug(this->pid + ": Querying state", this->logVerbosity);
	if (client->is_connected())
		try {
			client->publish(this->topic + "/get", "{\"state\":\"\"}");
		}  catch (mqtt::exception& ex) {
			this->plugin->openhat->logError(std::string(this->getID()) + ": Error querying state: " + this->topic + ": " + ex.what());
		}
	}

void GenericPort::handle_payload(std::string payload) {
	this->plugin->openhat->logDebug(this->pid + ": Payload received: '" + payload + "'");

	Poco::Mutex::ScopedLock lock(this->mutex);
	this->myValue = payload;
	this->valueSet = true;

	// reset query timer
	this->lastQueryTime = opdi_get_time_ms();
	// reset timeout counter
	this->timeoutCounter = opdi_get_time_ms();
}

uint8_t GenericPort::doWork(uint8_t canSend) {
	opdi::CustomPort::doWork(canSend);
	
	this->logExtreme("Value: " + this->value);
	
	// time for refresh?
	if ((this->queryInterval > 0) && (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000)) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::QUERY, this));
		this->lastQueryTime = opdi_get_time_ms();
	}
	
	Poco::Mutex::ScopedLock lock(this->mutex);
	// no error?
	if (this->myValue != "") {
		// error timeout reached without message?
		if ((opdi_get_time_ms() - this->timeoutCounter) / 1000 > this->timeoutSeconds) {
			// change to error state
			this->myValue = "";
			this->setError(Error::VALUE_NOT_AVAILABLE);
		}
	}

	// has a value been returned?
	if (this->valueSet) {
		// empty values signify an error
		if (this->myValue != "")
			// use base method to avoid triggering an action!
			opdi::CustomPort::setValue(this->myValue);
		else
			this->setError(Error::VALUE_NOT_AVAILABLE);
		this->valueSet = false;
	}

	return OPDI_STATUS_OK;
}

void GenericPort::setValue(const std::string& newValue, ChangeSource changeSource) {
	// opdi::CustomPort::setValue(newValue, changeSource);
	
	try {
		if (this->plugin->client != nullptr && this->plugin->client->is_connected()) {
			this->plugin->client->publish(this->topic + "/set", newValue);
			this->plugin->openhat->logDebug(std::string(this->getID()) + ": Published value to: " + this->topic + ": " + newValue);
		}
	}  catch (mqtt::exception& ex) {
		this->plugin->openhat->logError(std::string(this->getID()) + ": Error publishing state: " + this->topic + ": " + ex.what());
	}
}

void GenericPort::configure(Poco::Util::AbstractConfiguration::Ptr config) {
	CustomPort::configure(config);
	this->initialValue = config->getString("Value", "");
}


EventPort::EventPort(ArducomPlugin* plugin, const std::string& id, const std::string& topic) : 
	opdi::DigitalPort(id.c_str(), OPDI_PORTDIRCAP_OUTPUT, 0), ArducomPort(plugin, id, topic) {
	// events are enabled by default
	this->line = 1;
	this->mode = OPDI_DIGITAL_MODE_OUTPUT;
}

void EventPort::subscribe(mqtt::async_client *client) {
	if (client->is_connected())
		try {
			this->plugin->openhat->logDebug(std::string(this->getID()) + ": Subscribing to topic: " + this->topic, this->logVerbosity);
			client->subscribe(this->topic, this->plugin->QOS, nullptr, this->listener);
		}  catch (mqtt::exception& ex) {
			this->plugin->openhat->logError(std::string(this->getID()) + ": Error subscribing to topic " + this->topic + ": " + ex.what());
		}
}

void EventPort::query(mqtt::async_client *client) {
}

void EventPort::handle_payload(std::string payload) {
	this->plugin->openhat->logDebug(this->pid + ": Payload received: '" + payload + "'", this->logVerbosity);
	
	Poco::Mutex::ScopedLock lock(this->mutex);
	this->value = payload;
	this->valueSet = true;
}

uint8_t EventPort::doWork(uint8_t canSend) {
	opdi::DigitalPort::doWork(canSend);

	// must be active to react to events
	if (this->getLine() != 1) {
		this->valueSet = false;
		return OPDI_STATUS_OK;
	}
	
	// value received?
	if (this->valueSet) {
		// go through outputs, let them process the value
		auto it = this->outputs.begin();
		auto ite = this->outputs.end();
		while (it != ite) {
			uint8_t result = (*it)->process(this, this->value);
			if (result != OPDI_STATUS_OK)
				return result;
			++it;
		}
		
		this->valueSet = false;
	}
	
	return OPDI_STATUS_OK;
}

void EventPort::configure(openhat::ConfigurationView::Ptr config) {
	this->plugin->openhat->configureDigitalPort(config, this, false);
	// enumerate outputs (in specified order)
	this->plugin->openhat->logVerbose(this->ID() + ": Enumerating EventPort outputs: " + this->ID() + ".Outputs", this->logVerbosity);

	Poco::AutoPtr<openhat::ConfigurationView> nodes = this->plugin->openhat->createConfigView(config, "Outputs");
	config->addUsedKey("Outputs");

	// get ordered list of outputs
	openhat::ConfigurationView::Keys outputKeys;
	nodes->keys("", outputKeys);

	typedef Poco::Tuple<int, std::string> Item;
	typedef std::vector<Item> ItemList;
	ItemList orderedItems;

	// create ordered list of port keys (by priority)
	for (auto it = outputKeys.begin(), ite = outputKeys.end(); it != ite; ++it) {

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
		this->plugin->openhat->logWarning("No outputs configured in " + this->ID() + ".Outputs; is this intended?");
	}

	// go through items, create outputs in specified order
	auto nli = orderedItems.begin();
	auto nlie = orderedItems.end();
	while (nli != nlie) {
		std::string outputName = nli->get<1>();

		this->plugin->openhat->logVerbose(this->ID() + ": Setting up EventPort output: " + outputName, this->logVerbosity);

		// get device section from the configuration>
		Poco::AutoPtr<openhat::ConfigurationView> outputConfig = this->plugin->openhat->createConfigView(config, outputName);

		Output* output = new Output(this->plugin->openhat, outputName);
		// get output ports (required)
		output->outputPortStr = this->plugin->openhat->getConfigString(outputConfig, outputName, "Ports", "", true);
		output->pattern = this->plugin->openhat->getConfigString(outputConfig, outputName, "Pattern", "", false);
		output->value =  this->plugin->openhat->getConfigString(outputConfig, outputName, "Value", "", false);

		// test pattern if specified
		if (output->pattern != "") {
			Poco::RegularExpression re(output->pattern);
		}
		
		this->outputs.push_back(output);
		
		++nli;
	}
}

void EventPort::prepare() {
	// go through outputs, resolve ports
	auto it = this->outputs.begin();
	auto ite = this->outputs.end();
	while (it != ite) {
		// will be mapped to ports in exprPort.prepare() when the value arrives
		(*it)->exprPort.outputPortStr = (*it)->outputPortStr;
//		this->plugin->openhat->findPorts((*it)->name, "Ports", (*it)->outputPortStr, (*it)->exprPort.outputPorts);
		++it;
	}
}

HG06337Switch::HG06337Switch(ArducomPlugin* plugin, const std::string& id, const std::string& topic) : 
	opdi::DigitalPort(id.c_str(), OPDI_PORTDIRCAP_OUTPUT, 0), ArducomPort(plugin, id, topic) {
	this->switchState = -1;	// unknown
	this->valueSet = true;		// causes setError in doWork
	this->initialLine = -1;
	
	this->mode = OPDI_DIGITAL_MODE_OUTPUT;
	this->setIcon("powersocket");
}

void HG06337Switch::subscribe(mqtt::async_client *client) {
	if (client->is_connected())
		try {
			this->plugin->openhat->logDebug(std::string(this->getID()) + ": Subscribing to topic: " + this->topic, this->logVerbosity);
			client->subscribe(this->topic, this->plugin->QOS, nullptr, this->listener);
			
			if (this->initialLine > -1) {
				this->setLine(this->initialLine);
				this->initialLine = -1;
			}
		}  catch (mqtt::exception& ex) {
			this->plugin->openhat->logError(std::string(this->getID()) + ": Error subscribing to topic " + this->topic + ": " + ex.what());
		}
}

void HG06337Switch::query(mqtt::async_client *client) {
	this->plugin->openhat->logDebug(this->pid + ": Querying state", this->logVerbosity);
	if (client->is_connected())
		try {
			client->publish(this->topic + "/get", "{\"state\":\"\"}");
		}  catch (mqtt::exception& ex) {
			this->plugin->openhat->logError(std::string(this->getID()) + ": Error querying state: " + this->topic + ": " + ex.what());
		}
	}

void HG06337Switch::handle_payload(std::string payload) {
	this->plugin->openhat->logDebug(this->pid + ": Payload received: '" + payload + "'");
	if (payload.find("\"state\":\"OFF\"") != std::string::npos) {
		this->setSwitchState(0);
		this->valueSet = true;
	} else
	if (payload.find("\"state\":\"ON\"") != std::string::npos) {
		this->setSwitchState(1);
		this->valueSet = true;
	} else {
		// error
		this->setSwitchState(-1);
		this->valueSet = true;
	}
	// reset query timer
	this->lastQueryTime = opdi_get_time_ms();
	// reset timeout counter
	this->timeoutCounter = opdi_get_time_ms();
}

uint8_t HG06337Switch::doWork(uint8_t canSend) {
	opdi::DigitalPort::doWork(canSend);

	// time for refresh?
	if ((this->queryInterval > 0) && (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000)) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::QUERY, this));
		this->lastQueryTime = opdi_get_time_ms();
	}
	
	Poco::Mutex::ScopedLock lock(this->mutex);
	// no error?
	if (this->switchState > -1) {
		// error timeout reached without message?
		if ((opdi_get_time_ms() - this->timeoutCounter) / 1000 > this->timeoutSeconds) {
			// change to error state
			this->switchState = -1;
			this->setError(Error::VALUE_NOT_AVAILABLE);
		}
	}

	// has a value been returned?
	if (this->valueSet) {
		// values < 0 signify an error
		if (this->switchState > -1)
			// use base method to avoid triggering an action!
			opdi::DigitalPort::setLine(this->switchState);
		else
			this->setError(Error::VALUE_NOT_AVAILABLE);
		this->valueSet = false;
	}

	return OPDI_STATUS_OK;
}

void HG06337Switch::setLine(uint8_t line, ChangeSource changeSource) {
	// opdi::DigitalPort::setLine(line, changeSource);

	if (this->plugin->client == nullptr)
		return;
	try {
		std::string payload;
		if (line == 0)
			payload = "{\"state\":\"OFF\"}";
		else
		if (line == 1)
			payload = "{\"state\":\"ON\"}";

		if (payload != "") {
			this->plugin->client->publish(this->topic + "/set", payload);
			this->plugin->openhat->logDebug(std::string(this->getID()) + ": Published value to: " + this->topic + ": " + payload);
		} else
			this->plugin->openhat->logDebug(std::string(this->getID()) + ": No payload to publish, line is " + this->plugin->openhat->to_string(line));
		
	}  catch (mqtt::exception& ex) {
		this->plugin->openhat->logError(std::string(this->getID()) + ": Error publishing state: " + this->topic + ": " + ex.what());
	}
}

void HG06337Switch::setSwitchState(int8_t switchState) {
	Poco::Mutex::ScopedLock lock(this->mutex);

	this->switchState = switchState;
	this->valueSet = true;
}

TasmotaSwitch::TasmotaSwitch(ArducomPlugin* plugin, const std::string& id, const std::string& deviceID) : 
	opdi::DigitalPort(id.c_str(), OPDI_PORTDIRCAP_OUTPUT, 0), ArducomPort(plugin, id, std::string("stat/") + deviceID + "/POWER") {
    this->deviceID = deviceID;
    this->switchState = -1;	// unknown
    this->valueSet = true;		// causes setError in doWork
    this->initialLine = -1;
	
    this->mode = OPDI_DIGITAL_MODE_OUTPUT;
    this->setIcon("powersocket");
}

void TasmotaSwitch::subscribe(mqtt::async_client *client) {
	if (client->is_connected())
		try {
			this->plugin->openhat->logDebug(std::string(this->getID()) + ": Subscribing to topic: " + this->topic, this->logVerbosity);
			client->subscribe(this->topic, this->plugin->QOS, nullptr, this->listener);
			
			if (this->initialLine > -1) {
				this->setLine(this->initialLine);
				this->initialLine = -1;
			}
		}  catch (mqtt::exception& ex) {
			this->plugin->openhat->logError(std::string(this->getID()) + ": Error subscribing to topic " + this->topic + ": " + ex.what());
		}
}

void TasmotaSwitch::query(mqtt::async_client *client) {
	this->plugin->openhat->logDebug(this->pid + ": Querying state", this->logVerbosity);
	if (client->is_connected())
		try {
			client->publish(std::string("cmnd/") + this->deviceID + "/power", std::string());
		}  catch (mqtt::exception& ex) {
			this->plugin->openhat->logError(std::string(this->getID()) + ": Error querying state: " + this->topic + ": " + ex.what());
		}
	}

void TasmotaSwitch::handle_payload(std::string payload) {
	this->plugin->openhat->logDebug(this->pid + ": Payload received: '" + payload + "'");
	if (payload.find("OFF") != std::string::npos) {
		this->setSwitchState(0);
		this->valueSet = true;
	} else
	if (payload.find("ON") != std::string::npos) {
		this->setSwitchState(1);
		this->valueSet = true;
	} else {
		// error
		this->setSwitchState(-1);
		this->valueSet = true;
	}
	// reset query timer
	this->lastQueryTime = opdi_get_time_ms();
	// reset timeout counter
	this->timeoutCounter = opdi_get_time_ms();
}

uint8_t TasmotaSwitch::doWork(uint8_t canSend) {
	opdi::DigitalPort::doWork(canSend);

	// time for refresh?
	if ((this->queryInterval > 0) && (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000)) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::QUERY, this));
		this->lastQueryTime = opdi_get_time_ms();
	}
	
	Poco::Mutex::ScopedLock lock(this->mutex);
	// no error?
	if (this->switchState > -1) {
		// error timeout reached without message?
		if ((opdi_get_time_ms() - this->timeoutCounter) / 1000 > this->timeoutSeconds) {
			// change to error state
			this->switchState = -1;
			this->setError(Error::VALUE_NOT_AVAILABLE);
		}
	}

	// has a value been returned?
	if (this->valueSet) {
		// values < 0 signify an error
		if (this->switchState > -1)
			// use base method to avoid triggering an action!
			opdi::DigitalPort::setLine(this->switchState);
		else
			this->setError(Error::VALUE_NOT_AVAILABLE);
		this->valueSet = false;
	}

	return OPDI_STATUS_OK;
}

void TasmotaSwitch::setLine(uint8_t line, ChangeSource changeSource) {
	// opdi::DigitalPort::setLine(line, changeSource);

	if (this->plugin->client == nullptr)
		return;
	try {
		std::string payload;
		if (line == 0)
			payload = "OFF";
		else
		if (line == 1)
			payload = "ON";

		if (payload != "") {
                    std::string topic = "cmnd/" + this->deviceID + "/power";
                    this->plugin->client->publish(topic, payload);
                    this->plugin->openhat->logDebug(std::string(this->getID()) + ": Published value to: " + topic + ": " + payload);
		} else
                    this->plugin->openhat->logDebug(std::string(this->getID()) + ": No payload to publish, line is " + this->plugin->openhat->to_string(line));
		
	}  catch (mqtt::exception& ex) {
		this->plugin->openhat->logError(std::string(this->getID()) + ": Error publishing state: " + topic + ": " + ex.what());
	}
}

void TasmotaSwitch::setSwitchState(int8_t switchState) {
	Poco::Mutex::ScopedLock lock(this->mutex);

	this->switchState = switchState;
	this->valueSet = true;
}


TasmotaPower::TasmotaPower(ArducomPlugin* plugin, const std::string& id, const std::string& deviceID) : 
	opdi::DialPort(id.c_str(), 0, 999999999999, 1), ArducomPort(plugin, id, std::string("stat/") + deviceID + "/STATUS8") {
    this->deviceID = deviceID;
    this->newValue = -1;
    this->valueSet = true;		// causes setError in doWork
	this->readonly = true;
	
    this->setIcon("energymeter");
    this->setUnit("electricPower_mW");
}

void TasmotaPower::subscribe(mqtt::async_client *client) {
    if (client->is_connected())
        try {
            this->plugin->openhat->logDebug(std::string(this->getID()) + ": Subscribing to topic: " + this->topic, this->logVerbosity);
            client->subscribe(this->topic, this->plugin->QOS, nullptr, this->listener);
        }  catch (mqtt::exception& ex) {
                this->plugin->openhat->logError(std::string(this->getID()) + ": Error subscribing to topic " + this->topic + ": " + ex.what());
        }
}

void TasmotaPower::query(mqtt::async_client *client) {
    this->plugin->openhat->logDebug(this->pid + ": Querying state", this->logVerbosity);
    if (client->is_connected())
        try {
            std::string topic = std::string("cmnd/") + this->deviceID + "/status";
            client->publish(topic, std::string("8"));
        }  catch (mqtt::exception& ex) {
            this->plugin->openhat->logError(std::string(this->getID()) + ": Error querying state: " + topic + ": " + ex.what());
        }
}

void TasmotaPower::handle_payload(std::string payload) {
    this->plugin->openhat->logDebug(this->pid + ": Payload received: '" + payload + "'");

    if (payload.empty()) {
        this->setValue(-1);
        return;
    }

    // parse result JSON
    try {
            Poco::JSON::Parser parser;
            Poco::Dynamic::Var loParsedJson = parser.parse(payload);
            Poco::Dynamic::Var jsonResult = parser.result();

            Poco::JSON::Object::Ptr root = jsonResult.extract<Poco::JSON::Object::Ptr>();
            Poco::JSON::Object::Ptr body = this->plugin->GetObject(root, "StatusSNS");
            Poco::JSON::Object::Ptr data = this->plugin->GetObject(body, "ENERGY");
            std::string value = this->plugin->GetString(data, "Power");
            // parse value
            int power = -1;
            Poco::NumberParser::tryParse(value, power);
            this->setValue(power * 1000);	// milliwatts
    } catch (Poco::Exception &e) {
            this->plugin->errorOccurred(this->ID() + ": Error parsing JSON: " + this->plugin->openhat->getExceptionMessage(e));
            this->setValue(-1);
    }

    // reset query timer
    this->lastQueryTime = opdi_get_time_ms();
    // reset timeout counter
    this->timeoutCounter = opdi_get_time_ms();
}

uint8_t TasmotaPower::doWork(uint8_t canSend) {
    opdi::DialPort::doWork(canSend);

    // time for refresh?
    if ((this->queryInterval > 0) && (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000)) {
        this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::QUERY, this));
        this->lastQueryTime = opdi_get_time_ms();
    }

    Poco::Mutex::ScopedLock lock(this->mutex);
    // no error?
    if (this->newValue > -1) {
        // error timeout reached without message?
        if ((opdi_get_time_ms() - this->timeoutCounter) / 1000 > this->timeoutSeconds) {
            // change to error state
            this->newValue = -1;
            this->setError(Error::VALUE_NOT_AVAILABLE);
        }
    }

    // has a value been returned?
    if (this->valueSet) {
        // values < 0 signify an error
        if (this->newValue > -1)
                // use base method to avoid triggering an action!
                opdi::DialPort::setPosition(this->newValue);
        else
                this->setError(Error::VALUE_NOT_AVAILABLE);
        this->valueSet = false;
    }

    return OPDI_STATUS_OK;
}

void TasmotaPower::setValue(int64_t value) {
	Poco::Mutex::ScopedLock lock(this->mutex);

	this->newValue = value;
	this->valueSet = true;
}


*/

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

	// get Arducom connection details
	this->type = this->openhat->getConfigString(nodeConfig, node, "Type", "", true);
	this->device = this->openhat->getConfigString(nodeConfig, node, "Device", "", true);
	this->address = nodeConfig->getInt("Address");
	this->baudrate = nodeConfig->getInt("Baudrate", ARDUCOM_DEFAULT_BAUDRATE);
	this->retries = nodeConfig->getInt("Retries", DEFAULT_RETRIES);
	this->initDelay = nodeConfig->getInt("InitDelay", 0);
	this->timeoutSeconds = nodeConfig->getInt("Timeout", this->timeoutSeconds);

	// initialize the parameters
	ArducomBaseParameters params;
	params.transportType = type;
	
	this->arducom = nullptr;
	this->terminateRequested = false;

	// store main node's group (will become the default of ports)
	std::string group = nodeConfig->getString("Group", "");

	// store main node's verbosity level (will become the default of ports)
	this->logVerbosity = this->openhat->getConfigLogVerbosity(nodeConfig, this->openhat->getLogVerbosity());

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

		// get device section from the configuration>
		Poco::AutoPtr<openhat::ConfigurationView> deviceConfig = this->openhat->createConfigView(parentConfig, portName);

		// get device type (required)
		std::string deviceType = this->openhat->getConfigString(deviceConfig, portName, "Type", "", true);

		int timeout = deviceConfig->getInt("Timeout", this->timeoutSeconds);

		int queryInterval = deviceConfig->getInt("QueryInterval", DEFAULT_QUERY_INTERVAL);
		if (queryInterval < 0)
			this->openhat->throwSettingException(portName + ": Please specify a QueryInterval >= 0: " + this->openhat->to_string(queryInterval));
/*
		if (deviceType == "Generic") {
			this->openhat->logVerbose(node + ": Setting up generic Arducom device port: " + portName, this->logVerbosity);
                        // get topic (required)
                        std::string topic = this->openhat->getConfigString(deviceConfig, deviceName, "Topic", "", true);

			// setup the generic port instance and add it
			GenericPort* genericPort = new GenericPort(this, portName, topic);
			genericPort->timeoutSeconds = timeout;
			genericPort->queryInterval = queryInterval;
			// set default group: Arducom's node's group
			genericPort->setGroup(group);
			// set default log verbosity
			genericPort->logVerbosity = this->logVerbosity;
			this->openhat->configureCustomPort(deviceConfig, genericPort);
			this->openhat->addPort(genericPort);
			this->myPorts.push_back(genericPort);
		}
		else if (deviceType == "Event") {
			this->openhat->logVerbose(node + ": Setting up Arducom event port: " + portName, this->logVerbosity);
                        // get topic (required)
                        std::string topic = this->openhat->getConfigString(deviceConfig, deviceName, "Topic", "", true);

			// setup the generic port instance and add it
			EventPort* eventPort = new EventPort(this, deviceName, topic);
			eventPort->timeoutSeconds = timeout;
			eventPort->queryInterval = queryInterval;
			// set default group: Arducom's node's group
			eventPort->setGroup(group);
			// set default log verbosity
			eventPort->logVerbosity = this->logVerbosity;
			eventPort->configure(deviceConfig);
			this->openhat->addPort(eventPort);
			this->myPorts.push_back(eventPort);
		}
		else if (deviceType == "HG06337") {
			this->openhat->logVerbose(node + ": Setting up HG06337 device port: " + deviceName + "_Switch", this->logVerbosity);
                        // get topic (required)
                        std::string topic = this->openhat->getConfigString(deviceConfig, deviceName, "Topic", "", true);

			if (parentConfig->hasProperty(deviceName + "_Switch.Type")) {
				this->openhat->logVerbose(node + ": Setting up HG06337 device port: " + deviceName + "_Switch", this->logVerbosity);
				Poco::AutoPtr<openhat::ConfigurationView> portConfig = this->openhat->createConfigView(parentConfig, deviceName + "_Switch");
				// check type
				if (portConfig->getString("Type") != "DigitalPort")
					this->openhat->throwSettingException(node + ": HG06337 Switch device port must be of type 'DigitalPort'");
				// setup the switch port instance and add it
				HG06337Switch* switchPort = new HG06337Switch(this, deviceName + "_Switch", topic);
				switchPort->timeoutSeconds = timeout;
				switchPort->queryInterval = queryInterval;
				// set default group: Arducom's node's group
				switchPort->setGroup(group);
				// set default log verbosity
				switchPort->logVerbosity = this->logVerbosity;
				std::string portLine = portConfig->getString("Line", "");
				if (portLine == "High") {
					switchPort->initialLine = 1;
				} else if (portLine == "Low") {
					switchPort->initialLine = 0;
				} else if (portLine != "")
					this->openhat->throwSettingException("Unknown Line specified; expected 'Low' or 'High'", portLine);
				// allow only basic settings to be changed
				this->openhat->configurePort(portConfig, switchPort, 0);
				this->openhat->addPort(switchPort);
				this->myPorts.push_back(switchPort);
			} else {
				this->openhat->logVerbose(node + ": HG06337 device port: " + deviceName + "_Switch.Type not found, ignoring", this->logVerbosity);
			}
		}
		else if (deviceType == "GosundTasmota") {
			this->openhat->logVerbose(node + ": Setting up GosundTasmota device port: " + deviceName + "_Switch", this->logVerbosity);
                        // get device ID (required)
                        std::string deviceID = this->openhat->getConfigString(deviceConfig, deviceName, "DeviceID", "", true);

			if (parentConfig->hasProperty(deviceName + "_Switch.Type")) {
				this->openhat->logVerbose(node + ": Setting up GosundTasmota device port: " + deviceName + "_Switch", this->logVerbosity);
				Poco::AutoPtr<openhat::ConfigurationView> portConfig = this->openhat->createConfigView(parentConfig, deviceName + "_Switch");
				// check type
				if (portConfig->getString("Type") != "DigitalPort")
					this->openhat->throwSettingException(node + ": GosundTasmota Switch device port must be of type 'DigitalPort'");
				// setup the switch port instance and add it
				TasmotaSwitch* switchPort = new TasmotaSwitch(this, deviceName + "_Switch", deviceID);
				switchPort->timeoutSeconds = timeout;
				switchPort->queryInterval = queryInterval;
				// set default group: Arducom's node's group
				switchPort->setGroup(group);
				// set default log verbosity
				switchPort->logVerbosity = this->logVerbosity;
				std::string portLine = portConfig->getString("Line", "");
				if (portLine == "High") {
					switchPort->initialLine = 1;
				} else if (portLine == "Low") {
					switchPort->initialLine = 0;
				} else if (portLine != "")
					this->openhat->throwSettingException("Unknown Line specified; expected 'Low' or 'High'", portLine);
				// allow only basic settings to be changed
				this->openhat->configurePort(portConfig, switchPort, 0);
				this->openhat->addPort(switchPort);
				this->myPorts.push_back(switchPort);
			} else {
				this->openhat->logVerbose(node + ": GosundTasmota device port: " + deviceName + "_Switch.Type not found, ignoring", this->logVerbosity);
			}
                        
			if (parentConfig->hasProperty(deviceName + "_Power.Type")) {
				this->openhat->logVerbose(node + ": Setting up GosundTasmota device port: " + deviceName + "_Power", this->logVerbosity);
				Poco::AutoPtr<openhat::ConfigurationView> portConfig = this->openhat->createConfigView(parentConfig, deviceName + "_Power");
				// check type
				if (portConfig->getString("Type") != "DialPort")
					this->openhat->throwSettingException(node + ": GosundTasmota Power device port must be of type 'DialPort'");
				// setup the switch port instance and add it
				TasmotaPower* powerPort = new TasmotaPower(this, deviceName + "_Power", deviceID);
				powerPort->timeoutSeconds = timeout;
				powerPort->queryInterval = queryInterval;
				// set default group: Arducom's node's group
				powerPort->setGroup(group);
				// set default log verbosity
				powerPort->logVerbosity = this->logVerbosity;
				// allow only basic settings to be changed
				this->openhat->configurePort(portConfig, powerPort, 0);
				this->openhat->addPort(powerPort);
				this->myPorts.push_back(powerPort);
			} else {
				this->openhat->logVerbose(node + ": GosundTasmota device port: " + deviceName + "_Power.Type not found, ignoring", this->logVerbosity);
			}
                }
                else
                    this->openhat->throwSettingException(node + ": This plugin does not support the device type '" + deviceType + "'");
*/
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
		
		this->openhat->logExtreme(this->nodeID + ": Worker thread loop...", this->logVerbosity);

		if (this->arducom == nullptr) {
			this->openhat->logDebug(this->nodeID + ": Initializing Arducom master", this->logVerbosity);
			this->arducom = new ArducomMaster(transport);
		}

		if (this->arducom != nullptr) {
			try {
				this->openhat->logExtreme(this->nodeID + ": Waiting for actions...", this->logVerbosity);
				Poco::Notification::Ptr notification = this->queue.waitDequeueNotification(100);
				if (!this->openhat->shutdownRequested && notification) {
					ActionNotification::Ptr workNf = notification.cast<ActionNotification>();
					if (workNf) {
						std::string action;
						switch (workNf->type) {
							case ActionNotification::QUERY: action = "QUERY"; break;
						}
						ArducomPort* port = (ArducomPort*)workNf->port;

						this->openhat->logDebug(this->nodeID + ": Processing requested action: " + action + " for port: " + port->pid, this->logVerbosity);
						// inspect action and decide what to do
						switch (workNf->type) {
							case ActionNotification::QUERY: {
								workNf->port->query(this->arducom);
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
