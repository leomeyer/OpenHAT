#include <sstream>

#include "Poco/Tuple.h"
#include "Poco/Runnable.h"
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/StreamCopier.h>
#include <Poco/Path.h>
#include <Poco/URI.h>
#include <Poco/Exception.h>
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

#define INVALID_SID		"0000000000000000"

// FRITZ!Box HTTP API: see https://avm.de/fileadmin/user_upload/Global/Service/Schnittstellen/AHA-HTTP-Interface.pdf

namespace {

class FritzBoxPlugin;

class FritzPort  {
friend class FritzBoxPlugin;
protected:
	std::string id;
	std::string ain;
	uint32_t queryInterval;
	uint64_t lastQueryTime;
	bool valueSet;
	Poco::Mutex mutex;
public:
	FritzPort(const std::string& id, const std::string& ain, int queryInterval) : id(id), ain(ain), queryInterval(queryInterval) {};
};

class ActionNotification : public Poco::Notification {
public:
	typedef Poco::AutoPtr<ActionNotification> Ptr;

	enum ActionType {
		LOGIN,
		LOGOUT,
		GETSWITCHSTATE,
		SETSWITCHSTATEHIGH,
		SETSWITCHSTATELOW,
		GETPOWER,
		GETENERGY,
		GETTEMPERATURE
	};

	ActionType type;
	FritzPort* port;

	ActionNotification(ActionType type, FritzPort* port);
};

ActionNotification::ActionNotification(ActionType type, FritzPort* port) {
	this->type = type;
	this->port = port;
}

////////////////////////////////////////////////////////////////////////
// Plugin main class
////////////////////////////////////////////////////////////////////////

class FritzBoxPlugin : public IOpenHATPlugin, public openhat::IConnectionListener, public Poco::Runnable {
	friend class FritzDECT200Switch;
	friend class FritzDECT200Power;
	friend class FritzDECT200Energy;
	friend class FritzDECT200Temperature;

protected:
	std::string nodeID;

	std::string host;
	int port;
	std::string user;
	std::string password;
	int timeoutSeconds;

	std::string sid;
	int errorCount;
	std::string lastErrorMessage;

	opdi::LogVerbosity logVerbosity;

	Poco::NotificationQueue queue;

	Poco::Thread workThread;

	void errorOccurred(const std::string& message);

public:
	openhat::AbstractOpenHAT* openhat;

	virtual std::string httpGet(const std::string& url);

	virtual std::string getResponse(const std::string& challenge, const std::string& password);

	virtual std::string getSessionID(const std::string& user, const std::string& password);

	virtual std::string getXMLValue(const std::string& xml, const std::string& node);

	virtual void login(void);

	virtual void checkLogin(void);

	virtual void logout(void);

	virtual void getSwitchState(FritzPort* port);

	virtual void setSwitchState(FritzPort* port, uint8_t line);

	virtual void getSwitchEnergy(FritzPort* port);

	virtual void getSwitchPower(FritzPort* port);

	virtual void getTemperature(FritzPort* port);

	virtual void masterConnected(void) override;
	virtual void masterDisconnected(void) override;

	virtual void run(void);

	virtual void setupPlugin(openhat::AbstractOpenHAT* abstractOpenHAT, const std::string& node, openhat::ConfigurationView* nodeConfig, openhat::ConfigurationView* parentConfig, const std::string& driverPath) override;
};

////////////////////////////////////////////////////////////////////////
// Plugin ports
////////////////////////////////////////////////////////////////////////

class FritzDECT200Switch : public opdi::DigitalPort, public FritzPort {
	friend class FritzBoxPlugin;
protected:
	FritzBoxPlugin* plugin;

	int8_t switchState;

	virtual void setSwitchState(int8_t line);

public:
	FritzDECT200Switch(FritzBoxPlugin* plugin, const std::string& id, const std::string& ain, int queryInterval);

	virtual void setLine(uint8_t line, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;

	virtual uint8_t doWork(uint8_t canSend) override;
};

class FritzDECT200Power : public opdi::DialPort, public FritzPort {
	friend class FritzBoxPlugin;
protected:
	FritzBoxPlugin* plugin;

	int32_t power;

	virtual void setPower(int32_t power);

public:
	FritzDECT200Power(FritzBoxPlugin* plugin, const std::string& id, const std::string& ain, int queryInterval);

	virtual uint8_t doWork(uint8_t canSend) override;
};

class FritzDECT200Energy : public opdi::DialPort, public FritzPort {
	friend class FritzBoxPlugin;
protected:
	FritzBoxPlugin* plugin;

