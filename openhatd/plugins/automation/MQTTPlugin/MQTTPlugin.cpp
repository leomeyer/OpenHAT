#include <sstream>

#include "Poco/Tuple.h"
#include "Poco/Runnable.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/StreamCopier.h"
#include "Poco/Path.h"
#include "Poco/URI.h"
#include "Poco/Exception.h"
#include "Poco/DOM/DOMParser.h"
#include "Poco/DOM/Document.h"
#include "Poco/DOM/NodeIterator.h"
#include "Poco/DOM/NodeFilter.h"
#include "Poco/DOM/AutoPtr.h"
#include "Poco/SAX/InputSource.h"
#include "Poco/DigestStream.h"
#include "Poco/MD5Engine.h"
#include "Poco/UTF16Encoding.h"
#include "Poco/UnicodeConverter.h"
#include "Poco/Notification.h"
#include "Poco/NotificationQueue.h"
#include "Poco/AutoPtr.h"
#include "Poco/NumberParser.h"

#include "AbstractOpenHAT.h"

#include "TypeGUIDs.h"

// requires PAHO C libraries, see https://github.com/eclipse/paho.mqtt.cpp
#include "mqtt/client.h"

namespace {
	
class action_listener : public virtual mqtt::iaction_listener
{
	std::string name_;

	void on_failure(const mqtt::token& tok) override {
	}

	void on_success(const mqtt::token& tok) override {
	}

public:
	action_listener(const std::string& name) : name_(name) {}
};

class MQTTPlugin;

class MQTTPort  {
friend class MQTTPlugin;
protected:
	MQTTPlugin *plugin;
	std::string topic;
	bool valueSet;
	uint64_t lastQueryTime;
	uint32_t queryInterval;
	uint64_t timeoutCounter;
	Poco::Mutex mutex;
	action_listener listener;
public:
	std::string pid;
	
	MQTTPort(MQTTPlugin *plugin, const std::string& id, const std::string& topic): listener(id) {
		this->plugin = plugin;
		this->pid = id;
		this->topic = topic;
		this->valueSet = false;
		this->lastQueryTime = 0;
		this->queryInterval = 10;
		this->timeoutCounter = 0;
	};
	
	virtual void subscribe(mqtt::async_client *client) = 0;
	
	virtual void query(mqtt::async_client *client) = 0;

	virtual void handle_payload(std::string payload) = 0;
};

class ActionNotification : public Poco::Notification {
public:
	typedef Poco::AutoPtr<ActionNotification> Ptr;

	enum ActionType {
		SUBSCRIBE,
		QUERY,
		RECONNECT
	};

	ActionType type;
	MQTTPort* port;

