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
#include "Poco/DigestStream.h"
#include "Poco/MD5Engine.h"
#include "Poco/UTF16Encoding.h"
#include "Poco/UnicodeConverter.h"
#include "Poco/Notification.h"
#include "Poco/NotificationQueue.h"
#include "Poco/AutoPtr.h"
#include "Poco/NumberParser.h"
#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Dynamic/Var.h>

#include "AbstractOpenHAT.h"

namespace {
class FroniusPlugin;

class FroniusPort  {
friend class FroniusPlugin;
protected:
	FroniusPlugin *plugin;
	bool valueSet;
	uint64_t lastQueryTime;
	uint32_t queryInterval;
	uint64_t timeoutCounter;
	std::string parameters;
public:
	std::string pid;
	
	FroniusPort(FroniusPlugin *plugin, const std::string& id, const std::string& parameters) {
		this->plugin = plugin;
		this->pid = id;
		this->parameters = parameters;
		this->valueSet = false;
		this->lastQueryTime = 0;
		this->queryInterval = 30;
		this->timeoutCounter = 0;
	};
};

class ActionNotification : public Poco::Notification {
public:
	typedef Poco::AutoPtr<ActionNotification> Ptr;

	enum ActionType {
		GETPOWER,
		GETDAYENERGY
	};

	ActionType type;
	FroniusPort* port;

	ActionNotification(ActionType type, FroniusPort* port);
};

ActionNotification::ActionNotification(ActionType type, FroniusPort* port) {
	this->type = type;
	this->port = port;
}

////////////////////////////////////////////////////////////////////////
// Plugin main class
////////////////////////////////////////////////////////////////////////

class FroniusPlugin : public IOpenHATPlugin, public openhat::IConnectionListener, public Poco::Runnable
{
	friend class CurrentSolarPower;
protected:
	
	std::vector<FroniusPort*> myPorts;

public:

	std::string nodeID;

	std::string host;
	int port;
	std::string user;
	std::string password;
	int timeoutSeconds;
	int QOS;

	std::string sid;
	int errorCount;
	std::string lastErrorMessage;

	opdi::LogVerbosity logVerbosity;

	Poco::NotificationQueue queue;
	Poco::Mutex mutex;
	Poco::Thread workThread;
	
	bool terminateRequested;
	
	virtual std::string httpGet(const std::string& url);

	virtual void getCurrentPower(FroniusPort* port);

	virtual void getDayEnergy(FroniusPort* port);
	
	void errorOccurred(const std::string& message);
	
	Poco::JSON::Object::Ptr GetObject(Poco::JSON::Object::Ptr jsonObject, const std::string& key);

	std::string GetString(Poco::JSON::Object::Ptr jsonObject, const std::string& key);

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
class CurrentSolarPower : public opdi::DialPort, public FroniusPort {
	friend class FroniusPlugin;
protected:

	int32_t power;

	virtual void setPower(int32_t power);

public:
	CurrentSolarPower(FroniusPlugin* plugin, const std::string& id, const std::string& parameters);

	virtual uint8_t doWork(uint8_t canSend) override;
};


class DayEnergy : public opdi::DialPort, public FroniusPort {
	friend class FroniusPlugin;
protected:

	int32_t energy;

	virtual void setEnergy(int32_t energy);

public:
	DayEnergy(FroniusPlugin* plugin, const std::string& id, const std::string& parameters);

	virtual uint8_t doWork(uint8_t canSend) override;
};

}	// end anonymous namespace


CurrentSolarPower::CurrentSolarPower(FroniusPlugin* plugin, const std::string& id, const std::string& parameters) : 
	opdi::DialPort(id.c_str()), FroniusPort(plugin, id, parameters) {
	this->plugin = plugin;
	this->power = -1;	// unknown
	this->lastQueryTime = 0;
	this->valueSet = true;		// causes setError in doWork

	this->minValue = 0;
	this->maxValue = 999999999;	// measured in W
	this->step = 1;
	this->position = 0;

	this->setUnit("electricPower_mW");
	this->setIcon("powermeter");

	// port is readonly
	this->setReadonly(true);
}

uint8_t CurrentSolarPower::doWork(uint8_t canSend) {
	opdi::DialPort::doWork(canSend);

	// time for refresh?
	if (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::GETPOWER, this));
		this->lastQueryTime = opdi_get_time_ms();
	}

	Poco::Mutex::ScopedLock lock(this->plugin->mutex);

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

void CurrentSolarPower::setPower(int32_t power) {
	Poco::Mutex::ScopedLock lock(this->plugin->mutex);

	this->power = power;
	this->valueSet = true;
}