	int32_t energy;

	virtual void setEnergy(int32_t energy);

public:
	FritzDECT200Energy(FritzBoxPlugin* plugin, const std::string& id, const std::string& ain, int queryInterval);

	virtual uint8_t doWork(uint8_t canSend) override;
};

class FritzDECT200Temperature : public opdi::DialPort, public FritzPort {
	friend class FritzBoxPlugin;
protected:
	FritzBoxPlugin* plugin;

	int32_t temperature;

	virtual void setTemperature(int32_t temperature);

public:
	FritzDECT200Temperature(FritzBoxPlugin* plugin, const std::string& id, const std::string& ain, int queryInterval);

	virtual uint8_t doWork(uint8_t canSend) override;
};

}	// end anonymous namespace

FritzDECT200Switch::FritzDECT200Switch(FritzBoxPlugin* plugin, const std::string& id, const std::string& ain, int queryInterval) : 
	opdi::DigitalPort(id.c_str(), OPDI_PORTDIRCAP_OUTPUT, 0), FritzPort(id, ain, queryInterval) {
	this->plugin = plugin;
	this->switchState = -1;	// unknown
	this->lastQueryTime = 0;
	this->valueSet = false;

	this->mode = OPDI_DIGITAL_MODE_OUTPUT;
	this->setIcon("powersocket");
}

uint8_t FritzDECT200Switch::doWork(uint8_t canSend) {
	opdi::DigitalPort::doWork(canSend);

	// time for refresh?
	if (opdi_get_time_ms() - this->lastQueryTime > this->queryInterval * 1000) {
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::GETSWITCHSTATE, this));
		this->lastQueryTime = opdi_get_time_ms();
	}

	Poco::Mutex::ScopedLock lock(this->mutex);

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

void FritzDECT200Switch::setLine(uint8_t line, ChangeSource changeSource) {
	opdi::DigitalPort::setLine(line, changeSource);

	if (line == 0)
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::SETSWITCHSTATELOW, this));
	else
	if (line == 1)
		this->plugin->queue.enqueueNotification(new ActionNotification(ActionNotification::SETSWITCHSTATEHIGH, this));
}

void FritzDECT200Switch::setSwitchState(int8_t switchState) {
	Poco::Mutex::ScopedLock lock(this->mutex);

	this->switchState = switchState;
	this->valueSet = true;
}

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

void FritzBoxPlugin::errorOccurred(const std::string& message) {
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

std::string FritzBoxPlugin::httpGet(const std::string& url) {
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

		return ss.str().erase(ss.str().find_last_not_of("\n") + 1);
	} catch (Poco::Exception &e) {
		this->errorOccurred(this->nodeID + ": Error during HTTP GET for " + uri.toString() + ": " + this->openhat->getExceptionMessage(e));
	}

	// avoid further processing if shutdown was requested in the mean time
	if (this->openhat->shutdownRequested)
		throw Poco::Exception("Shutdown occurred");

	return "";
}

std::string FritzBoxPlugin::getXMLValue(const std::string& xml, const std::string& node) {
	try {
		std::stringstream in(xml);
		Poco::XML::InputSource src(in);
		Poco::XML::DOMParser parser;
		Poco::AutoPtr<Poco::XML::Document> pDoc = parser.parse(&src);
		Poco::XML::NodeIterator it(pDoc, Poco::XML::NodeFilter::SHOW_ALL);
		Poco::XML::Node* pNode = it.nextNode();
		while (pNode)
		{
			if ((pNode->nodeName() == node) && (pNode->firstChild() != nullptr))
				return pNode->firstChild()->nodeValue();
			pNode = it.nextNode();
		}
	} catch (Poco::Exception &e) {
		throw Poco::Exception("Error parsing XML", e);
	}

	throw Poco::NotFoundException("Node or value not found in XML: " + node);
}

std::string FritzBoxPlugin::getResponse(const std::string& challenge, const std::string& password) {
	Poco::MD5Engine md5;
	// password is UTF8 internally; need to convert to UTF16LE
	std::string toHash = challenge + "-" + password;
	std::wstring toHashUTF16;
	Poco::UnicodeConverter::toUTF16(toHash, toHashUTF16);
	// replace chars > 255 with a dot (compatibility reasons)
	// convert to 16 bit types (can't use wstring directly because on Linux it's more than 16 bit wide)
	std::vector<uint16_t> charVec;
	wchar_t* c = (wchar_t*)&toHashUTF16.c_str()[0];
	while (*c) {
		if (*c > 255)
			*c = '.';
		charVec.push_back((uint16_t)*c);
		c++;
	}
	// twice the string length (each char has 2 bytes)
	md5.update(charVec.data(), charVec.size() * 2);
	const Poco::DigestEngine::Digest& digest = md5.digest(); // obtain result
	return challenge + "-" + Poco::DigestEngine::digestToHex(digest);
}

