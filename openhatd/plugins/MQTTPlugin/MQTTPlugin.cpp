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
#include "Poco/RegularExpression.h"
#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Parser.h>

#include "OPDI_Ports.h"
#include "AbstractOpenHAT.h"
#include "ExpressionPort.h"

#include "TypeGUIDs.h"

// requires PAHO C libraries, see https://github.com/eclipse/paho.mqtt.cpp
#include "mqtt/client.h"

#define DEFAULT_QUERY_INTERVAL	30
#define DEFAULT_TIMEOUT			(2 * DEFAULT_QUERY_INTERVAL)

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
	std::string getTopic;
	std::string getPayload;
	bool valueSet;
	uint64_t lastQueryTime;
	int32_t timeoutSeconds;
	uint64_t queryInterval;
	uint64_t lastReceiveTime;
	Poco::Mutex mutex;
	action_listener listener;

	std::string inputPattern;
	std::string outputPattern;

	Poco::RegularExpression* inputRegex;
	Poco::RegularExpression* outputRegex;

	virtual std::string extractFirstGroup(std::string& expr, Poco::RegularExpression* regex, std::string& str);

	virtual std::size_t replace_all(std::string& inout, std::string what, std::string with);

	virtual void tokenize(std::string const& str, const char delim,	std::vector<std::string>& out);
public:
	std::string pid;
	
	MQTTPort(MQTTPlugin *plugin, const std::string& id, const std::string& topic): listener(id) {
		this->plugin = plugin;
		this->pid = id;
		this->topic = topic;
		this->valueSet = false;
		this->lastQueryTime = 0;
		this->queryInterval = DEFAULT_QUERY_INTERVAL;
		this->lastReceiveTime = 0;
		this->inputRegex = nullptr;
		this->outputRegex = nullptr;
	};

	virtual void configure(openhat::ConfigurationView::Ptr config);
	
	virtual void subscribe(mqtt::async_client *client) = 0;
	
	virtual void query(mqtt::async_client *client);

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
//		bool handled = false;
		while (it != ite) {
			if ((*it)->topic == msg->get_topic()) {
				(*it)->handle_payload(msg->to_string());
//				handled = true;
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
	int port = 1883;
	std::string user;
	std::string password;
	uint32_t timeoutSeconds = DEFAULT_TIMEOUT;
	int QOS = 0;

	std::string sid;
	int errorCount = 0;
	std::string lastErrorMessage;

	opdi::LogVerbosity logVerbosity = opdi::LogVerbosity::NORMAL;

	Poco::NotificationQueue queue;
	Poco::Thread workThread;
	
	mqtt::async_client *client = nullptr;
	
	bool terminateRequested = false;
	
	void publish(const std::string& topic, const std::string& payload);

	void errorOccurred(const std::string& message);

public:
	openhat::AbstractOpenHAT* openhat = nullptr;

	virtual void masterConnected(void) override;
	virtual void masterDisconnected(void) override;
	
	bool is_connected(void);

	virtual void run(void);

	virtual void setupPlugin(openhat::AbstractOpenHAT* abstractOpenHAT, const std::string& node, openhat::ConfigurationView::Ptr nodeConfig, openhat::ConfigurationView::Ptr parentConfig, const std::string& driverPath) override;
	
	virtual void terminate(void) override;
	
	Poco::JSON::Object::Ptr GetObject(Poco::JSON::Object::Ptr jsonObject, const std::string& key);

	std::string GetString(Poco::JSON::Object::Ptr jsonObject, const std::string& key);
};

////////////////////////////////////////////////////////////////////////
// Plugin ports
////////////////////////////////////////////////////////////////////////

class DigitalPort : public opdi::DigitalPort, public MQTTPort {
	friend class MQTTPlugin;
protected:
	std::string value;

	std::string setTopic;
	std::string inputValueLow;
	std::string inputValueHigh;
	std::string outputValueLow;
	std::string outputValueHigh;

public:
	DigitalPort(MQTTPlugin* plugin, const std::string& id, const std::string& topic);

	virtual void subscribe(mqtt::async_client* client) override;

	virtual void handle_payload(std::string payload) override;

	virtual uint8_t doWork(uint8_t canSend) override;

	virtual void configure(openhat::ConfigurationView::Ptr config) override;

	virtual bool setLine(uint8_t line, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT) override;
};

class DialPort : public opdi::DialPort, public MQTTPort {
	friend class MQTTPlugin;
protected:
	int64_t newValue;

	std::string setTopic;
	std::string inputExpression;
	std::string outputExpression;

	openhat::ExpressionPort* inputExpr;
	openhat::ExpressionPort* outputExpr;

public:
	DialPort(MQTTPlugin* plugin, const std::string& id, const std::string& topic);

	virtual void subscribe(mqtt::async_client* client) override;

	virtual void handle_payload(std::string payload) override;

	virtual uint8_t doWork(uint8_t canSend) override;

	virtual void configure(openhat::ConfigurationView::Ptr config) override;

	virtual bool setPosition(int64_t position, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT) override;
};


class SelectPort : public opdi::SelectPort, public MQTTPort {
	friend class MQTTPlugin;
protected:
	int16_t newPosition;

	std::string setTopic;
	std::string inputMap;
	std::vector<std::string> inputValues;
	std::string outputMap;
	std::vector<std::string> outputValues;

public:
	SelectPort(MQTTPlugin* plugin, const std::string& id, const std::string& topic);

	virtual void subscribe(mqtt::async_client* client) override;

	virtual void handle_payload(std::string payload) override;

	virtual uint8_t doWork(uint8_t canSend) override;

	virtual void configure(openhat::ConfigurationView::Ptr config, openhat::ConfigurationView::Ptr parentConfig);

	virtual bool setPosition(uint16_t value, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT) override;
};


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
	
	virtual bool setValue(const std::string& value, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT) override;
	
	virtual void configure(Poco::Util::AbstractConfiguration::Ptr config) override;
};