DayEnergy::DayEnergy(FroniusPlugin* plugin, const std::string& id, const std::string& parameters) : 
	opdi::DialPort(id.c_str()), FroniusPort(plugin, id, parameters) {
	this->plugin = plugin;
	this->energy = -1;	// unknown
	this->lastQueryTime = 0;
	this->valueSet = true;		// causes setError in doWork

	this->minValue = 0;
	this->maxValue = 999999999;	// measured in Wh
	this->step = 1;
	this->position = 0;

	this->setUnit("electricEnergy_Wh");
	this->setIcon("energymeter");

	// port is readonly
	this->setReadonly(true);
}

uint8_t DayEnergy::doWork(uint8_t canSend) {
	opdi::DialPort::doWork(canSend);

	// time for refresh?
	if (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::GETDAYENERGY, this));
		this->lastQueryTime = opdi_get_time_ms();
	}

	Poco::Mutex::ScopedLock lock(this->plugin->mutex);

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

void DayEnergy::setEnergy(int32_t energy) {
	Poco::Mutex::ScopedLock lock(this->plugin->mutex);

	this->energy = energy;
	this->valueSet = true;
}

////////////////////////////////////////////////////////
// Plugin implementation
////////////////////////////////////////////////////////

Poco::JSON::Object::Ptr FroniusPlugin::FroniusPlugin::GetObject(Poco::JSON::Object::Ptr jsonObject, const std::string& key) {
	Poco::JSON::Object::Ptr result = jsonObject->getObject(key);
	if (result == nullptr)
		throw Poco::Exception("JSON value not found: " + key);
    return result;
}