std::string FritzBoxPlugin::getSessionID(const std::string& user, const std::string& password) {
	std::string loginPage = this->httpGet("/login_sid.lua?sid=" + this->sid);
	if (loginPage.empty())
		return INVALID_SID;
	std::string sid = getXMLValue(loginPage, "SID");
	if (sid == INVALID_SID) {
		std::string challenge = getXMLValue(loginPage, "Challenge");
		std::string uri = "/login_sid.lua?username=" + user + "&response=" + this->getResponse(challenge, password);
		loginPage = this->httpGet(uri);
		if (loginPage.empty())
			return INVALID_SID;
		sid = getXMLValue(loginPage,  "SID");
	}
	return sid;
}

void FritzBoxPlugin::login(void) {
	this->openhat->logDebug(this->nodeID + ": Attempting to login at " + this->host + " with user " + this->user, this->logVerbosity);

	this->sid = this->getSessionID(this->user, this->password);

	if (sid == INVALID_SID) {
		this->errorOccurred(this->nodeID + ": Login at " + this->host + " with user " + this->user + " failed");
		return;
	}
	// one normal message after failed attempts
	if (this->errorCount > 0) {
		this->openhat->logNormal(this->nodeID + ": Login at " + this->host + " with user " + this->user + " successful; sid = " + this->sid, this->logVerbosity);
		this->errorCount = 0;
	} else
		this->openhat->logDebug(this->nodeID + ": Login at " + this->host + " with user " + this->user + " successful; sid = " + this->sid, this->logVerbosity);
}

void FritzBoxPlugin::checkLogin(void) {
	// check whether we're currently logged in
	std::string loginPage = this->httpGet("/login_sid.lua?sid=" + this->sid);

	// error case
	if (loginPage.empty()) {
		this->sid = INVALID_SID;
		return;
	}
	this->sid = getXMLValue(loginPage, "SID");
	if (sid == INVALID_SID) {
		// try to log in
		this->login();
	}
}

void FritzBoxPlugin::logout(void) {
	// not connected?
	if (this->sid == INVALID_SID)
		return;

	this->openhat->logDebug(this->nodeID + ": Logout from " + this->host, this->logVerbosity);

	this->httpGet("/login_sid.lua?logout=true&sid=" + this->sid);


	this->sid = INVALID_SID;
}

void FritzBoxPlugin::getSwitchState(FritzPort* port) {
	// port must be a DECT 200 switch port
	FritzDECT200Switch* switchPort = (FritzDECT200Switch*)port;

	this->checkLogin();
	// problem?
	if (this->sid == INVALID_SID) {
		switchPort->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
		return;
	}

	// query the FritzBox
	std::string result = httpGet("/webservices/homeautoswitch.lua?ain=" + switchPort->ain + "&switchcmd=getswitchstate&sid=" + this->sid);
	// problem?
	if (result.empty()) {
		switchPort->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
		return;
	}

	if (result == "1") {
		switchPort->setSwitchState(1);
	} else
	if (result == "0") {
		switchPort->setSwitchState(0);
	} else
		switchPort->setSwitchState(-1);
}

void FritzBoxPlugin::setSwitchState(FritzPort* port, uint8_t line) {
	// port must be a DECT 200 switch port
	FritzDECT200Switch* switchPort = (FritzDECT200Switch*)port;

	this->checkLogin();
	// problem?
	if (this->sid == INVALID_SID) {
		switchPort->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
		return;
	}

	std::string cmd = (line == 1 ? "setswitchon" : "setswitchoff");
	std::string result = this->httpGet("/webservices/homeautoswitch.lua?ain=" + switchPort->ain + "&switchcmd=" + cmd + "&sid=" + this->sid);
	// problem?
	if (result.empty()) {
		switchPort->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
		return;
	}

	if (result == "1") {
		switchPort->setSwitchState(1);
	} else
	if (result == "0") {
		switchPort->setSwitchState(0);
	} else
		switchPort->setSwitchState(-1);
}