class EventPort : public opdi::DigitalPort, public MQTTPort {
	friend class MQTTPlugin;
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
	EventPort(MQTTPlugin* plugin, const std::string& id, const std::string& topic);
	
	virtual void subscribe(mqtt::async_client *client) override;
	
	virtual void query(mqtt::async_client *client) override;
	
	virtual void handle_payload(std::string payload) override;

	virtual uint8_t doWork(uint8_t canSend) override;
	
	virtual void configure(openhat::ConfigurationView::Ptr config);

	virtual void prepare() override;
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

	virtual bool setLine(uint8_t line, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;

	virtual uint8_t doWork(uint8_t canSend) override;
};


class TasmotaSwitch : public opdi::DigitalPort, public MQTTPort {
	friend class MQTTPlugin;
protected:
        std::string deviceID;
	int8_t switchState;
	int8_t initialLine;
	virtual void setSwitchState(int8_t line);

public:
	TasmotaSwitch(MQTTPlugin* plugin, const std::string& id, const std::string& deviceID);
	
	virtual void subscribe(mqtt::async_client *client) override;
	
	virtual void query(mqtt::async_client *client) override;
	
	virtual void handle_payload(std::string payload) override;

	virtual bool setLine(uint8_t line, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;

	virtual uint8_t doWork(uint8_t canSend) override;
};

class TasmotaPower : public opdi::DialPort, public MQTTPort {
	friend class MQTTPlugin;
protected:
        std::string deviceID;
	int64_t newValue;
	virtual void setValue(int64_t value);

public:
	TasmotaPower(MQTTPlugin* plugin, const std::string& id, const std::string& deviceID);
	
	virtual void subscribe(mqtt::async_client *client) override;
	
	virtual void query(mqtt::async_client *client) override;
	
	virtual void handle_payload(std::string payload) override;

	virtual uint8_t doWork(uint8_t canSend) override;
};

}	// end anonymous namespace

////////////////////////////////////////////////////////////////////////
// Plugin port implementations
////////////////////////////////////////////////////////////////////////

std::string MQTTPort::extractFirstGroup(std::string& expr, Poco::RegularExpression* regex, std::string& str) {
	std::vector<std::string> strings;
	try {
		regex->split(str, strings, 0);
	}
	catch (Poco::RegularExpressionException& ree) {
		this->plugin->openhat->logError(this->pid + ": Error evaluating regular expression '" + expr + "': " + ree.what());
		return std::string();
	}
	if (strings.size() == 0)
		return std::string();
	return strings[0];
}

std::size_t MQTTPort::replace_all(std::string& inout, std::string what, std::string with)
{
	std::size_t count{};
	for (std::string::size_type pos{};
		inout.npos != (pos = inout.find(what.data(), pos, what.length()));
		pos += with.length(), ++count) {
		inout.replace(pos, what.length(), with.data(), with.length());
	}
	return count;
}

void MQTTPort::tokenize(std::string const& str, const char delim,
	std::vector<std::string>& out)
{
	size_t start;
	size_t end = 0;

	while ((start = str.find_first_not_of(delim, end)) != std::string::npos) {
		end = str.find(delim, start);
		out.push_back(str.substr(start, end - start));
	}
}

void MQTTPort::configure(openhat::ConfigurationView::Ptr config) {
	this->getTopic = config->getString("GetTopic", "");
	this->getPayload = config->getString("GetPayload", "");
	this->inputPattern = config->getString("InputPattern", "");
	if (this->inputPattern.empty())
		this->inputPattern = "(.*)";
	this->inputRegex = new Poco::RegularExpression(this->inputPattern);
	this->outputPattern = config->getString("OutputPattern", "");
	if (this->outputPattern.empty())
		this->outputPattern = "$value";
	this->outputRegex = new Poco::RegularExpression("\\$value");
}

void MQTTPort::query(mqtt::async_client* client) {
	if (client->is_connected() && !this->getTopic.empty()) {
		try {
			client->publish(this->getTopic, this->getPayload);
		}
		catch (mqtt::exception& ex) {
			this->plugin->openhat->logError(std::string("Error publishing MQTT topic: " + this->getTopic + " : " + ex.what()));
		}
	}
}

DigitalPort::DigitalPort(MQTTPlugin* plugin, const std::string& id, const std::string& topic) :
	opdi::DigitalPort(id.c_str(), OPDI_PORTDIRCAP_BIDI, 0), MQTTPort(plugin, id, topic) {
	// default mode is "output"
	this->setMode(OPDI_DIGITAL_MODE_OUTPUT);
	this->refreshMode = RefreshMode::REFRESH_AUTO;
	this->valueSet = true;		// causes error in doWork
}

void DigitalPort::subscribe(mqtt::async_client* client) {
	if (client->is_connected())
		try {
		this->plugin->openhat->logDebug(std::string(this->getID()) + ": Subscribing to topic: " + this->topic, this->logVerbosity);
		client->subscribe(this->topic, this->plugin->QOS, nullptr, this->listener);
	}
	catch (mqtt::exception& ex) {
		this->plugin->openhat->logError(std::string(this->getID()) + ": Error subscribing to topic " + this->topic + ": " + ex.what());
	}
}

void DigitalPort::handle_payload(std::string payload) {
	this->plugin->openhat->logDebug(this->pid + ": Payload received: '" + payload + "'", this->logVerbosity);

	Poco::Mutex::ScopedLock lock(this->mutex);
	this->value = payload;
	this->valueSet = true;
}

uint8_t DigitalPort::doWork(uint8_t canSend) {
	opdi::DigitalPort::doWork(canSend);

	Poco::Mutex::ScopedLock lock(this->mutex);

	// time for refresh?
	if (!this->getTopic.empty() && (this->queryInterval > 0) && (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000ul)) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::QUERY, this));
	}

	// no error?
	if (this->getError() == Error::VALUE_OK) {
		// error timeout reached without message?
		if ((this->timeoutSeconds > 0) && (opdi_get_time_ms() - this->lastReceiveTime) / 1000 > this->timeoutSeconds) {
			// change to error state
			this->setError(Error::VALUE_EXPIRED);
		}
	}

	// value received?
	if (this->valueSet) {
		try {
			// compare received value
			if (this->value == this->inputValueLow)
				opdi::DigitalPort::setLine(0);
			else if (this->value == this->inputValueHigh)
				opdi::DigitalPort::setLine(1);
			else {
				if (value != "")	// ignore initial error
					this->plugin->openhat->logWarning(this->pid + ": Unspecified value received: '" + value + "'");
				this->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
			}
		}
		catch (std::exception& e) {
			this->plugin->openhat->logError(this->pid + ": Error setting DigitalPort line: " + e.what());
		}
		this->valueSet = false;
	}

	return OPDI_STATUS_OK;
}