	ActionNotification(ActionType type, MQTTPort* port);
};

ActionNotification::ActionNotification(ActionType type, MQTTPort* port) {
	this->type = type;
	this->port = port;
}

////////////////////////////////////////////////////////////////////////
// Plugin main class
////////////////////////////////////////////////////////////////////////

class MQTTPlugin : public IOpenHATPlugin, public openhat::IConnectionListener, public Poco::Runnable,
	public virtual mqtt::callback, public virtual mqtt::iaction_listener		
{
	friend class HG06337Switch;
protected:
	
	class Callback: public virtual mqtt::callback, public virtual mqtt::iaction_listener {
		protected:
			MQTTPlugin *plugin;
		public:
			Callback(MQTTPlugin *plugin) {
				this->plugin = plugin;
			}
			// callback delegates implementation
			void connection_lost(const std::string& cause) override {
				this->plugin->connection_lost(cause);
			}

			void on_failure(const mqtt::token& tok) override {
				this->plugin->on_failure(tok);
			}

			// (Re)connection success
			// Either this or connected() can be used for callbacks.
			void on_success(const mqtt::token& tok) override {
				this->plugin->on_success(tok);
			}

			// (Re)connection success
			void connected(const std::string& cause) override {
				this->plugin->connected(cause);
			}

			void delivery_complete(mqtt::delivery_token_ptr tok) override {
				this->plugin->delivery_complete(tok);
			}
			
			void message_arrived(mqtt::const_message_ptr msg) override {
				this->plugin->message_arrived(msg);
			}
	};
	
	// callback implementation
	void connection_lost(const std::string& cause) override {
//		this->openhat->logError(nodeID + ": Lost connection" + (cause.size() > 0 ? ": " + cause : ""));
		queue.enqueueNotification(new ActionNotification(ActionNotification::RECONNECT, nullptr));
	}

	void on_failure(const mqtt::token& tok) override {
//		this->openhat->logError(nodeID + ": Connection attempt failed");
	}

	// (Re)connection success
	// Either this or connected() can be used for callbacks.
	void on_success(const mqtt::token& tok) override {}

	// (Re)connection success
	void connected(const std::string& cause) override {
//		this->openhat->logVerbose(nodeID + ": Connection established", this->logVerbosity);

		// clear pending queries
		this->queue.clear();
		// request subscription
		queue.enqueueNotification(new ActionNotification(ActionNotification::SUBSCRIBE, nullptr));
	}
	
	void delivery_complete(mqtt::delivery_token_ptr tok) override {
		std::cout << "\n\t[Delivery complete for token: "
			<< (tok ? tok->get_message_id() : -1) << "]" << std::endl;
	}

	void message_arrived(mqtt::const_message_ptr msg) override {
//		this->openhat->logDebug(this->nodeID + ": Message received, topic: '" + msg->get_topic() + "', payload: '" + msg->to_string() + "'", this->logVerbosity);
		
		// distribute to the correct port(s)
		auto it = this->myPorts.begin();
		auto ite = this->myPorts.end();
		bool handled = false;
		while (it != ite) {
			if ((*it)->topic == msg->get_topic()) {
				(*it)->handle_payload(msg->to_string());
				handled = true;
			}
			++it;
		}
		
//		if (!handled)
//			this->openhat->logError(this->nodeID + ": Unable to find port to handle topic " + msg->get_topic());
	}
	
	
	std::vector<MQTTPort*> myPorts;

	public:

	std::string nodeID;

	std::string host;
	int port;
	std::string user;
	std::string password;
	uint32_t timeoutSeconds;
	int QOS;

	std::string sid;
	int errorCount;
	std::string lastErrorMessage;

	opdi::LogVerbosity logVerbosity;

	Poco::NotificationQueue queue;
	Poco::Thread workThread;
	
	mqtt::async_client *client;
	
	bool terminateRequested;
	
	void publish(const std::string& topic, const std::string& payload);

	void errorOccurred(const std::string& message);

public:
	openhat::AbstractOpenHAT* openhat;

	virtual void masterConnected(void) override;
	virtual void masterDisconnected(void) override;
	
	bool is_connected(void);

	virtual void run(void);

	virtual void setupPlugin(openhat::AbstractOpenHAT* abstractOpenHAT, const std::string& node, openhat::ConfigurationView::Ptr nodeConfig, openhat::ConfigurationView::Ptr parentConfig, const std::string& driverPath) override;
	
	virtual void terminate(void) override;
};

////////////////////////////////////////////////////////////////////////
// Plugin ports
////////////////////////////////////////////////////////////////////////

class GenericPort : public opdi::CustomPort, public MQTTPort {
	friend class MQTTPlugin;
protected:
	std::string myValue;
	std::string initialValue;
public:
	GenericPort(MQTTPlugin* plugin, const std::string& id, const std::string& topic);
	
	virtual void subscribe(mqtt::async_client *client) override;
	
	virtual void query(mqtt::async_client *client) override;
	
	virtual void handle_payload(std::string payload) override;

	virtual uint8_t doWork(uint8_t canSend) override;
	
	virtual void setValue(const std::string& value, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT) override;
	
	virtual void configure(Poco::Util::AbstractConfiguration::Ptr config) override;
};


class HG06337Switch : public opdi::DigitalPort, public MQTTPort {
	friend class MQTTPlugin;
protected:
	int8_t switchState;
	int8_t initialLine;
	virtual void setSwitchState(int8_t line);

public:
	HG06337Switch(MQTTPlugin* plugin, const std::string& id, const std::string& topic);
	
	virtual void subscribe(mqtt::async_client *client) override;
	
	virtual void query(mqtt::async_client *client) override;
	
	virtual void handle_payload(std::string payload) override;