void FritzBoxPlugin::getSwitchEnergy(FritzPort* port) {
	// port must be a DECT 200 energy port
	FritzDECT200Energy* energyPort = (FritzDECT200Energy*)port;

	this->checkLogin();

	// problem?
	if (this->sid == INVALID_SID) {
		energyPort->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
		return;
	}

	// query the FritzBox
	std::string result = httpGet("/webservices/homeautoswitch.lua?ain=" + energyPort->ain + "&switchcmd=getswitchenergy&sid=" + this->sid);
	// problem?
	if (result.empty()) {
		energyPort->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
		return;
	}

	// parse result
	int energy = -1;
	Poco::NumberParser::tryParse(result, energy);

	energyPort->setEnergy(energy);
}

void FritzBoxPlugin::getSwitchPower(FritzPort* port) {
	// port must be a DECT 200 power port
	FritzDECT200Power* powerPort = (FritzDECT200Power*)port;

	this->checkLogin();
	// problem?
	if (this->sid == INVALID_SID) {
		powerPort->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
		return;
	}

	// query the FritzBox
	std::string result = httpGet("/webservices/homeautoswitch.lua?ain=" + powerPort->ain + "&switchcmd=getswitchpower&sid=" + this->sid);
	// problem?
	if (result.empty()) {
		powerPort->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
		return;
	}

	// parse result
	int power = -1;
	Poco::NumberParser::tryParse(result, power);

	powerPort->setPower(power);
}

void FritzBoxPlugin::getTemperature(FritzPort* port) {
	// port must be a DECT 200 temperature port
	FritzDECT200Temperature* tempPort = (FritzDECT200Temperature*)port;

	this->checkLogin();
	// problem?
	if (this->sid == INVALID_SID) {
		tempPort->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
		return;
	}

	// query the FritzBox
	std::string result = httpGet("/webservices/homeautoswitch.lua?ain=" + tempPort->ain + "&switchcmd=gettemperature&sid=" + this->sid);
	// problem?
	if (result.empty()) {
		tempPort->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
		return;
	}

	// parse result
	int temp = -9999;
	Poco::NumberParser::tryParse(result, temp);

	tempPort->setTemperature(temp);
}

void FritzBoxPlugin::setupPlugin(openhat::AbstractOpenHAT* abstractOpenHAT, const std::string& node, openhat::ConfigurationView* nodeConfig, openhat::ConfigurationView* parentConfig, const std::string& /*driverPath*/) {
	this->openhat = abstractOpenHAT;
	this->nodeID = node;
	this->sid = INVALID_SID;			// default; means not connected
	this->timeoutSeconds = 5;			// short timeout (assume local network)

	this->errorCount = 0;

	// test case for response calculation (see documentation: http://avm.de/fileadmin/user_upload/Global/Service/Schnittstellen/AVM_Technical_Note_-_Session_ID.pdf)
	// this->openhat->log("Test Response: " + this->getResponse("1234567z", "äbc"));

	// get host and credentials
	this->host = this->openhat->getConfigString(nodeConfig, node, "Host", "", true);
	this->port = nodeConfig->getInt("Port", 80);
	this->user = this->openhat->getConfigString(nodeConfig, node, "User", "", true);
	this->password = this->openhat->getConfigString(nodeConfig, node, "Password", "", true);
	this->timeoutSeconds = nodeConfig->getInt("Timeout", this->timeoutSeconds);

	// store main node's group (will become the default of ports)
	std::string group = nodeConfig->getString("Group", "");

	// store main node's verbosity level (will become the default of ports)
	this->logVerbosity = this->openhat->getConfigLogVerbosity(nodeConfig, this->openhat->getLogVerbosity());

	// enumerate keys of the plugin's nodes (in specified order)
	this->openhat->logVerbose(node + ": Enumerating FritzBox devices: " + node + ".Devices", this->logVerbosity);

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

		this->openhat->logVerbose(node + ": Setting up FritzBoxPlugin device: " + deviceName, this->logVerbosity);

		// get device section from the configuration>
		Poco::AutoPtr<openhat::ConfigurationView> deviceConfig = this->openhat->createConfigView(parentConfig, deviceName);

		// get device type (required)
		std::string deviceType = this->openhat->getConfigString(deviceConfig, deviceName, "Type", "", true);
		int queryInterval = deviceConfig->getInt("QueryInterval", 30);
		if (queryInterval < 1)
			this->openhat->throwSettingException(deviceName + ": Please specify a QueryInterval greater than 1: " + this->openhat->to_string(queryInterval));

		if (deviceType == "FritzDECT200") {
			// get AIN (required)
			std::string ain = this->openhat->getConfigString(deviceConfig, deviceName, "AIN", "", true);

			if (parentConfig->hasProperty(deviceName + "_Switch.Type")) {
				this->openhat->logVerbose(node + ": Setting up FritzBoxPlugin device port: " + deviceName + "_Switch", this->logVerbosity);
				Poco::AutoPtr<openhat::ConfigurationView> portConfig = this->openhat->createConfigView(parentConfig, deviceName + "_Switch");
				// check type
				if (portConfig->getString("Type") != "DigitalPort")
					this->openhat->throwSettingException(node + ": FritzDECT200 Switch device port must be of type 'DigitalPort'");
				// setup the switch port instance and add it
				FritzDECT200Switch* switchPort = new FritzDECT200Switch(this, deviceName + "_Switch", ain, queryInterval);
				// set default group: FritzBox's node's group
				switchPort->setGroup(group);
				// set default log verbosity
				switchPort->logVerbosity = this->logVerbosity;
				// allow only basic settings to be changed
				this->openhat->configurePort(portConfig, switchPort, 0);
				this->openhat->addPort(switchPort);
			} else {
				this->openhat->logVerbose(node + ": FritzBoxPlugin device port: " + deviceName + "_Switch.Type not found, ignoring", this->logVerbosity);
			}

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

		++nli;
	}

	// this->openhat->addConnectionListener(this);

	this->workThread.setName(node + " work thread");
	this->workThread.start(*this);

	this->openhat->logVerbose(node + ": FritzBoxPlugin setup completed successfully", this->logVerbosity);
}