void DigitalPort::configure(openhat::ConfigurationView::Ptr config) {
	this->plugin->openhat->configureDigitalPort(config, this, false);

	MQTTPort::configure(config);

	this->setTopic = config->getString("SetTopic", "");
	// configure as input if there is no set topic
	if (this->setTopic == "") {
		this->setReadonly(true);
		this->setDirCaps(OPDI_PORTDIRCAP_INPUT);
	}

	this->inputValueLow = config->getString("InputValueLow");
	this->inputValueHigh = config->getString("InputValueHigh");
	this->outputValueLow = config->getString("OutputValueLow", "");
	this->outputValueHigh = config->getString("OutputValueHigh", "");
}

bool DigitalPort::setLine(uint8_t line, ChangeSource changeSource) {
	bool changed = opdi::DigitalPort::setLine(line, changeSource);

	if (this->getError() != Error::VALUE_OK)
		return changed;

	if (changed) {
		// publish state if specified
		if ((this->getLine() == 0) && (this->setTopic != "") && (this->outputValueLow != "")) {
			this->plugin->publish(this->setTopic, this->outputValueLow);
		}
		else if ((this->getLine() == 1) && (this->setTopic != "") && (this->outputValueHigh != "")) {
			this->plugin->publish(this->setTopic, this->outputValueHigh);
		}
	}
	return changed;
}


DialPort::DialPort(MQTTPlugin* plugin, const std::string& id, const std::string& topic) :
	opdi::DialPort(id.c_str(), -999999999999, 999999999999, 1), MQTTPort(plugin, id, topic) {
	this->newValue = this->getMin() - 1;
	this->valueSet = true;		// causes setError in doWork
	this->inputExpr = nullptr;
	this->outputExpr = nullptr;
}

void DialPort::configure(openhat::ConfigurationView::Ptr config) {
	this->plugin->openhat->configureDialPort(config, this, false);

	MQTTPort::configure(config);

	this->setTopic = config->getString("SetTopic", "");
	// configure as input if there is no set topic
	if (this->setTopic == "") {
		this->setReadonly(true);
		this->setDirCaps(OPDI_PORTDIRCAP_INPUT);
	}

	this->inputExpression = config->getString("InputExpression", "");
	if (this->inputExpression.empty())
		this->inputExpression = "$value";
	this->inputExpr = new openhat::ExpressionPort(this->plugin->openhat, (this->pid + "_inputExpr").c_str());
	this->outputExpression = config->getString("OutputExpression", "");
	if (this->outputExpression.empty())
		this->outputExpression = "$value";
	this->outputExpr = new openhat::ExpressionPort(this->plugin->openhat, (this->pid + "_outputExpr").c_str());
}