std::string FroniusPlugin::GetString(Poco::JSON::Object::Ptr jsonObject, const std::string& key) {
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


std::string FroniusPlugin::httpGet(const std::string& url) {
	Poco::URI uri(std::string("http://") + this->host + ":" + this->openhat->to_string(this->port) + url);
	try {
		// prepare session
		Poco::Net::HTTPClientSession session(uri.getHost(), uri.getPort());
		session.setTimeout(Poco::Timespan(this->timeoutSeconds, 0));

		// prepare path
		std::string path(uri.getPathAndQuery());
		if (path.empty()) path = "/";

		this->openhat->logDebug(this->nodeID + ": HTTP GET: " + uri.toString(), this->logVerbosity);

		// send request
		Poco::Net::HTTPRequest req(Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
		session.sendRequest(req);

		// get response
		Poco::Net::HTTPResponse res;

		// print response
		std::istream &is = session.receiveResponse(res);

		if (res.getStatus() != 200) {
			this->errorOccurred(this->nodeID + ": HTTP GET: " + uri.toString() + ": The server returned an error: " + this->openhat->to_string(res.getStatus()) + " " + res.getReason());
			return "";
		} else
			this->openhat->logDebug(this->nodeID + ": HTTP Response: " + this->openhat->to_string(res.getStatus()) + " " + res.getReason(), this->logVerbosity);

		std::stringstream ss;

		Poco::StreamCopier::copyStream(is, ss);

		if (this->errorCount > 0) {
			this->openhat->logNormal(this->nodeID + ": HTTP GET successful: " + uri.toString(), this->logVerbosity);
			this->errorCount = 0;
		} else
			this->openhat->logDebug(this->nodeID + ": HTTP GET successful: " + uri.toString(), this->logVerbosity);

		std::string result = ss.str().erase(ss.str().find_last_not_of("\n") + 1);
		this->openhat->logExtreme(this->nodeID + ": HTTP result: " + result, this->logVerbosity);
		return result;
	} catch (Poco::Exception &e) {
		this->errorOccurred(this->nodeID + ": Error during HTTP GET for " + uri.toString() + ": " + this->openhat->getExceptionMessage(e));
	}

	// avoid further processing if shutdown was requested in the mean time
	if (this->openhat->shutdownRequested)
		throw Poco::Exception("Shutdown occurred");

	return "";
}


void FroniusPlugin::getCurrentPower(FroniusPort* port) {
	// port must be a CurrentSolarPower port
	CurrentSolarPower* powerPort = (CurrentSolarPower*)port;

	// query the inverter
	std::string result = httpGet("/solar_api/v1/GetInverterRealtimeData.cgi?" + powerPort->parameters);
	
	// problem?
	if (result.empty()) {
		powerPort->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
		return;
	}

	// parse result JSON
	try {
		Poco::JSON::Parser parser;
		Poco::Dynamic::Var loParsedJson = parser.parse(result);
		Poco::Dynamic::Var jsonResult = parser.result();

		Poco::JSON::Object::Ptr root = jsonResult.extract<Poco::JSON::Object::Ptr>();
		Poco::JSON::Object::Ptr body = this->GetObject(root, "Body");
		Poco::JSON::Object::Ptr data = this->GetObject(body, "Data");
		Poco::JSON::Object::Ptr pac = this->GetObject(data, "PAC");
		Poco::JSON::Object::Ptr values = this->GetObject(pac, "Values");
		std::string value = this->GetString(values, "1");
		// parse value
		int power = -1;
		Poco::NumberParser::tryParse(value, power);
		powerPort->setPower(power * 1000);	// milliwatts
	} catch (Poco::Exception &e) {
		this->errorOccurred(this->nodeID + ": Error parsing JSON: " + this->openhat->getExceptionMessage(e));
		powerPort->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
	}
}

void FroniusPlugin::getDayEnergy(FroniusPort* port) {
	// port must be a DayEnergy port
	DayEnergy* energyPort = (DayEnergy*)port;

	// query the inverter
	std::string result = httpGet("/solar_api/v1/GetInverterRealtimeData.cgi?" + energyPort->parameters);
	
	// problem?
	if (result.empty()) {
		energyPort->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
		return;
	}

	// parse result JSON
	try {
		Poco::JSON::Parser parser;
		Poco::Dynamic::Var loParsedJson = parser.parse(result);
		Poco::Dynamic::Var jsonResult = parser.result();

		Poco::JSON::Object::Ptr root = jsonResult.extract<Poco::JSON::Object::Ptr>();
		Poco::JSON::Object::Ptr body = this->GetObject(root, "Body");
		Poco::JSON::Object::Ptr data = this->GetObject(body, "Data");
		Poco::JSON::Object::Ptr pac = this->GetObject(data, "DAY_ENERGY");
		Poco::JSON::Object::Ptr values = this->GetObject(pac, "Values");
		std::string value = this->GetString(values, "1");
		// parse value
		int energy = -1;
		Poco::NumberParser::tryParse(value, energy);
		energyPort->setEnergy(energy);	// watt hours
	} catch (Poco::Exception &e) {
		this->errorOccurred(this->nodeID + ": Error parsing JSON: " + this->openhat->getExceptionMessage(e));
		energyPort->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
	}
}


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

void FroniusPlugin::errorOccurred(const std::string& message) {
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

void FroniusPlugin::setupPlugin(openhat::AbstractOpenHAT* abstractOpenHAT, const std::string& node, openhat::ConfigurationView::Ptr nodeConfig, openhat::ConfigurationView::Ptr parentConfig, const std::string& /*driverPath*/) {
	this->openhat = abstractOpenHAT;
	this->nodeID = node;
	this->timeoutSeconds = 30;	// time without received payloads until the devices go into error mode

	this->errorCount = 0;

	// get host and credentials
	this->host = this->openhat->getConfigString(nodeConfig, node, "Host", "", true);
	this->port = nodeConfig->getInt("Port", 80);
	this->user = this->openhat->getConfigString(nodeConfig, node, "User", "", false);
	this->password = this->openhat->getConfigString(nodeConfig, node, "Password", "", false);
	this->timeoutSeconds = nodeConfig->getInt("Timeout", this->timeoutSeconds);
	
	this->terminateRequested = false;

	// store main node's group (will become the default of ports)
	std::string group = nodeConfig->getString("Group", "");

	// store main node's verbosity level (will become the default of ports)
	this->logVerbosity = this->openhat->getConfigLogVerbosity(nodeConfig, this->openhat->getLogVerbosity());

	// enumerate keys of the plugin's nodes (in specified order)
	this->openhat->logVerbose(node + ": Enumerating Fronius devices: " + node + ".Devices", this->logVerbosity);

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

		this->openhat->logVerbose(node + ": Setting up Fronius device: " + deviceName, this->logVerbosity);

		// get device section from the configuration>
		Poco::AutoPtr<openhat::ConfigurationView> deviceConfig = this->openhat->createConfigView(parentConfig, deviceName);

		// get device type (required)
		std::string deviceType = this->openhat->getConfigString(deviceConfig, deviceName, "Type", "", true);

		if (deviceType == "System") {
			this->openhat->logVerbose(node + ": Setting up Fronius system device port: " + deviceName + "_Power", this->logVerbosity);

			if (parentConfig->hasProperty(deviceName + "_Power.Type")) {
				Poco::AutoPtr<openhat::ConfigurationView> portConfig = this->openhat->createConfigView(parentConfig, deviceName + "_Power");
				// check type
				if (portConfig->getString("Type") != "DialPort")
					this->openhat->throwSettingException(node + ": Fronius Power device port must be of type 'DialPort'");
				// setup the switch port instance and add it
				CurrentSolarPower* powerPort = new CurrentSolarPower(this, deviceName + "_Power", "Scope=System");
				// set default group: node's group
				powerPort->setGroup(group);
				// set default log verbosity
				powerPort->logVerbosity = this->logVerbosity;
				// allow only basic settings to be changed
				this->openhat->configurePort(portConfig, powerPort, 0);
				int queryInterval = portConfig->getInt("QueryInterval", 0);
				if (queryInterval > 0)
					powerPort->queryInterval = queryInterval;

				this->openhat->addPort(powerPort);
				this->myPorts.push_back(powerPort);
			} else {
				this->openhat->logVerbose(node + ": Fronius Power device port: " + deviceName + "_Power.Type not found, ignoring", this->logVerbosity);
			}

			this->openhat->logVerbose(node + ": Setting up Fronius system device port: " + deviceName + "_Power", this->logVerbosity);

			if (parentConfig->hasProperty(deviceName + "_DayEnergy.Type")) {
				Poco::AutoPtr<openhat::ConfigurationView> portConfig = this->openhat->createConfigView(parentConfig, deviceName + "_DayEnergy");
				// check type
				if (portConfig->getString("Type") != "DialPort")
					this->openhat->throwSettingException(node + ": Fronius DayEnergy device port must be of type 'DialPort'");
				// setup the switch port instance and add it
				DayEnergy* energyPort = new DayEnergy(this, deviceName + "_DayEnergy", "Scope=System");
				// set default group: node's group
				energyPort->setGroup(group);
				// set default log verbosity
				energyPort->logVerbosity = this->logVerbosity;
				// allow only basic settings to be changed
				this->openhat->configurePort(portConfig, energyPort, 0);
				int queryInterval = portConfig->getInt("QueryInterval", 0);
				if (queryInterval > 0)
					energyPort->queryInterval = queryInterval;

				this->openhat->addPort(energyPort);
				this->myPorts.push_back(energyPort);
			} else {
				this->openhat->logVerbose(node + ": Fronius DayEnergy device port: " + deviceName + "_DayEnergy.Type not found, ignoring", this->logVerbosity);
			}
		} else
			this->openhat->throwSettingException(node + ": This plugin does not support the device type '" + deviceType + "'");
		
		
		++nli;
	}

	// this->openhat->addConnectionListener(this);

	this->workThread.setName(node + " work thread");
	this->workThread.start(*this);

	this->openhat->logVerbose(node + ": FroniusPlugin setup completed successfully", this->logVerbosity);
}

void FroniusPlugin::masterConnected() {
}

void FroniusPlugin::masterDisconnected() {
}

void FroniusPlugin::run(void) {

	this->openhat->logVerbose(this->nodeID + ": FroniusPlugin worker thread started", this->logVerbosity);

	while (!this->openhat->shutdownRequested && !this->terminateRequested) {
		try {
			Poco::Notification::Ptr notification = this->queue.waitDequeueNotification(100);
			if (!this->openhat->shutdownRequested && notification) {
				ActionNotification::Ptr workNf = notification.cast<ActionNotification>();
				if (workNf) {
					std::string action;
					switch (workNf->type) {
					case ActionNotification::GETPOWER: action = "GETPOWER"; break;
					case ActionNotification::GETDAYENERGY: action = "GETDAYENERGY"; break;
					}
					FroniusPort* port = (FroniusPort*)workNf->port;
					if (port == nullptr)
						this->openhat->logDebug(this->nodeID + ": Processing requested action: " + action, this->logVerbosity);
					else
						this->openhat->logDebug(this->nodeID + ": Processing requested action: " + action + " for port: " + port->pid, this->logVerbosity);
					// inspect action and decide what to do
					switch (workNf->type) {
					case ActionNotification::GETPOWER:
						this->getCurrentPower(port);
						break;
					case ActionNotification::GETDAYENERGY:
						this->getDayEnergy(port);
						break;
					}
				}
			}
		} catch (Poco::Exception &e) {
			this->openhat->logNormal(this->nodeID + ": Unhandled exception in worker thread: " + this->openhat->getExceptionMessage(e), this->logVerbosity);
		}
	}

	this->openhat->logVerbose(this->nodeID + ": FroniusPlugin worker thread terminated", this->logVerbosity);
}

void FroniusPlugin::terminate() {
	this->terminateRequested = true;
	while (this->workThread.isRunning())
		Poco::Thread::current()->yield();
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
	return new FroniusPlugin();
}