void FritzBoxPlugin::masterConnected() {
}

void FritzBoxPlugin::masterDisconnected() {
}

void FritzBoxPlugin::run(void) {

	this->openhat->logVerbose(this->nodeID + ": FritzBoxPlugin worker thread started", this->logVerbosity);

	while (!this->openhat->shutdownRequested) {
		try {
			Poco::Notification::Ptr notification = this->queue.waitDequeueNotification(100);
			if (!this->openhat->shutdownRequested && notification) {
				ActionNotification::Ptr workNf = notification.cast<ActionNotification>();
				if (workNf) {
					std::string action;
					switch (workNf->type) {
					case ActionNotification::LOGIN: action = "LOGIN"; break;
					case ActionNotification::LOGOUT: action = "LOGOUT"; break;
					case ActionNotification::GETSWITCHSTATE: action = "GETSWITCHSTATE"; break;
					case ActionNotification::SETSWITCHSTATELOW: action = "SETSWITCHSTATELOW"; break;
					case ActionNotification::SETSWITCHSTATEHIGH: action = "SETSWITCHSTATEHIGH"; break;
					case ActionNotification::GETENERGY: action = "GETENERGY"; break;
					case ActionNotification::GETPOWER: action = "GETPOWER"; break;
					case ActionNotification::GETTEMPERATURE: action = "GETTEMPERATURE"; break;
					}
					FritzPort* fp = (FritzPort*)workNf->port;
					std::string portID = fp->id;
					this->openhat->logDebug(this->nodeID + ": FritzBoxPlugin processing requested action: " + action + " for port: " + portID, this->logVerbosity);
					// inspect action and decide what to do
					switch (workNf->type) {
					case ActionNotification::LOGIN: this->login(); break;
					case ActionNotification::LOGOUT: this->logout(); break;
					case ActionNotification::GETSWITCHSTATE: this->getSwitchState(workNf->port); break;
					case ActionNotification::SETSWITCHSTATELOW: this->setSwitchState(workNf->port, 0); break;
					case ActionNotification::SETSWITCHSTATEHIGH: this->setSwitchState(workNf->port, 1); break;
					case ActionNotification::GETENERGY: this->getSwitchEnergy(workNf->port); break;
					case ActionNotification::GETPOWER: this->getSwitchPower(workNf->port); break;
					case ActionNotification::GETTEMPERATURE: this->getTemperature(workNf->port); break;
					}
				}
			}
		} catch (Poco::Exception &e) {
			this->openhat->logNormal(this->nodeID + ": Unhandled exception in worker thread: " + this->openhat->getExceptionMessage(e), this->logVerbosity);
		}
	}

	this->openhat->logVerbose(this->nodeID + ": FritzBoxPlugin worker thread terminated", this->logVerbosity);
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
	return new FritzBoxPlugin();
}