void DialPort::subscribe(mqtt::async_client* client) {
	if (client->is_connected())
		try {
		this->plugin->openhat->logDebug(std::string(this->getID()) + ": Subscribing to topic: " + this->topic, this->logVerbosity);
		client->subscribe(this->topic, this->plugin->QOS, nullptr, this->listener);
	}
	catch (mqtt::exception& ex) {
		this->plugin->openhat->logError(std::string(this->getID()) + ": Error subscribing to topic " + this->topic + ": " + ex.what());
	}
}

void DialPort::handle_payload(std::string payload) {
	this->plugin->openhat->logDebug(this->pid + ": Payload received: '" + payload + "'");

	Poco::Mutex::ScopedLock lock(this->mutex);

	if (payload.empty()) {
		this->newValue = this->getMin() - 1;
		this->valueSet = true;
		return;
	}

	// extract value from input pattern
	std::string input = this->extractFirstGroup(this->inputPattern, this->inputRegex, payload);
	// no value or not matched?
	if (input.empty()) {
		this->plugin->openhat->logWarning(this->pid + ": Payload does not match input pattern: '" + payload + "'");
		// set to error state
		this->newValue = this->getMin() - 1;
		this->valueSet = true;
		return;
	}

	// replace $value in input expression with the extracted string
	std::string expr = this->inputExpression;
	if (replace_all(expr, "$value", input) == 0) {
		this->plugin->openhat->logWarning(this->pid + ": Could not replace placeholder '$value' in input expression: '" + this->inputExpression + "'");
		// set to error state
		this->newValue = this->getMin() - 1;
		this->valueSet = true;
		return;
	}

	// apply input expression to value
	this->inputExpr->expressionStr = expr;
	this->inputExpr->prepare();
	double value = this->inputExpr->apply();
	if (std::isnan(value)) {
		this->plugin->openhat->logWarning(this->pid + ": Expression evaluated to NAN: '" + expr + "'");
		// set to error state
		this->newValue = this->getMin() - 1;
		this->valueSet = true;
		return;
	}

	this->newValue = (int64_t)value;
	this->valueSet = true;

	this->lastReceiveTime = opdi_get_time_ms();
}

uint8_t DialPort::doWork(uint8_t canSend) {
	opdi::DialPort::doWork(canSend);

	Poco::Mutex::ScopedLock lock(this->mutex);

	// time for refresh?
	if (!this->getTopic.empty() && (this->queryInterval > 0) && (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000)) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::QUERY, this));
	}

	// no error?
	if (this->getError() == Error::VALUE_OK) {
		// error timeout reached without message?
		if ((this->timeoutSeconds > 0) && (opdi_get_time_ms() - this->lastReceiveTime) / 1000 > this->timeoutSeconds) {
			// change to error state
			this->setError(Error::VALUE_EXPIRED);
		}
	}

	if (this->valueSet) {
		try {
			// values out of range cause an error
			if (this->newValue < this->getMin() || this->newValue > this->getMax()) {
				// change to error state
				this->setError(Error::VALUE_NOT_AVAILABLE);
			}
			else {
				// use base method to avoid triggering an action!
				opdi::DialPort::setPosition(this->newValue);
			}
		}
		catch (std::exception& e) {
			this->plugin->openhat->logError(this->pid + ": Error setting DialPort position: " + e.what());
		}
		this->valueSet = false;
	}

	return OPDI_STATUS_OK;
}

bool DialPort::setPosition(int64_t value, ChangeSource changeSource) {
	bool changed = opdi::DialPort::setPosition(value, changeSource);

	if (this->getError() != Error::VALUE_OK)
		return changed;

	if (!changed)
		return false;

	// replace $value in output expression with the position
	std::string expr = this->outputExpression;
	if (replace_all(expr, "$value", Poco::NumberFormatter::format(this->getPosition())) == 0) {
		this->plugin->openhat->logWarning(this->pid + ": Could not replace placeholder '$value' in output expression: '" + this->outputExpression + "'");
		return true;
	}

	// apply output expression
	this->outputExpr->expressionStr = expr;
	this->outputExpr->prepare();
	double dValue = this->outputExpr->apply();
	if (std::isnan(dValue)) {
		this->plugin->openhat->logWarning(this->pid + ": Expression evaluated to NAN: '" + expr + "'");
		return true;
	}

	std::string sValue = Poco::NumberFormatter::format(dValue);
	std::string output = this->outputPattern;
	if (this->outputRegex->subst(output, sValue, Poco::RegularExpression::RE_GLOBAL) == 0) {
		this->plugin->openhat->logWarning(this->pid + ": Could not replace placeholder '$value' in output pattern: '" + this->outputPattern + "'");
		return true;
	}

	// publish state if specified
	if ((this->setTopic != "") && (sValue != "")) {
		this->plugin->publish(this->setTopic, output);
	}

	return true;
}


SelectPort::SelectPort(MQTTPlugin* plugin, const std::string& id, const std::string& topic) :
	opdi::SelectPort(id.c_str()), MQTTPort(plugin, id, topic) {
	this->newPosition = -1;
	this->valueSet = true;		// causes setError in doWork
}