	virtual void setLine(uint8_t line, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;

	virtual uint8_t doWork(uint8_t canSend) override;
};

}	// end anonymous namespace

GenericPort::GenericPort(MQTTPlugin* plugin, const std::string& id, const std::string& topic) : 
	opdi::CustomPort(id, OPDI_CUSTOM_PORT_TYPEGUID), MQTTPort(plugin, id, topic) {
	this->myValue = "";
	this->valueSet = true;		// causes setError in doWork
}

void GenericPort::subscribe(mqtt::async_client *client) {
	if (client->is_connected())
		try {
			this->plugin->openhat->logDebug(std::string(this->getID()) + ": Subscribing to topic: " + this->topic, this->logVerbosity);
			client->subscribe(this->topic, this->plugin->QOS, nullptr, this->listener);
			
			if (this->initialValue != "") {
				this->setValue(this->initialValue);
				this->initialValue = "";
			}
		}  catch (mqtt::exception& ex) {
			this->plugin->openhat->logError(std::string(this->getID()) + ": Error subscribing to topic " + this->topic + ": " + ex.what());
		}
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
	this->myValue = payload;
	this->valueSet = true;

	// reset query timer
	this->lastQueryTime = opdi_get_time_ms();
	// reset timeout counter
	this->timeoutCounter = opdi_get_time_ms();
}

uint8_t GenericPort::doWork(uint8_t canSend) {
	opdi::CustomPort::doWork(canSend);
	
	// time for refresh?
	if (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::QUERY, this));
		this->lastQueryTime = opdi_get_time_ms();
	}
	
	Poco::Mutex::ScopedLock lock(this->mutex);
	// no error?
	if (this->myValue != "") {
		// error timeout reached without message?
		if ((opdi_get_time_ms() - this->timeoutCounter) / 1000 > this->plugin->timeoutSeconds) {
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


HG06337Switch::HG06337Switch(MQTTPlugin* plugin, const std::string& id, const std::string& topic) : 
	opdi::DigitalPort(id.c_str(), OPDI_PORTDIRCAP_OUTPUT, 0), MQTTPort(plugin, id, topic) {
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
	if (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::QUERY, this));
		this->lastQueryTime = opdi_get_time_ms();
	}
	
	Poco::Mutex::ScopedLock lock(this->mutex);
	// no error?
	if (this->switchState > -1) {
		// error timeout reached without message?
		if ((opdi_get_time_ms() - this->timeoutCounter) / 1000 > this->plugin->timeoutSeconds) {
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


/*
FritzDECT200Power::FritzDECT200Power(FritzBoxPlugin* plugin, const std::string& id, const std::string& ain, int queryInterval) : 
	opdi::DialPort(id.c_str()), FritzPort(id, ain, queryInterval) {
	this->plugin = plugin;
	this->power = -1;	// unknown
	this->lastQueryTime = 0;
	this->valueSet = true;		// causes setError in doWork

	this->minValue = 0;
	this->maxValue = 2300000;	// measured in mW; 2300 W is maximum power load for the DECT200
	this->step = 1;
	this->position = 0;

	this->setUnit("electricPower_mW");
	this->setIcon("powermeter");

	// port is readonly
	this->setReadonly(true);
}

uint8_t FritzDECT200Power::doWork(uint8_t canSend) {
	opdi::DialPort::doWork(canSend);

	// time for refresh?
	if (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::GETPOWER, this));
		this->lastQueryTime = opdi_get_time_ms();
	}

	Poco::Mutex::ScopedLock lock(this->mutex);

	// has a value been returned?
	if (this->valueSet) {
		// values < 0 signify an error
		if (this->power > -1)
			opdi::DialPort::setPosition(this->power);
		else
			this->setError(Error::VALUE_NOT_AVAILABLE);
		this->valueSet = false;
	}

	return OPDI_STATUS_OK;
}

void FritzDECT200Power::setPower(int32_t power) {
	Poco::Mutex::ScopedLock lock(this->mutex);

	this->power = power;
	this->valueSet = true;
}

FritzDECT200Energy::FritzDECT200Energy(FritzBoxPlugin* plugin, const std::string& id, const std::string& ain, int queryInterval) : 
	opdi::DialPort(id.c_str()), FritzPort(id, ain, queryInterval) {
	this->plugin = plugin;
	this->energy = -1;	// unknown
	this->lastQueryTime = 0;
	this->valueSet = true;		// causes setError in doWork

	this->minValue = 0;
	this->maxValue = 2147483647;	// measured in Wh
	this->step = 1;
	this->position = 0;

	this->setUnit("electricEnergy_Wh");
	this->setIcon("energymeter");

	// port is readonly
	this->setReadonly(true);
}

uint8_t FritzDECT200Energy::doWork(uint8_t canSend) {
	opdi::DialPort::doWork(canSend);

	// time for refresh?
	if (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::GETENERGY, this));
		this->lastQueryTime = opdi_get_time_ms();
	}

	Poco::Mutex::ScopedLock lock(this->mutex);

	// has a value been returned?
	if (this->valueSet) {
		// values < 0 signify an error
		if (this->energy > -1)
			opdi::DialPort::setPosition(this->energy);
		else
			this->setError(Error::VALUE_NOT_AVAILABLE);
		this->valueSet = false;
	}

	return OPDI_STATUS_OK;
}

void FritzDECT200Energy::setEnergy(int32_t energy) {
	Poco::Mutex::ScopedLock lock(this->mutex);

	this->energy = energy;
	this->valueSet = true;
}

FritzDECT200Temperature::FritzDECT200Temperature(FritzBoxPlugin* plugin, const std::string& id, const std::string& ain, int queryInterval) :
	opdi::DialPort(id.c_str()), FritzPort(id, ain, queryInterval) {
	this->plugin = plugin;
	this->temperature = -9999;	// unknown
	this->lastQueryTime = 0;
	this->valueSet = true;		// causes setError in doWork

	// value is measured in centidegrees Celsius
	this->minValue = -1000;			// -100°C
	this->maxValue = 1000;			// +100°C
	this->step = 1;
	this->position = 0;

	this->setUnit("temperature_centiDegreesCelsius");
	this->setIcon("thermometer_celsius");

	// port is readonly
	this->setReadonly(true);
}

uint8_t FritzDECT200Temperature::doWork(uint8_t canSend) {
	opdi::DialPort::doWork(canSend);

	// time for refresh?
	if (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::GETTEMPERATURE, this));
		this->lastQueryTime = opdi_get_time_ms();
	}

	Poco::Mutex::ScopedLock lock(this->mutex);

	// has a value been returned?
	if (this->valueSet) {
		// values <= -9999 signifies an error
		if (this->temperature > -9999)
			opdi::DialPort::setPosition(this->temperature);
		else
			this->setError(Error::VALUE_NOT_AVAILABLE);
		this->valueSet = false;
	}

	return OPDI_STATUS_OK;
}

void FritzDECT200Temperature::setTemperature(int32_t temperature) {
	Poco::Mutex::ScopedLock lock(this->mutex);

	this->temperature = temperature;
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

void MQTTPlugin::publish(const std::string& topic, const std::string& payload) {
	this->openhat->logVerbose(this->nodeID + ": Sending message to '" + topic + "': '" + payload + "'", this->logVerbosity);

	//this->client->publish(mqtt::message(topic, payload, QOS, false));
}

void MQTTPlugin::errorOccurred(const std::string& message) {
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
void MQTTPlugin::setupPlugin(openhat::AbstractOpenHAT* abstractOpenHAT, const std::string& node, openhat::ConfigurationView::Ptr nodeConfig, openhat::ConfigurationView::Ptr parentConfig, const std::string& /*driverPath*/) {
	this->openhat = abstractOpenHAT;
	this->nodeID = node;
	this->timeoutSeconds = 30;	// time without received payloads until the devices go into error mode

	this->errorCount = 0;

	// get host and credentials
	this->host = this->openhat->getConfigString(nodeConfig, node, "Host", "", true);
	this->port = nodeConfig->getInt("Port", 1883);
	this->user = this->openhat->getConfigString(nodeConfig, node, "User", "", false);
	this->password = this->openhat->getConfigString(nodeConfig, node, "Password", "", false);
	this->timeoutSeconds = nodeConfig->getInt("Timeout", this->timeoutSeconds);
	this->QOS = 1;
	
	this->client = nullptr;
	this->terminateRequested = false;

	// store main node's group (will become the default of ports)
	std::string group = nodeConfig->getString("Group", "");

	// store main node's verbosity level (will become the default of ports)
	this->logVerbosity = this->openhat->getConfigLogVerbosity(nodeConfig, this->openhat->getLogVerbosity());

	// enumerate keys of the plugin's nodes (in specified order)
	this->openhat->logVerbose(node + ": Enumerating MQTT devices: " + node + ".Devices", this->logVerbosity);

	Poco::AutoPtr<openhat::ConfigurationView> nodes = this->openhat->createConfigView(nodeConfig, "Devices");
	nodeConfig->addUsedKey("Devices");

	// get ordered list of devices
	openhat::ConfigurationView::Keys deviceKeys;
	nodes->keys("", deviceKeys);

	typedef Poco::Tuple<int, std::string> Item;
	typedef std::vector<Item> ItemList;
	ItemList orderedItems;

	// create ordered list of port keys (by priority)
	for (auto it = deviceKeys.begin(), ite = deviceKeys.end(); it != ite; ++it) {

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
		this->openhat->logWarning("No devices configured in " + node + ".Devices; is this intended?");
	}

	// go through items, create ports in specified order
	auto nli = orderedItems.begin();
	auto nlie = orderedItems.end();
	while (nli != nlie) {
		std::string deviceName = nli->get<1>();

		this->openhat->logVerbose(node + ": Setting up MQTT device: " + deviceName, this->logVerbosity);

		// get device section from the configuration>
		Poco::AutoPtr<openhat::ConfigurationView> deviceConfig = this->openhat->createConfigView(parentConfig, deviceName);

		// get device type (required)
		std::string deviceType = this->openhat->getConfigString(deviceConfig, deviceName, "Type", "", true);

/*
		int queryInterval = deviceConfig->getInt("QueryInterval", 30);
		if (queryInterval < 1)
			this->openhat->throwSettingException(deviceName + ": Please specify a QueryInterval greater than 1: " + this->openhat->to_string(queryInterval));
 */
		// get topic (required)
		std::string topic = this->openhat->getConfigString(deviceConfig, deviceName, "Topic", "", true);

		if (deviceType == "Generic") {
			this->openhat->logVerbose(node + ": Setting up generic MQTT device port: " + deviceName, this->logVerbosity);

			// setup the generic port instance and add it
			GenericPort* genericPort = new GenericPort(this, deviceName, topic);
			// set default group: MQTT's node's group
			genericPort->setGroup(group);
			// set default log verbosity
			genericPort->logVerbosity = this->logVerbosity;
			this->openhat->configureCustomPort(deviceConfig, genericPort);
			this->openhat->addPort(genericPort);
			this->myPorts.push_back(genericPort);
		}
		else if (deviceType == "HG06337") {
			this->openhat->logVerbose(node + ": Setting up HG06337 device port: " + deviceName + "_Switch", this->logVerbosity);

			if (parentConfig->hasProperty(deviceName + "_Switch.Type")) {
				this->openhat->logVerbose(node + ": Setting up HG06337 device port: " + deviceName + "_Switch", this->logVerbosity);
				Poco::AutoPtr<openhat::ConfigurationView> portConfig = this->openhat->createConfigView(parentConfig, deviceName + "_Switch");
				// check type
				if (portConfig->getString("Type") != "DigitalPort")
					this->openhat->throwSettingException(node + ": HG06337 Switch device port must be of type 'DigitalPort'");
				// setup the switch port instance and add it
				HG06337Switch* switchPort = new HG06337Switch(this, deviceName + "_Switch", topic);
				// set default group: MQTT's node's group
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
/*
			if (parentConfig->hasProperty(deviceName + "_Power.Type")) {
				this->openhat->logVerbose(node + ": Setting up FritzBoxPlugin device port: " + deviceName + "_Power", this->logVerbosity);
				Poco::AutoPtr<openhat::ConfigurationView> portConfig = this->openhat->createConfigView(parentConfig, deviceName + "_Power");
				// check type
				if (portConfig->getString("Type") != "DialPort")
					this->openhat->throwSettingException(node + ": FritzDECT200 Power device port must be of type 'DialPort'");
				// setup the power port instance and add it
				FritzDECT200Power* powerPort = new FritzDECT200Power(this, deviceName + "_Power", ain, queryInterval);
				// set default group: FritzBox's node's group
				powerPort->setGroup(group);
				// set default log verbosity
				powerPort->logVerbosity = this->logVerbosity;
				// allow only basic settings to be changed
				this->openhat->configurePort(portConfig, powerPort, 0);
				this->openhat->addPort(powerPort);
			} else {
				this->openhat->logVerbose(node + ": FritzBoxPlugin device port: " + deviceName + "_Power.Type not found, ignoring", this->logVerbosity);
			}

			if (parentConfig->hasProperty(deviceName + "_Energy.Type")) {
				this->openhat->logVerbose(node + ": Setting up FritzBoxPlugin device port: " + deviceName + "_Energy", this->logVerbosity);
				Poco::AutoPtr<openhat::ConfigurationView> portConfig = this->openhat->createConfigView(parentConfig, deviceName + "_Energy");
				// check type
				if (portConfig->getString("Type") != "DialPort")
					this->openhat->throwSettingException(node + ": FritzDECT200 Energy device port must be of type 'DialPort'");
				// setup the energy port instance and add it
				FritzDECT200Energy* energyPort = new FritzDECT200Energy(this, deviceName + "_Energy", ain, queryInterval);
				// set default group: FritzBox's node's group
				energyPort->setGroup(group);
				// set default log verbosity
				energyPort->logVerbosity = this->logVerbosity;
				// allow only basic settings to be changed
				this->openhat->configurePort(portConfig, energyPort, 0);
				this->openhat->addPort(energyPort);
			} else {
				this->openhat->logVerbose(node + ": FritzBoxPlugin device port: " + deviceName + "_Energy.Type not found, ignoring", this->logVerbosity);
			}

			if (parentConfig->hasProperty(deviceName + "_Temperature.Type")) {
				this->openhat->logVerbose(node + ": Setting up FritzBoxPlugin device port: " + deviceName + "_Temperature", this->logVerbosity);
				Poco::AutoPtr<openhat::ConfigurationView> portConfig = this->openhat->createConfigView(parentConfig, deviceName + "_Temperature");
				// check type
				if (portConfig->getString("Type") != "DialPort")
					this->openhat->throwSettingException(node + ": FritzDECT200 Temperature device port must be of type 'DialPort'");
				// setup the energy port instance and add it
				FritzDECT200Temperature* temperaturePort = new FritzDECT200Temperature(this, deviceName + "_Temperature", ain, queryInterval);
				// set default group: FritzBox's node's group
				temperaturePort->setGroup(group);
				// set default log verbosity
				temperaturePort->logVerbosity = this->logVerbosity;
				// allow only basic settings to be changed
				this->openhat->configurePort(portConfig, temperaturePort, 0);
				this->openhat->addPort(temperaturePort);
			} else {
				this->openhat->logVerbose(node + ": FritzBoxPlugin device port: " + deviceName + "_Temperature.Type not found, ignoring", this->logVerbosity);
			}

		} else
			this->openhat->throwSettingException(node + ": This plugin does not support the device type '" + deviceType + "'");
*/
		++nli;
	}

	// this->openhat->addConnectionListener(this);

	this->workThread.setName(node + " work thread");
	this->workThread.start(*this);

	this->openhat->logVerbose(node + ": MQTTPlugin setup completed successfully", this->logVerbosity);
}

void MQTTPlugin::masterConnected() {
}

void MQTTPlugin::masterDisconnected() {
}

bool MQTTPlugin::is_connected(void) {
	return this->client != nullptr && this->client->is_connected();
}

void MQTTPlugin::run(void) {

	this->openhat->logVerbose(this->nodeID + ": MQTTPlugin worker thread started", this->logVerbosity);

//	sample_mem_persistence persist;
	Callback callback(this);
	
	while (!this->openhat->shutdownRequested && !this->terminateRequested) {
		
		this->openhat->logExtreme(this->nodeID + ": Worker thread loop...", this->logVerbosity);

		// current client disconnected?
		if (this->client != nullptr) {
			if (!this->client->is_connected()) {
				this->openhat->logDebug(this->nodeID + ": Client not connected", this->logVerbosity);
				delete this->client;
				this->client = nullptr;
			}
		}
		
		if (this->client == nullptr) {
			this->openhat->logDebug(this->nodeID + ": Initializing MQTT client", this->logVerbosity);
			// try to connect to the server
			std::string server_address = std::string("tcp://") + this->host + ":" + this->openhat->to_string(this->port);
			this->client = new mqtt::async_client(server_address, this->openhat->getSlaveName());
			
			this->client->set_callback(callback);
			
			mqtt::connect_options connOpts;
			connOpts.set_keep_alive_interval(20);
			connOpts.set_clean_session(true);

			try {
				this->openhat->logVerbose(this->nodeID + ": Connecting to MQTT server: " + server_address, this->logVerbosity);
				mqtt::token_ptr cr = this->client->connect(connOpts);
			} catch (const mqtt::exception& exc) {
				this->openhat->logError(this->nodeID + ": Error connecting to MQTT server " + server_address + ": " + exc.what());
				delete this->client;
				this->client = nullptr;
			} catch (const std::exception& exc) {
				this->openhat->logError(this->nodeID + ": Error connecting to MQTT server " + server_address + ": " + exc.what());
				delete this->client;
				this->client = nullptr;
			}
			if (this->client == nullptr) {
				// sleep and try again
                struct timespec aSleep;
                struct timespec aRem;
				aSleep.tv_sec = 10;
				aSleep.tv_nsec = 0;
				nanosleep(&aSleep, &aRem);
			} else {
				// wait until connection established or timeout up
				uint64_t aTime = opdi_get_time_ms();
				while ((opdi_get_time_ms() - aTime < this->timeoutSeconds * 1000)
						&& !this->client->is_connected()) {
					this->openhat->logExtreme(this->nodeID + ": Waiting for connection...", this->logVerbosity);
					struct timespec aSleep;
					struct timespec aRem;
					aSleep.tv_sec = 0;
					aSleep.tv_nsec = 1000000;
					nanosleep(&aSleep, &aRem);
				}
				
				// still not connected?
				if (!this->client->is_connected()) {
					this->openhat->logError(this->nodeID + ": Could not connect to MQTT server, retrying");
					delete this->client;
					this->client = nullptr;
/*
					// sleep and try again
					struct timespec aSleep;
					struct timespec aRem;
					aSleep.tv_sec = 10;
					aSleep.tv_nsec = 0;
					nanosleep(&aSleep, &aRem);
 * */
				}
			}
		}

		if (this->client != nullptr && this->client->is_connected()) {
			try {
				this->openhat->logExtreme(this->nodeID + ": Waiting for actions...", this->logVerbosity);
				Poco::Notification::Ptr notification = this->queue.waitDequeueNotification(100);
				if (!this->openhat->shutdownRequested && notification) {
					ActionNotification::Ptr workNf = notification.cast<ActionNotification>();
					if (workNf) {
						std::string action;
						switch (workNf->type) {
						case ActionNotification::SUBSCRIBE: action = "SUBSCRIBE"; break;
						case ActionNotification::QUERY: action = "QUERY"; break;
						case ActionNotification::RECONNECT: action = "RECONNECT"; break;
						}
						MQTTPort* port = (MQTTPort*)workNf->port;
						if (port == nullptr)
							this->openhat->logDebug(this->nodeID + ": Processing requested action: " + action, this->logVerbosity);
						else
							this->openhat->logDebug(this->nodeID + ": Processing requested action: " + action + " for port: " + port->pid, this->logVerbosity);
						// inspect action and decide what to do
						switch (workNf->type) {
						case ActionNotification::SUBSCRIBE: {
							this->openhat->logVerbose(nodeID + ": Connection established", this->logVerbosity);
							// client is connected, subscribe ports to their topics
							auto it = this->myPorts.begin();
							auto ite = this->myPorts.end();
							while (it != ite) {
								this->openhat->logExtreme(nodeID + ": Subscribing port: " + (*it)->pid, this->logVerbosity);
								(*it)->subscribe(client);
								// post query notification
								this->openhat->logExtreme(nodeID + ": Requesting query...", this->logVerbosity);
								queue.enqueueNotification(new ActionNotification(ActionNotification::QUERY, *it));
								this->openhat->logExtreme(nodeID + ": Done.", this->logVerbosity);
								++it;
							}
							break;
						}
						case ActionNotification::QUERY:  {
							if (this->client != nullptr && this->client->is_connected())
								workNf->port->query(this->client);
							break;
						}
						case ActionNotification::RECONNECT:
							break;
						}
					}
				}
			} catch (Poco::Exception &e) {
				this->openhat->logNormal(this->nodeID + ": Unhandled exception in worker thread: " + this->openhat->getExceptionMessage(e), this->logVerbosity);
			}
		} else
			this->openhat->logExtreme(this->nodeID + ": Client not connected", this->logVerbosity);
	}
	
	if (this->client != nullptr && this->client->is_connected()) {
		this->client->disconnect();
		delete this->client;
		this->client = nullptr;
	}

	this->openhat->logVerbose(this->nodeID + ": MQTTPlugin worker thread terminated", this->logVerbosity);
}

void MQTTPlugin::terminate() {
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
	return new MQTTPlugin();
}