void SelectPort::configure(openhat::ConfigurationView::Ptr config, openhat::ConfigurationView::Ptr parentConfig) {
	this->plugin->openhat->configureSelectPort(config, parentConfig, this, false);

	MQTTPort::configure(config);

	this->setTopic = config->getString("SetTopic", "");
	// configure as input if there is no set topic
	if (this->setTopic == "") {
		this->setReadonly(true);
		this->setDirCaps(OPDI_PORTDIRCAP_INPUT);
	}

	this->inputMap = this->plugin->openhat->getConfigString(config, this->ID(), "InputMap", "", true);
	this->tokenize(this->inputMap, '|', this->inputValues);

	this->outputMap = this->plugin->openhat->getConfigString(config, this->ID(), "OutputMap", "", false);
	this->tokenize(this->outputMap, '|', this->outputValues);
}

void SelectPort::subscribe(mqtt::async_client* client) {
	if (client->is_connected()) {
		try {
			this->plugin->openhat->logDebug(std::string(this->getID()) + ": Subscribing to topic: " + this->topic, this->logVerbosity);
			client->subscribe(this->topic, this->plugin->QOS, nullptr, this->listener);
		}
		catch (mqtt::exception& ex) {
			this->plugin->openhat->logError(std::string(this->getID()) + ": Error subscribing to topic " + this->topic + ": " + ex.what());
		}
	}
}

void SelectPort::handle_payload(std::string payload) {
	this->plugin->openhat->logDebug(this->pid + ": Payload received: '" + payload + "'");

	Poco::Mutex::ScopedLock lock(this->mutex);

	if (payload.empty()) {
		this->newPosition = -1;
		this->valueSet = true;
		return;
	}

	// extract value from input pattern
	std::string input = this->extractFirstGroup(this->inputPattern, this->inputRegex, payload);
	// no value or not matched?
	if (input.empty()) {
		this->plugin->openhat->logWarning(this->pid + ": Payload does not match input pattern: '" + payload + "'");
		// set to error state
		this->newPosition = -1;
		this->valueSet = true;
		return;
	}

	// locate input string in input map
	auto it = std::find(this->inputValues.begin(), this->inputValues.end(), input);

	// element not found
	if (it == this->inputValues.end()) {
		this->plugin->openhat->logWarning(this->pid + ": Extracted payload input not in InputMap: '" + input + "'");
		// set to error state
		this->newPosition = -1;
		this->valueSet = true;
		return;
	}

	int index = it - this->inputValues.begin();
	if (index > this->getMaxPosition()) {
		this->plugin->openhat->logWarning(this->pid + ": Extracted payload input index in InputMap exceeds maximum position " + this->plugin->openhat->to_string(this->getMaxPosition()) + ": '" + input + "'");
		// set to error state
		this->newPosition = -1;
		this->valueSet = true;
		return;
	}

	this->newPosition = index;
	this->valueSet = true;

	
	this->lastReceiveTime = opdi_get_time_ms();
}

uint8_t SelectPort::doWork(uint8_t canSend) {
	opdi::SelectPort::doWork(canSend);

	Poco::Mutex::ScopedLock lock(this->mutex);

    // time for refresh?
    if (!this->getTopic.empty() && (this->queryInterval > 0) && (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000)) {
        this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::QUERY, this));
    }

	// no error?
	if (this->getError() == Error::VALUE_OK) {
		// error timeout reached without message?
		if ((this->timeoutSeconds > 0) && (opdi_get_time_ms() - this->lastReceiveTime) / 1000 > this->timeoutSeconds) {
			// change to error state
			this->setError(Error::VALUE_EXPIRED);
		}
	}

	if (this->valueSet) {
		try {
			// values < 0 signify an error
			if (this->newPosition < 0) {
				// change to error state
				this->setError(Error::VALUE_NOT_AVAILABLE);
			}
			else {
				// use base method to avoid triggering an action!
				opdi::SelectPort::setPosition(this->newPosition);
			}
		}
		catch (std::exception& e) {
			this->plugin->openhat->logError(this->pid + ": Error setting SelectPort position: " + e.what());
		}
		this->valueSet = false;
	}

	return OPDI_STATUS_OK;
}

bool SelectPort::setPosition(uint16_t value, ChangeSource changeSource) {
	bool changed = opdi::SelectPort::setPosition(value, changeSource);

	if (this->getError() != Error::VALUE_OK)
		return changed;

	if (!changed)
		return false;

	if (this->outputValues.size() < this->getPosition() + 1) {
		this->plugin->openhat->logWarning(this->pid + ": Not enough values in output map (position: " + this->plugin->openhat->to_string(this->getPosition()));
		return true;
	}
	std::string sValue = this->outputValues[this->getPosition()];
	std::string output = this->outputPattern;
	if (this->outputRegex->subst(output, sValue, Poco::RegularExpression::RE_GLOBAL) == 0) {
		this->plugin->openhat->logWarning(this->pid + ": Could not replace placeholder '$value' in output pattern: '" + this->outputPattern + "'");
		return true;
	}

	// publish state if specified
	if ((this->setTopic != "") && (sValue != "")) {
		this->plugin->publish(this->setTopic, output);
	}
	return true;
}


GenericPort::GenericPort(MQTTPlugin* plugin, const std::string& id, const std::string& topic) : 
	opdi::CustomPort(id, OPDI_CUSTOM_PORT_TYPEGUID), MQTTPort(plugin, id, topic) {
	this->myValue = "";
	this->valueSet = false;
}

void GenericPort::subscribe(mqtt::async_client *client) {
	if (client->is_connected())
		try {
			this->plugin->openhat->logDebug(std::string(this->getID()) + ": Subscribing to topic: " + this->topic, this->logVerbosity);
			client->subscribe(this->topic, this->plugin->QOS, nullptr, this->listener);
			
			if (this->initialValue != "") {
				Poco::Mutex::ScopedLock lock(this->mutex);
				this->myValue = this->initialValue;
				this->valueSet = true;
				this->initialValue = "";
			}
		}  catch (mqtt::exception& ex) {
			this->plugin->openhat->logError(std::string(this->getID()) + ": Error subscribing to topic " + this->topic + ": " + ex.what());
		}
}

void GenericPort::query(mqtt::async_client* client) {
	this->plugin->openhat->logDebug(this->pid + ": Querying state", this->logVerbosity);
	if (client->is_connected()) {
		try {
			client->publish(this->topic + "/get", "{\"state\":\"\"}");
		}
		catch (mqtt::exception& ex) {
			this->plugin->openhat->logError(std::string(this->getID()) + ": Error querying state: " + this->topic + ": " + ex.what());
		}
	}
}

void GenericPort::handle_payload(std::string payload) {
	this->plugin->openhat->logDebug(this->pid + ": Payload received: '" + payload + "'");

	Poco::Mutex::ScopedLock lock(this->mutex);
	this->myValue = payload;
	this->valueSet = true;

	
	this->lastReceiveTime = opdi_get_time_ms();
}

uint8_t GenericPort::doWork(uint8_t canSend) {
	opdi::CustomPort::doWork(canSend);
	
	this->logExtreme("Value: " + this->value);
	
	// time for refresh?
	if ((this->queryInterval > 0) && (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000)) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::QUERY, this));
	}
	
	Poco::Mutex::ScopedLock lock(this->mutex);
	// no error?
	if (this->getError() == Error::VALUE_OK) {
		// error timeout reached without message?
		if ((this->timeoutSeconds > 0) && (opdi_get_time_ms() - this->lastReceiveTime) / 1000 > this->timeoutSeconds) {
			// change to error state
			this->myValue = "";
			this->setError(Error::VALUE_EXPIRED);
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

bool GenericPort::setValue(const std::string& newValue, ChangeSource changeSource) {
	bool changed = opdi::CustomPort::setValue(newValue, changeSource);
	
	try {
		if (this->plugin->client != nullptr && this->plugin->client->is_connected()) {
			this->plugin->client->publish(this->topic + "/set", newValue);
			this->plugin->openhat->logDebug(std::string(this->getID()) + ": Published value to: " + this->topic + ": " + newValue);
		}
	}  catch (mqtt::exception& ex) {
		this->plugin->openhat->logError(std::string(this->getID()) + ": Error publishing state: " + this->topic + ": " + ex.what());
	}

	return true;
}

void GenericPort::configure(Poco::Util::AbstractConfiguration::Ptr config) {
	CustomPort::configure(config);
	this->initialValue = config->getString("Value", "");
}


EventPort::EventPort(MQTTPlugin* plugin, const std::string& id, const std::string& topic) : 
	opdi::DigitalPort(id.c_str(), OPDI_PORTDIRCAP_OUTPUT, 0), MQTTPort(plugin, id, topic) {
	this->setMode(OPDI_DIGITAL_MODE_OUTPUT);
	// events are enabled by default
	this->setLine(1);
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
	opdi::DigitalPort::prepare();

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

HG06337Switch::HG06337Switch(MQTTPlugin* plugin, const std::string& id, const std::string& topic) : 
	opdi::DigitalPort(id.c_str(), OPDI_PORTDIRCAP_OUTPUT, 0), MQTTPort(plugin, id, topic) {
	this->switchState = -1;	// unknown
	this->valueSet = true;		// causes setError in doWork
	this->initialLine = -1;
	
	this->setMode(OPDI_DIGITAL_MODE_OUTPUT);
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

void HG06337Switch::query(mqtt::async_client* client) {
	this->plugin->openhat->logDebug(this->pid + ": Querying state", this->logVerbosity);
	if (client->is_connected()) {
		try {
			client->publish(this->topic + "/get", "{\"state\":\"\"}");
		}
		catch (mqtt::exception& ex) {
			this->plugin->openhat->logError(std::string(this->getID()) + ": Error querying state: " + this->topic + ": " + ex.what());
		}
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
	
	this->lastReceiveTime = opdi_get_time_ms();
}

uint8_t HG06337Switch::doWork(uint8_t canSend) {
	opdi::DigitalPort::doWork(canSend);

	// time for refresh?
	if ((this->queryInterval > 0) && (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000)) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::QUERY, this));
	}
	
	Poco::Mutex::ScopedLock lock(this->mutex);
	// no error?
	if (this->getError() == Error::VALUE_OK) {
		// error timeout reached without message?
		if ((this->timeoutSeconds > 0) && (opdi_get_time_ms() - this->lastReceiveTime) / 1000 > this->timeoutSeconds) {
			// change to error state
			this->switchState = -1;
			this->setError(Error::VALUE_EXPIRED);
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

bool HG06337Switch::setLine(uint8_t line, ChangeSource changeSource) {
	bool changed = opdi::DigitalPort::setLine(line, changeSource);

	if (this->plugin->client == nullptr)
		return changed;

	if (!changed)
		return false;

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
	return true;
}

void HG06337Switch::setSwitchState(int8_t switchState) {
	Poco::Mutex::ScopedLock lock(this->mutex);

	this->switchState = switchState;
	this->valueSet = true;
}

TasmotaSwitch::TasmotaSwitch(MQTTPlugin* plugin, const std::string& id, const std::string& deviceID) : 
	opdi::DigitalPort(id.c_str(), OPDI_PORTDIRCAP_OUTPUT, 0), MQTTPort(plugin, id, std::string("stat/") + deviceID + "/POWER") {
    this->deviceID = deviceID;
    this->switchState = -1;	// unknown
    this->valueSet = true;		// causes setError in doWork
    this->initialLine = -1;
	
	this->setMode(OPDI_DIGITAL_MODE_OUTPUT);
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
	
	this->lastReceiveTime = opdi_get_time_ms();
}

uint8_t TasmotaSwitch::doWork(uint8_t canSend) {
	opdi::DigitalPort::doWork(canSend);

	// time for refresh?
	if ((this->queryInterval > 0) && (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000)) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::QUERY, this));
	}
	
	Poco::Mutex::ScopedLock lock(this->mutex);
	// no error?
	if (this->getError() == Error::VALUE_OK) {
		// error timeout reached without message?
		if ((this->timeoutSeconds > 0) && (this->timeoutSeconds > 0) && (opdi_get_time_ms() - this->lastReceiveTime) / 1000 > this->timeoutSeconds) {
			// change to error state
			this->switchState = -1;
			this->setError(Error::VALUE_EXPIRED);
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

bool TasmotaSwitch::setLine(uint8_t line, ChangeSource changeSource) {
	bool changed = opdi::DigitalPort::setLine(line, changeSource);

	if (this->plugin->client == nullptr)
		return changed;
	if (!changed)
		return false;

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
	return true;
}

void TasmotaSwitch::setSwitchState(int8_t switchState) {
	Poco::Mutex::ScopedLock lock(this->mutex);

	this->switchState = switchState;
	this->valueSet = true;
}


TasmotaPower::TasmotaPower(MQTTPlugin* plugin, const std::string& id, const std::string& deviceID) : 
	opdi::DialPort(id.c_str(), 0, 999999999999, 1), MQTTPort(plugin, id, std::string("stat/") + deviceID + "/STATUS8") {
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
            this->setValue(power * 1000ul);	// milliwatts
    } catch (Poco::Exception &e) {
            this->plugin->errorOccurred(this->ID() + ": Error parsing JSON: " + this->plugin->openhat->getExceptionMessage(e));
            this->setValue(-1);
    }


    this->lastReceiveTime = opdi_get_time_ms();
}

uint8_t TasmotaPower::doWork(uint8_t canSend) {
    opdi::DialPort::doWork(canSend);

    // time for refresh?
    if ((this->queryInterval > 0) && (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000)) {
        this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::QUERY, this));
    }

    Poco::Mutex::ScopedLock lock(this->mutex);
    // no error?
	if (this->getError() == Error::VALUE_OK) {
		// error timeout reached without message?
        if ((this->timeoutSeconds > 0) && (opdi_get_time_ms() - this->lastReceiveTime) / 1000 > this->timeoutSeconds) {
            // change to error state
            this->newValue = -1;
			this->setError(Error::VALUE_EXPIRED);
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

	this->client->publish(topic, payload, QOS, false);
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
	this->timeoutSeconds = DEFAULT_TIMEOUT;	// time without received payloads until the devices go into error mode

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

		int timeout = deviceConfig->getInt("Timeout", this->timeoutSeconds);

		int queryInterval = deviceConfig->getInt("QueryInterval", DEFAULT_QUERY_INTERVAL);
		if (queryInterval < 0)
			this->openhat->throwSettingException(deviceName + ": Please specify a QueryInterval >= 0: " + this->openhat->to_string(queryInterval));
		
		if (deviceType == "DigitalPort") {
			this->openhat->logVerbose(node + ": Setting up MQTT digital port: " + deviceName, this->logVerbosity);
			// get topic (required)
			std::string topic = this->openhat->getConfigString(deviceConfig, deviceName, "Topic", "", true);

			// setup the port instance and add it
			DigitalPort* digitalPort = new DigitalPort(this, deviceName, topic);
			digitalPort->timeoutSeconds = timeout;
			digitalPort->queryInterval = queryInterval;
			// set default group: MQTT's node's group
			digitalPort->setGroup(group);
			// set default log verbosity
			digitalPort->logVerbosity = this->logVerbosity;
			digitalPort->configure(deviceConfig);
			this->openhat->addPort(digitalPort);
			this->myPorts.push_back(digitalPort);
		}
		else if (deviceType == "DialPort") {
			this->openhat->logVerbose(node + ": Setting up MQTT dial port: " + deviceName, this->logVerbosity);
			// get topic (required)
			std::string topic = this->openhat->getConfigString(deviceConfig, deviceName, "Topic", "", true);

			// setup the port instance and add it
			DialPort* dialPort = new DialPort(this, deviceName, topic);
			dialPort->timeoutSeconds = timeout;
			dialPort->queryInterval = queryInterval;
			// set default group: MQTT's node's group
			dialPort->setGroup(group);
			// set default log verbosity
			dialPort->logVerbosity = this->logVerbosity;
			dialPort->configure(deviceConfig);
			this->openhat->addPort(dialPort);
			this->myPorts.push_back(dialPort);
		}
		else if (deviceType == "SelectPort") {
			this->openhat->logVerbose(node + ": Setting up MQTT select port: " + deviceName, this->logVerbosity);
			// get topic (required)
			std::string topic = this->openhat->getConfigString(deviceConfig, deviceName, "Topic", "", true);

			// setup the port instance and add it
			SelectPort* selectPort = new SelectPort(this, deviceName, topic);
			selectPort->timeoutSeconds = timeout;
			selectPort->queryInterval = queryInterval;
			// set default group: MQTT's node's group
			selectPort->setGroup(group);
			// set default log verbosity
			selectPort->logVerbosity = this->logVerbosity;
			selectPort->configure(deviceConfig, parentConfig);
			this->openhat->addPort(selectPort);
			this->myPorts.push_back(selectPort);
		}
		else if (deviceType == "Generic") {
			this->openhat->logVerbose(node + ": Setting up generic MQTT port: " + deviceName, this->logVerbosity);
            // get topic (required)
            std::string topic = this->openhat->getConfigString(deviceConfig, deviceName, "Topic", "", true);

			// setup the generic port instance and add it
			GenericPort* genericPort = new GenericPort(this, deviceName, topic);
			genericPort->timeoutSeconds = timeout;
			genericPort->queryInterval = queryInterval;
			// set default group: MQTT's node's group
			genericPort->setGroup(group);
			// set default log verbosity
			genericPort->logVerbosity = this->logVerbosity;
			this->openhat->configureCustomPort(deviceConfig, genericPort);
			this->openhat->addPort(genericPort);
			this->myPorts.push_back(genericPort);
		}
		else if (deviceType == "Event") {
			this->openhat->logVerbose(node + ": Setting up MQTT event port: " + deviceName, this->logVerbosity);
            // get topic (required)
            std::string topic = this->openhat->getConfigString(deviceConfig, deviceName, "Topic", "", true);

			// setup the generic port instance and add it
			EventPort* eventPort = new EventPort(this, deviceName, topic);
			eventPort->timeoutSeconds = timeout;
			eventPort->queryInterval = queryInterval;
			// set default group: MQTT's node's group
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
				// set default group: MQTT's node's group
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

		++nli;
	}

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
#ifdef _WIN32
				Sleep(10000);
#else
                struct timespec aSleep;
                struct timespec aRem;
				aSleep.tv_sec = 10;
				aSleep.tv_nsec = 0;
				nanosleep(&aSleep, &aRem);
#endif
			} else {
				// wait until connection established or timeout up
				uint64_t aTime = opdi_get_time_ms();
				while ((opdi_get_time_ms() - aTime < this->timeoutSeconds * 1000ul)
						&& !this->client->is_connected()) {
					this->openhat->logExtreme(this->nodeID + ": Waiting for connection...", this->logVerbosity);
#ifdef _WIN32
					Sleep(1);
#else
					struct timespec aSleep;
					struct timespec aRem;
					aSleep.tv_sec = 0;
					aSleep.tv_nsec = 1000000;
					nanosleep(&aSleep, &aRem);
#endif
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
								if ((*it)->queryInterval > 0) {
									// post query notification
									this->openhat->logExtreme(nodeID + ": Requesting query...", this->logVerbosity);
									queue.enqueueNotification(new ActionNotification(ActionNotification::QUERY, *it));
								}
								++it;
							}
							break;
						}
						case ActionNotification::QUERY:  {
							if (this->client != nullptr && this->client->is_connected()) {
								workNf->port->lastQueryTime = opdi_get_time_ms();
								workNf->port->query(this->client);
							}
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

Poco::JSON::Object::Ptr MQTTPlugin::GetObject(Poco::JSON::Object::Ptr jsonObject, const std::string& key) {
	Poco::JSON::Object::Ptr result = jsonObject->getObject(key);
	if (result.isNull())
		throw Poco::Exception("JSON value not found: " + key);
    return result;
}

std::string MQTTPlugin::GetString(Poco::JSON::Object::Ptr jsonObject, const std::string& key) {
    Poco::Dynamic::Var loVariable;
    std::string lsReturn;

    // Get the member Variable
    //
    loVariable = jsonObject->get(key);

    // Get the Value from the Variable
    //
    lsReturn = loVariable.convert<std::string>();

    return lsReturn;
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
