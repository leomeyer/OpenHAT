#include "AbstractOpenHAT.h"

#include <vector>
#include <time.h>

#include "Poco/Exception.h"
#include "Poco/Tuple.h"
#include "Poco/Timestamp.h"
#include "Poco/Timezone.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DateTimeParser.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/Mutex.h"
#include "Poco/SimpleFileChannel.h"
#include "Poco/UTF8String.h"
#include "Poco/FileStream.h"
#include "Poco/Process.h"
#include "Poco/Util/LayeredConfiguration.h"
#include "Poco/NumberParser.h"
#include "Poco/RegularExpression.h"

#include "opdi_constants.h"

#include "OPDI_Ports.h"
#include "Ports.h"
#include "TimerPort.h"
#include "ExpressionPort.h"
#include "ExecPort.h"

#define DEFAULT_IDLETIMEOUT_MS	180000
#define DEFAULT_TCP_PORT		13110

#define SUPPORTED_PROTOCOLS	"EP,BP"

// global device flags
uint16_t opdi_device_flags = 0;

#define ENCRYPTION_METHOD_MAXLENGTH		16
char opdi_encryption_method[(ENCRYPTION_METHOD_MAXLENGTH + 1)];
#define ENCRYPTION_KEY_MAXLENGTH		16
char opdi_encryption_key[(ENCRYPTION_KEY_MAXLENGTH + 1)];

#define OPDI_ENCRYPTION_BLOCKSIZE	16
const uint16_t opdi_encryption_blocksize = OPDI_ENCRYPTION_BLOCKSIZE;

#ifdef _MSC_VER
unsigned char opdi_encryption_buffer[OPDI_ENCRYPTION_BLOCKSIZE];
unsigned char opdi_encryption_buffer_2[OPDI_ENCRYPTION_BLOCKSIZE];
#endif

// slave protocol callback
void protocol_callback(uint8_t state) {
	Opdi->protocolCallback(state);
}

namespace openhat {

AbstractOpenHAT::AbstractOpenHAT(void) {
	this->majorVersion = OPENHAT_MAJOR_VERSION;
	this->minorVersion = OPENHAT_MINOR_VERSION;
	this->patchVersion = OPENHAT_PATCH_VERSION;

	this->logVerbosity = opdi::LogVerbosity::UNKNOWN;
	this->persistentConfig = nullptr;

	this->logger = nullptr;
	this->timestampFormat = "%Y-%m-%d %H:%M:%S.%i";

	this->monSecondPos = 0;
	this->monSecondStats = (uint64_t*)malloc(this->maxSecondStats * sizeof(uint64_t));
	this->totalMicroseconds = 0;
	this->targetFramesPerSecond = 200;
	this->waitingCallsPerSecond = 0;
	this->framesPerSecond = 0;
	this->allowHiddenPorts = true;

	// map result codes
	opdiCodeTexts[0] = "STATUS_OK";
	opdiCodeTexts[1] = "DISCONNECTED";
	opdiCodeTexts[2] = "TIMEOUT";
	opdiCodeTexts[3] = "CANCELLED";
	opdiCodeTexts[4] = "ERROR_MALFORMED_MESSAGE";
	opdiCodeTexts[5] = "ERROR_CONVERSION";
	opdiCodeTexts[6] = "ERROR_MSGBUF_OVERFLOW";
	opdiCodeTexts[7] = "ERROR_DEST_OVERFLOW";
	opdiCodeTexts[8] = "ERROR_STRINGS_OVERFLOW";
	opdiCodeTexts[9] = "ERROR_PARTS_OVERFLOW";
	opdiCodeTexts[10] = "PROTOCOL_ERROR";
	opdiCodeTexts[11] = "PROTOCOL_NOT_SUPPORTED";
	opdiCodeTexts[12] = "ENCRYPTION_NOT_SUPPORTED";
	opdiCodeTexts[13] = "ENCRYPTION_REQUIRED";
	opdiCodeTexts[14] = "ENCRYPTION_ERROR";
	opdiCodeTexts[15] = "AUTH_NOT_SUPPORTED";
	opdiCodeTexts[16] = "AUTHENTICATION_EXPECTED";
	opdiCodeTexts[17] = "AUTHENTICATION_FAILED";
	opdiCodeTexts[18] = "DEVICE_ERROR";
	opdiCodeTexts[19] = "TOO_MANY_PORTS";
	opdiCodeTexts[20] = "PORTTYPE_UNKNOWN";
	opdiCodeTexts[21] = "PORT_UNKNOWN";
	opdiCodeTexts[22] = "WRONG_PORT_TYPE";
	opdiCodeTexts[23] = "TOO_MANY_BINDINGS";
	opdiCodeTexts[24] = "NO_BINDING";
	opdiCodeTexts[25] = "CHANNEL_INVALID";
	opdiCodeTexts[26] = "POSITION_INVALID";
	opdiCodeTexts[27] = "NETWORK_ERROR";
	opdiCodeTexts[28] = "TERMINATOR_IN_PAYLOAD";
	opdiCodeTexts[29] = "PORT_ACCESS_DENIED";
	opdiCodeTexts[30] = "PORT_ERROR";
	opdiCodeTexts[31] = "SHUTDOWN";
	opdiCodeTexts[32] = "GROUP_UNKNOWN";
	opdiCodeTexts[33] = "MESSAGE_UNKNOWN";
	opdiCodeTexts[34] = "FUNCTION_UNKNOWN";
}

AbstractOpenHAT::~AbstractOpenHAT(void) {
}

uint8_t AbstractOpenHAT::idleTimeoutReached(void) {
	uint8_t result = OPDI::idleTimeoutReached();

	// log if idle timeout occurred
	if (result == OPDI_DISCONNECTED)
		this->logNormal("Idle timeout reached");

	return result;
}

void AbstractOpenHAT::protocolCallback(uint8_t protState) {
	if (protState == OPDI_PROTOCOL_START_HANDSHAKE) {
		this->logDebug("Handshake started");
	} else
	if (protState == OPDI_PROTOCOL_CONNECTED) {
		this->connected();
	} else
	if (protState == OPDI_PROTOCOL_DISCONNECTED) {
		this->disconnected();
	}
}

void AbstractOpenHAT::connected() {
	this->logNormal("Connected to: " + this->masterName);

	// notify registered listeners
	auto it = this->connectionListeners.begin();
	auto ite = this->connectionListeners.end();
	while (it != ite) {
		(*it)->masterConnected();
		++it;
	}
}

void AbstractOpenHAT::disconnected() {
	this->logNormal("Disconnected from: " + this->masterName);

	this->masterName = std::string();

	// notify registered listeners
	auto it = this->connectionListeners.begin();
	auto ite = this->connectionListeners.end();
	while (it != ite) {
		(*it)->masterDisconnected();
		++it;
	}
}

void AbstractOpenHAT::addConnectionListener(IConnectionListener* listener) {
	this->removeConnectionListener(listener);
	this->connectionListeners.push_back(listener);
}

void AbstractOpenHAT::removeConnectionListener(IConnectionListener* listener) {
	ConnectionListenerList::iterator it = std::find(this->connectionListeners.begin(), this->connectionListeners.end(), listener);
	if (it != this->connectionListeners.end())
		this->connectionListeners.erase(it);
}

void AbstractOpenHAT::sayHello(void) {
	if (this->logVerbosity == opdi::LogVerbosity::QUIET)
		return;

	this->logNormal("OpenHAT version " + this->to_string(this->majorVersion) + "." + this->to_string(this->minorVersion) + "." + this->to_string(this->patchVersion) + " (c) Leo Meyer 2015", opdi::LogVerbosity::NORMAL);
	this->logVerbose("Build: " + std::string(__DATE__) + " " + std::string(__TIME__), opdi::LogVerbosity::VERBOSE);
	this->logVerbose("Running as user: " + this->getCurrentUser(), opdi::LogVerbosity::VERBOSE);
}

void AbstractOpenHAT::showHelp(void) {
	this->sayHello();
	this->println("Open Protocol for Device Interaction Daemon");
	this->println("Mandatory command line parameters:");
	this->println("  -c <config_file>: use the specified configuration file");
	this->println("Optional command line parameters:");
	this->println("  --version: print version number and exit");
	this->println("  -h or -?: print help text and exit");
	this->println("  -q: quiet logging mode: log errors only");
	this->println("  -v: verbose logging mode: explicit logging (recommended for normal use)");
	this->println("  -d: debug logging mode: more explicit; log message details");
	this->println("  -x: extreme logging mode: produces a lot of output");
	this->println("  -l <filename>: write log to the specified file");
	this->println("  -t: test mode; validate config, prepare ports and exit");
	this->println("  -p <name=value>: set config parameter $name to value");
}

std::string AbstractOpenHAT::getTimestampStr(void) {
	return Poco::DateTimeFormatter::format(Poco::LocalDateTime(), this->timestampFormat);
}

std::string AbstractOpenHAT::getOPDIResult(uint8_t code) {
	OPDICodeTexts::const_iterator it = this->opdiCodeTexts.find(code);
	if (it == this->opdiCodeTexts.end())
		return std::string("Unknown status code: ") + to_string((int)code);
	return it->second;
}

Poco::Util::AbstractConfiguration* AbstractOpenHAT::readConfiguration(const std::string& filename, const std::map<std::string, std::string>& parameters) {
	// will throw an exception if something goes wrong
	Poco::Util::AbstractConfiguration* result = new OpenHATConfigurationFile(filename, parameters);
	// remember config file location
	Poco::Path filePath(filename);
	result->setString(OPENHAT_CONFIG_FILE_SETTING, filePath.absolute().toString());

	return result;
}

std::string AbstractOpenHAT::getConfigString(Poco::Util::AbstractConfiguration* config, const std::string &section, const std::string &key, const std::string &defaultValue, const bool isRequired) {
	if (isRequired) {
		if (!config->hasProperty(key)) {
			throw Poco::DataException("Expected configuration parameter not found in section '" + section + "'", key);
		}
	}
	return config->getString(key, defaultValue);
}

void AbstractOpenHAT::println(const std::string& text) {
	this->println(text.c_str());
}

void AbstractOpenHAT::printlne(const std::string& text) {
	this->printlne(text.c_str());
}

void AbstractOpenHAT::log(const std::string& text) {
	// Important: log must be thread-safe.
	Poco::Mutex::ScopedLock(this->mutex);

	std::string msg = "[" + this->getTimestampStr() + "] " + (this->shutdownRequested ? "<AFTER SHUTDOWN> " : "") + text;
	if (this->logger != nullptr) {
		this->logger->information(msg);
	} else {
		this->println(msg);
	}
}

void AbstractOpenHAT::logErr(const std::string& message) {
	// Important: log must be thread-safe.
	Poco::Mutex::ScopedLock(this->mutex);

	std::string msg = "[" + this->getTimestampStr() + "] " + "ERROR: " + message;
	if (this->logger != nullptr) {
		this->logger->error(msg);
	}
	this->printlne(msg);
}

void AbstractOpenHAT::logWarn(const std::string& message) {
	// Important: log must be thread-safe.
	Poco::Mutex::ScopedLock(this->mutex);

	std::string msg = "[" + this->getTimestampStr() + "] " + "WARNING: " + message;
	if (this->logger != nullptr) {
		this->logger->warning(msg);
	}
	this->printlne(msg);
}

int AbstractOpenHAT::startup(const std::vector<std::string>& args, const std::map<std::string, std::string>& environment) {
	this->environment = environment;
	Poco::AutoPtr<Poco::Util::AbstractConfiguration> configuration = nullptr;

	bool testMode = false;

	// add default environment parameters
	this->environment["$DATETIME"] = Poco::DateTimeFormatter::format(Poco::LocalDateTime(), this->timestampFormat);
	this->environment["$LOG_DATETIME"] = Poco::DateTimeFormatter::format(Poco::LocalDateTime(), "%Y%m%d_%H%M%S");

	this->shutdownRequested = false;
	std::string configFile;

	// evaluate arguments
	for (unsigned int i = 1; i < args.size(); i++) {
		if (args.at(i) == "-h" || args.at(i) == "-?") {
			this->showHelp();
			return 0;
		} else
		if (args.at(i) == "--version") {
			this->logVerbosity = opdi::LogVerbosity::VERBOSE;
			this->sayHello();
			return 0;
		} else
		if (args.at(i) == "-d") {
			this->logVerbosity = opdi::LogVerbosity::DEBUG;
		} else
		if (args.at(i) == "-e") {
			this->logVerbosity = opdi::LogVerbosity::EXTREME;
		} else
		if (args.at(i) == "-v") {
			this->logVerbosity = opdi::LogVerbosity::VERBOSE;
		} else
		if (args.at(i) == "-q") {
			this->logVerbosity = opdi::LogVerbosity::QUIET;
		} else
		if (args.at(i) == "-t") {
			testMode = true;
		} else
		if (args.at(i) == "-c") {
			i++;
			if (args.size() == i) {
				throw Poco::SyntaxException("Expected configuration file name after argument -c");
			} else {
				configFile = args.at(i);
			}
		} else
		if (args.at(i) == "-l") {
			i++;
			if (args.size() == i) {
				throw Poco::SyntaxException("Expected log file name after argument -l");
			} else {
				// initialize logger and channel
				Poco::AutoPtr<Poco::SimpleFileChannel> pChannel(new Poco::SimpleFileChannel);
				pChannel->setProperty("path", args.at(i));
				pChannel->setProperty("rotation", "1 M");
				Poco::Logger::root().setChannel(pChannel);
				this->logger = Poco::Logger::has("");
			}
		} else
		if (args.at(i) == "-p") {
			i++;
			if (args.size() == i) {
				throw Poco::SyntaxException("Expected parameter name=value after argument -p");
			} else {
				std::string param = args.at(i);
				// detect position of "="
				size_t pos = param.find_first_of("=");
				if ((pos == std::string::npos) || (pos == 0))
					throw Poco::SyntaxException("Illegal parameter syntax; expected name=value: " + param);
				std::string name = param.substr(0, pos);
				std::string value = param.substr(pos + 1);
				this->environment["$" + name] = value;
			}
		} else {
			throw Poco::SyntaxException("Invalid argument", args.at(i));
		}
	}

	// no configuration?
	if (configFile == "")
		throw Poco::SyntaxException("Expected argument: -c <config_file>");

	// load configuration, substituting environment parameters
	configuration = this->readConfiguration(configFile, this->environment);

	this->sayHello();
	this->logVerbose("Using configuration file: " + configFile);

	if (this->logVerbosity >= opdi::LogVerbosity::DEBUG) {
		this->logDebug("Environment parameters:");
		auto it = this->environment.begin();
		auto ite = this->environment.end();
		while (it != ite) {
			this->logDebug("  " + (*it).first + " = " + (*it).second);
			++it;
		}
	}

	// create view to "General" section
	Poco::AutoPtr<Poco::Util::AbstractConfiguration> general = configuration->createView("General");
	this->setGeneralConfiguration(general);

	this->setupRoot(configuration);

	this->logVerbose("Node setup complete, preparing ports");

	this->sortPorts();
	this->preparePorts();

	// startup has been done using the process owner
	// if specified, change process privileges to a different user
	if (general->hasProperty("SwitchToUser")) {
		this->switchToUser(general->getString("SwitchToUser"));
	}

	// create view to "Connection" section
	Poco::AutoPtr<Poco::Util::AbstractConfiguration> connection = configuration->createView("Connection");

	int result = this->setupConnection(connection, testMode);

	// save persistent configuration when exiting
	this->savePersistentConfig();

	return result;
}

void AbstractOpenHAT::lockResource(const std::string& resourceID, const std::string& lockerID) {
	this->logDebug("Trying to lock resource '" + resourceID + "' for " + lockerID);
	// try to locate the resource ID
	LockedResources::const_iterator it = this->lockedResources.find(resourceID);
	// resource already used?
	if (it != this->lockedResources.end())
		throw Poco::DataException("Resource requested by " + lockerID + " is already in use by " + it->second + ": " + resourceID);

	// store the resource
	this->lockedResources[resourceID] = lockerID;
}

opdi::LogVerbosity AbstractOpenHAT::getConfigLogVerbosity(Poco::Util::AbstractConfiguration* config, opdi::LogVerbosity defaultVerbosity) {
	std::string logVerbosityStr = config->getString("LogVerbosity", "");

	if ((logVerbosityStr != "")) {
		if (logVerbosityStr == "Quiet") {
			return opdi::LogVerbosity::QUIET;
		} else
		if (logVerbosityStr == "Normal") {
			return opdi::LogVerbosity::NORMAL;
		} else
		if (logVerbosityStr == "Verbose") {
			return opdi::LogVerbosity::VERBOSE;
		} else
		if (logVerbosityStr == "Debug") {
			return opdi::LogVerbosity::DEBUG;
		} else
		if (logVerbosityStr == "Extreme") {
			return opdi::LogVerbosity::EXTREME;
		} else
			throw Poco::InvalidArgumentException("Verbosity level unknown (expected one of 'Quiet', 'Normal', 'Verbose', 'Debug' or 'Extreme')", logVerbosityStr);
	}
	return defaultVerbosity;
}

Poco::Util::AbstractConfiguration* AbstractOpenHAT::getConfigForState(Poco::Util::AbstractConfiguration* baseConfig, const std::string& viewName) {
	// replace configuration with a layered configuration that uses the persistent
	// configuration with higher priority
	// thus, port states will be pulled from the persistent configuration if they are present
	Poco::Util::LayeredConfiguration* newConfig = new Poco::Util::LayeredConfiguration();
	// persistent configuration specified?
	if (this->persistentConfig != nullptr) {
		// persistent config has high priority
		if (viewName == "")
			newConfig->add(this->persistentConfig, 0);
		else {
			Poco::AutoPtr<Poco::Util::AbstractConfiguration> configView = this->persistentConfig->createView(viewName);
			newConfig->add(configView);
		}
	}
	// standard config has low priority
	newConfig->add(baseConfig, 10);

	return newConfig;
}

void AbstractOpenHAT::setGeneralConfiguration(Poco::Util::AbstractConfiguration* general) {
	this->logVerbose("Setting up general configuration");

	// set log verbosity only if it's not already set
	if (this->logVerbosity == opdi::LogVerbosity::UNKNOWN)
		this->logVerbosity = this->getConfigLogVerbosity(general, opdi::LogVerbosity::NORMAL);

	// initialize persistent configuration if specified
	std::string persistentFile = this->getConfigString(general, "General", "PersistentConfig", "", false);
	if (persistentFile != "") {
		// determine CWD-relative path depending on location of config file
		std::string configFilePath = general->getString(OPENHAT_CONFIG_FILE_SETTING, "");
		if (configFilePath == "")
			throw Poco::DataException("Programming error: Configuration file path not specified in config settings");

		Poco::Path filePath(configFilePath);
		Poco::Path absPath(filePath.absolute());
		Poco::Path parentPath = absPath.parent();
		// append or replace new config file path to path of previous config file
		Poco::Path finalPath = parentPath.resolve(persistentFile);
		persistentFile = finalPath.toString();
		Poco::File file(finalPath);

		// the persistent configuration is not an INI file (because POCO can't write INI files)
		// but a "properties-format" file
		if (file.exists())
			// load existing file
			this->persistentConfig = new Poco::Util::PropertyFileConfiguration(persistentFile);
		else
			// file does not yet exist; will be created when persisting state
			this->persistentConfig = new Poco::Util::PropertyFileConfiguration();
		this->persistentConfigFile = persistentFile;
	}

	this->heartbeatFile = this->getConfigString(general, "General", "HeartbeatFile", "", false);
	this->targetFramesPerSecond = general->getInt("TargetFPS", this->targetFramesPerSecond);

	std::string slaveName = this->getConfigString(general, "General", "SlaveName", "", true);
	int messageTimeout = general->getInt("MessageTimeout", OPDI_DEFAULT_MESSAGE_TIMEOUT);
	if ((messageTimeout < 0) || (messageTimeout > 65535))
			throw Poco::InvalidArgumentException("MessageTimeout must be greater than 0 and may not exceed 65535", to_string(messageTimeout));
	opdi_set_timeout(messageTimeout);

	int idleTimeout = general->getInt("IdleTimeout", DEFAULT_IDLETIMEOUT_MS);

	this->deviceInfo = general->getString("DeviceInfo", "");

	this->allowHiddenPorts = general->getBool("AllowHidden", true);

	// encryption defined?
	std::string encryptionNode = general->getString("Encryption", "");
	if (encryptionNode != "") {
		Poco::AutoPtr<Poco::Util::AbstractConfiguration> encryptionConfig = general->createView(encryptionNode);
		this->configureEncryption(encryptionConfig);
	}

	// authentication defined?
	std::string authenticationNode = general->getString("Authentication", "");
	if (authenticationNode != "") {
		Poco::AutoPtr<Poco::Util::AbstractConfiguration> authenticationConfig = general->createView(authenticationNode);
		this->configureAuthentication(authenticationConfig);
	}

	// initialize OPDI slave
	this->setup(slaveName.c_str(), idleTimeout);
}

void AbstractOpenHAT::configureEncryption(Poco::Util::AbstractConfiguration* config) {
	this->logVerbose("Configuring encryption");

	std::string type = config->getString("Type", "");
	if (type == "AES") {
		std::string key = config->getString("Key", "");
		if (key.length() != 16)
			throw Poco::DataException("AES encryption setting 'Key' must be specified and 16 characters long");

		strncpy(opdi_encryption_method, "AES", ENCRYPTION_METHOD_MAXLENGTH);
		strncpy(opdi_encryption_key, key.c_str(), ENCRYPTION_KEY_MAXLENGTH);
	} else
		throw Poco::DataException("Encryption type not supported, expected 'AES': " + type);
}

void AbstractOpenHAT::configureAuthentication(Poco::Util::AbstractConfiguration* config) {
	this->logVerbose("Configuring authentication");

	std::string type = config->getString("Type", "");
	if (type == "Login") {
		std::string username = config->getString("Username", "");
		if (username == "")
			throw Poco::DataException("Authentication setting 'Username' must be specified");
		this->loginUser = username;

		this->loginPassword = config->getString("Password", "");

		// flag the device: expect authentication
		opdi_device_flags |= OPDI_FLAG_AUTHENTICATION_REQUIRED;
	} else
		throw Poco::DataException("Authentication type not supported, expected 'Login': " + type);
}

/** Reads common properties from the configuration and configures the port group. */
void AbstractOpenHAT::configureGroup(Poco::Util::AbstractConfiguration* groupConfig, opdi::PortGroup* group, int defaultFlags) {
	// the default label is the port ID
	std::string portLabel = this->getConfigString(groupConfig, group->getID(), "Label", group->getID(), false);
	group->setLabel(portLabel.c_str());

	int flags = groupConfig->getInt("Flags", -1);
	if (flags >= 0) {
		group->setFlags(flags);
	} else
		// default flags specified?
		// avoid calling setFlags unnecessarily because subclasses may implement specific behavior
		if (defaultFlags > 0)
			group->setFlags(defaultFlags);

	// extended properties
	std::string icon = this->getConfigString(groupConfig, group->getID(), "Icon", "", false);
	if (icon != "") {
		group->setIcon(icon);
	}
	std::string parent = this->getConfigString(groupConfig, group->getID(), "Parent", "", false);
	if (parent != "") {
		group->setParent(parent.c_str());
	}
}

void AbstractOpenHAT::setupGroup(Poco::Util::AbstractConfiguration* groupConfig, const std::string& group) {
	this->logVerbose("Setting up group: " + group);

	opdi::PortGroup* portGroup = new opdi::PortGroup(group.c_str());
	this->configureGroup(groupConfig, portGroup, 0);

	this->addPortGroup(portGroup);
}

std::string AbstractOpenHAT::resolveRelativePath(Poco::Util::AbstractConfiguration* config, const std::string& source, const std::string& path, const std::string defaultValue) {
	// determine path type
	std::string relativeTo = this->getConfigString(config, source, "RelativeTo", defaultValue, false);

	if (relativeTo == "CWD") {
		Poco::Path filePath(path);
		Poco::Path absPath(filePath.absolute());
		// absolute path specified?
		if (filePath.toString() == absPath.toString()) {
			this->logWarning("Path specified as relative to CWD, but absolute path given: '" + path + "'");
			return absPath.toString();
		}
		return path;
	} else
	if (relativeTo == "Config") {
		Poco::Path filePath(path);
		Poco::Path absPath(filePath.absolute());
		// absolute path specified?
		if (filePath.toString() == absPath.toString()) {
			this->logWarning("Path specified as relative to Config, but absolute path given: '" + path + "'");
			return absPath.toString();
		}
		// determine configuration-relative path depending on location of previous config file
		std::string configFile = config->getString(OPENHAT_CONFIG_FILE_SETTING, "");
		if (configFile == "")
			throw Poco::DataException("Programming error: Configuration file path not specified in config settings");
		Poco::Path configFilePath(configFile);
		Poco::Path parentPath = configFilePath.parent();
        this->logDebug("Resolving path '" + path + "' relative to configuration file path '" + parentPath.toString() + "'");
		// append or replace new config file path to path of previous config file
		Poco::Path finalPath = parentPath.resolve(path);
		this->logDebug("Resolved path: '" + finalPath.toString() + "'");
		return finalPath.toString();
	} else
        if (relativeTo.empty())
            return path;

    throw Poco::DataException("Unknown RelativeTo property specified; expected 'CWD' or 'Config'", relativeTo);
}

void AbstractOpenHAT::setupInclude(Poco::Util::AbstractConfiguration* config, Poco::Util::AbstractConfiguration* parentConfig, const std::string& node) {
	this->logVerbose("Setting up include: " + node);

	// filename must be present; include files are by default relative to the current configuration file
	std::string filename = this->resolveRelativePath(config, node, this->getConfigString(config, node, "Filename", "", true), "Config");

	// read parameters and build a map, based on the environment parameters
	std::map<std::string, std::string> parameters;
	this->getEnvironment(parameters);

	// the include node requires a section "<node>.Parameters"
	this->logVerbose(node + ": Evaluating include parameters section: " + node + ".Parameters");

	Poco::AutoPtr<Poco::Util::AbstractConfiguration> paramConfig = parentConfig->createView(node + ".Parameters");

	// get list of parameters
	Poco::Util::AbstractConfiguration::Keys paramKeys;
	paramConfig->keys("", paramKeys);

	for (auto it = paramKeys.begin(), ite = paramKeys.end(); it != ite; ++it) {
		std::string param = paramConfig->getString(*it, "");
		// store in the map
		parameters[std::string("$") + (*it)] = param;
	}

	// warn if no parameters
	if ((parameters.size() == 0))
		this->logNormal(node + ": No parameters for include in section " + node + ".Parameters found, is this intended?");

	this->logVerbose(node + ": Processing include file: " + filename);

	if (this->logVerbosity >= opdi::LogVerbosity::DEBUG) {
		this->logDebug(node + ": Include file parameters:");
		auto it = parameters.begin();
		auto ite = parameters.end();
		while (it != ite) {
			this->logDebug(node + ":   " + (*it).first + " = " + (*it).second);
			++it;
		}
	}
	Poco::Util::AbstractConfiguration* includeConfig = this->readConfiguration(filename, parameters);

	// setup the root node of the included configuration
	this->setupRoot(includeConfig);

	if (this->logVerbosity >= opdi::LogVerbosity::VERBOSE) {
		this->logVerbose(node + ": Include file " + filename + " processed successfully.");
		std::string configFilePath = parentConfig->getString(OPENHAT_CONFIG_FILE_SETTING, "");
		if (configFilePath != "")
			this->logVerbose("Continuing with parent configuration file: " + configFilePath);
	}
}

void AbstractOpenHAT::configurePort(Poco::Util::AbstractConfiguration* portConfig, opdi::Port* port, int defaultFlags) {
	// ports can be hidden if allowed
	if (this->allowHiddenPorts)
		port->setHidden(portConfig->getBool("Hidden", false));

	// ports can be readonly
	port->setReadonly(portConfig->getBool("Readonly", false));

	// ports can be persistent
	port->setPersistent(portConfig->getBool("Persistent", false));

	// the default label is the port ID
	std::string portLabel = this->getConfigString(portConfig, port->ID(), "Label", port->ID(), false);
	port->setLabel(portLabel.c_str());

	std::string portDirCaps = this->getConfigString(portConfig, port->ID(), "DirCaps", "", false);
	if (portDirCaps == "Input") {
		port->setDirCaps(OPDI_PORTDIRCAP_INPUT);
	} else if (portDirCaps == "Output") {
		port->setDirCaps(OPDI_PORTDIRCAP_OUTPUT);
	} else if (portDirCaps == "Bidi") {
		port->setDirCaps(OPDI_PORTDIRCAP_BIDI);
	} else if (portDirCaps != "")
		throw Poco::DataException("Unknown DirCaps specified; expected 'Input', 'Output' or 'Bidi'", portDirCaps);

	int flags = portConfig->getInt("Flags", -1);
	if (flags >= 0) {
		port->setFlags(flags);
	} else
		// default flags specified?
		// avoid calling setFlags unnecessarily because subclasses may implement specific behavior
		if (defaultFlags > 0)
			port->setFlags(defaultFlags);

	std::string refreshMode = this->getConfigString(portConfig, port->ID(), "RefreshMode", "", false);
	if (refreshMode == "Off") {
		port->setRefreshMode(opdi::Port::RefreshMode::REFRESH_OFF);
	} else
	if (refreshMode == "Periodic") {
		port->setRefreshMode(opdi::Port::RefreshMode::REFRESH_PERIODIC);
	} else
	if (refreshMode == "Auto") {
		port->setRefreshMode(opdi::Port::RefreshMode::REFRESH_AUTO);
	} else
		if (refreshMode != "")
			throw Poco::DataException("Unknown RefreshMode specified; expected 'Off', 'Periodic' or 'Auto': " + refreshMode);

	if (port->getRefreshMode() == opdi::Port::RefreshMode::REFRESH_PERIODIC) {
		int time = portConfig->getInt("RefreshTime", -1);
		if (time >= 0) {
			port->setPeriodicRefreshTime(time);
		} else {
			throw Poco::DataException("A RefreshTime > 0 must be specified in Periodic refresh mode: " + to_string(time));
		}
	}

	// extended properties
	std::string unit = this->getConfigString(portConfig, port->ID(), "Unit", "", false);
	if (unit != "") {
		port->setUnit(unit);
	}
	std::string icon = this->getConfigString(portConfig, port->ID(), "Icon", "", false);
	if (icon != "") {
		port->setIcon(icon);
	}
	std::string group = this->getConfigString(portConfig, port->ID(), "Group", "", false);
	if (group != "") {
		port->setGroup(group);
	}

	port->tag = this->getConfigString(portConfig, port->ID(), "Tag", "", false);

	port->orderID = portConfig->getInt("OrderID", -1);

	port->onChangeIntPortsStr = this->getConfigString(portConfig, port->ID(), "OnChangeInt", "", false);
	port->onChangeUserPortsStr = this->getConfigString(portConfig, port->ID(), "OnChangeUser", "", false);

	port->setLogVerbosity(this->getConfigLogVerbosity(portConfig, this->logVerbosity));
}

void AbstractOpenHAT::configureDigitalPort(Poco::Util::AbstractConfiguration* portConfig, opdi::DigitalPort* port, bool stateOnly) {
	if (!stateOnly)
		this->configurePort(portConfig, port, 0);

	Poco::AutoPtr<Poco::Util::AbstractConfiguration> stateConfig = this->getConfigForState(portConfig, port->ID());

	std::string portMode = this->getConfigString(stateConfig, port->ID(), "Mode", "", false);
	if (portMode == "Input") {
		port->setMode(OPDI_DIGITAL_MODE_INPUT_FLOATING);
	} else if (portMode == "Input with pullup") {
		port->setMode(OPDI_DIGITAL_MODE_INPUT_PULLUP);
	} else if (portMode == "Input with pulldown") {
		port->setMode(OPDI_DIGITAL_MODE_INPUT_PULLDOWN);
	} else if (portMode == "Output") {
		port->setMode(OPDI_DIGITAL_MODE_OUTPUT);
	} else if (portMode != "")
		throw Poco::DataException("Unknown Mode specified; expected 'Input', 'Input with pullup', 'Input with pulldown', or 'Output'", portMode);

	std::string portLine = this->getConfigString(stateConfig, port->ID(), "Line", "", false);
	if (portLine == "High") {
		port->setLine(1);
	} else if (portLine == "Low") {
		port->setLine(0);
	} else if (portLine != "")
		throw Poco::DataException("Unknown Line specified; expected 'Low' or 'High'", portLine);
}

void AbstractOpenHAT::setupEmulatedDigitalPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port) {
	this->logVerbose("Setting up emulated digital port: " + port);

	opdi::DigitalPort* digPort = new opdi::DigitalPort(port.c_str());
	this->configureDigitalPort(portConfig, digPort);

	this->addPort(digPort);
}

void AbstractOpenHAT::configureAnalogPort(Poco::Util::AbstractConfiguration* portConfig, opdi::AnalogPort* port, bool stateOnly) {
	if (!stateOnly)
		this->configurePort(portConfig, port,
			// default flags: assume everything is supported
			OPDI_ANALOG_PORT_CAN_CHANGE_RES |
			OPDI_ANALOG_PORT_RESOLUTION_8 |
			OPDI_ANALOG_PORT_RESOLUTION_9 |
			OPDI_ANALOG_PORT_RESOLUTION_10 |
			OPDI_ANALOG_PORT_RESOLUTION_11 |
			OPDI_ANALOG_PORT_RESOLUTION_12 |
			OPDI_ANALOG_PORT_CAN_CHANGE_REF |
			OPDI_ANALOG_PORT_REFERENCE_INT |
			OPDI_ANALOG_PORT_REFERENCE_EXT);

	Poco::AutoPtr<Poco::Util::AbstractConfiguration> stateConfig = this->getConfigForState(portConfig, port->ID());

	std::string mode = this->getConfigString(stateConfig, port->ID(), "Mode", "", false);
	if (mode == "Input")
		port->setMode(0);
	else if (mode == "Output")
		port->setMode(1);
	else if (mode != "")
		throw Poco::ApplicationException("Unknown mode specified; expected 'Input' or 'Output'", mode);

	// TODO reference?

	if (stateConfig->hasProperty("Resolution")) {
		uint8_t resolution = stateConfig->getInt("Resolution", OPDI_ANALOG_PORT_RESOLUTION_12);
		port->setResolution(resolution);
	}
	if (stateConfig->hasProperty("Value")) {
		uint32_t value = stateConfig->getInt("Value", 0);
		port->setValue(value);
	}
}

void AbstractOpenHAT::setupEmulatedAnalogPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port) {
	this->logVerbose("Setting up emulated analog port: " + port);

	opdi::AnalogPort* anaPort = new opdi::AnalogPort(port.c_str());
	this->configureAnalogPort(portConfig, anaPort);

	this->addPort(anaPort);
}

void AbstractOpenHAT::configureSelectPort(Poco::Util::AbstractConfiguration* portConfig, Poco::Util::AbstractConfiguration* parentConfig, opdi::SelectPort* port, bool stateOnly) {
	if (!stateOnly) {
		this->configurePort(portConfig, port, 0);

		// the select port requires a prefix or section "<portID>.Items"
		Poco::AutoPtr<Poco::Util::AbstractConfiguration> portItems = parentConfig->createView(std::string(port->ID()) + ".Items");

		// get ordered list of items
		Poco::Util::AbstractConfiguration::Keys itemKeys;
		portItems->keys("", itemKeys);

		typedef Poco::Tuple<int, std::string> Item;
		typedef std::vector<Item> ItemList;
		ItemList orderedItems;

		// create ordered list of items (by priority)
		for (auto it = itemKeys.begin(), ite = itemKeys.end(); it != ite; ++it) {
			int itemNumber = portItems->getInt(*it, 0);
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

		if (orderedItems.size() == 0)
			throw Poco::DataException("The select port " + std::string(port->ID()) + " requires at least one item in its config section", std::string(port->ID()) + ".Items");

		// go through items, create ordered list of char* items
		std::vector<const char*> charItems;
		auto nli = orderedItems.begin();
		auto nlie = orderedItems.end();
		while (nli != nlie) {
			charItems.push_back(nli->get<1>().c_str());
			++nli;
		}
		charItems.push_back(nullptr);

		// set port items
		port->setItems(&charItems[0]);
	}

	Poco::AutoPtr<Poco::Util::AbstractConfiguration> stateConfig = this->getConfigForState(portConfig, port->ID());

	if (stateConfig->getString("Position", "") != "") {
		int16_t position = stateConfig->getInt("Position", 0);
		if ((position < 0) || (position > port->getMaxPosition()))
			throw Poco::DataException("Wrong select port setting: Position is out of range: " + to_string(position));
		port->setPosition(position);
	}
}

void AbstractOpenHAT::setupEmulatedSelectPort(Poco::Util::AbstractConfiguration* portConfig, Poco::Util::AbstractConfiguration* parentConfig, const std::string& port) {
	this->logVerbose("Setting up emulated select port: " + port);

	opdi::SelectPort* selPort = new opdi::SelectPort(port.c_str());
	this->configureSelectPort(portConfig, parentConfig, selPort);

	this->addPort(selPort);
}

void AbstractOpenHAT::configureDialPort(Poco::Util::AbstractConfiguration* portConfig, opdi::DialPort* port, bool stateOnly) {
	if (!stateOnly) {
		this->configurePort(portConfig, port, 0);

		int64_t min = portConfig->getInt64("Min", LLONG_MIN);
		int64_t max = portConfig->getInt64("Max", LLONG_MAX);
		if (min >= max)
			throw Poco::DataException("Wrong dial port setting: Max must be greater than Min");
		int64_t step = portConfig->getInt64("Step", 1);
		if (step < 1)
			throw Poco::DataException("Wrong dial port setting: Step may not be negative or zero: " + to_string(step));

		port->setMin(min);
		port->setMax(max);
		port->setStep(step);

		// a dial port can have an automatically generated, hidden aggregator port that provides it with history values
		std::string history = portConfig->getString("History", "");
		if (!history.empty()) {
			// create aggregator port with internal ID
			std::string aggPortID = port->ID() + "_AutoAggregator";
			AggregatorPort* aggPort = new AggregatorPort(this, aggPortID.c_str());
			aggPort->sourcePortID = port->ID();
			// default interval is one minute
			aggPort->queryInterval = 60;
			// allow some error tolerance (in case a port value is temporarily unavailable)
			aggPort->allowedErrors = 3;
			if (history == "Hour") {
				aggPort->totalValues = 60;
			} else
			if (history == "Day") {
				aggPort->totalValues = 1440;
			} else
				throw Poco::DataException(port->ID() + ": Value for 'History' not supported, expected 'Hour' or 'Day': " + history);
			aggPort->setHidden(true);
			// set the standard log verbosity of hidden automatic aggregators to "Normal" because they can sometimes generate a log of log spam
			// if the dial port's log verbosity is defined, the aggregator uses the same value
			aggPort->logVerbosity = this->getConfigLogVerbosity(portConfig, opdi::LogVerbosity::NORMAL);
			// Auto-Aggregators are persistent to not lose values over a restart (requires a persistent config file to work)
			aggPort->setPersistent(true);
			// add the port
			this->addPort(aggPort);
			// if a history port is used and the RefreshMode has not been set manually, the RefreshMode is set to Automatic
			// this provides expected behavior without the need to specify RefreshMode for the port manually.
			if (portConfig->getString("RefreshMode", "").empty())
				port->setRefreshMode(opdi::Port::RefreshMode::REFRESH_AUTO);
		}
	}

	Poco::AutoPtr<Poco::Util::AbstractConfiguration> stateConfig = this->getConfigForState(portConfig, port->ID());

	int64_t position = stateConfig->getInt64("Position", port->getMin());
	// set port error to invalid if the value is out of range
	if ((position < port->getMin()) || (position > port->getMax()))
		port->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
		//throw Poco::DataException("Wrong dial port setting: Position is out of range: " + to_string(position));
	else
		port->setPosition(position);
}

void AbstractOpenHAT::setupEmulatedDialPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port) {
	this->logVerbose("Setting up emulated dial port: " + port);

	opdi::DialPort* dialPort = new opdi::DialPort(port.c_str());
	this->configureDialPort(portConfig, dialPort);

	this->addPort(dialPort);
}

void AbstractOpenHAT::configureStreamingPort(Poco::Util::AbstractConfiguration* portConfig, opdi::StreamingPort* port) {
	this->configurePort(portConfig, port, 0);
}

void AbstractOpenHAT::setupSerialStreamingPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port) {
	this->logVerbose("Setting up serial streaming port: " + port);

	SerialStreamingPort* ssPort = new SerialStreamingPort(this, port.c_str());
	ssPort->configure(portConfig);

	this->addPort(ssPort);
}

void AbstractOpenHAT::setupLoggerPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port) {
	this->logVerbose("Setting up Logger port: " + port);

	LoggerPort* logPort = new LoggerPort(this, port.c_str());
	logPort->configure(portConfig);

	this->addPort(logPort);
}

void AbstractOpenHAT::setupLogicPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port) {
	this->logVerbose("Setting up LogicPort: " + port);

	LogicPort* dlPort = new LogicPort(this, port.c_str());
	dlPort->configure(portConfig);

	this->addPort(dlPort);
}

void AbstractOpenHAT::setupFaderPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port) {
	this->logVerbose("Setting up FaderPort: " + port);

	FaderPort* fPort = new FaderPort(this, port.c_str());
	fPort->configure(portConfig);

	this->addPort(fPort);
}

void AbstractOpenHAT::setupExecPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port) {
	this->logVerbose("Setting up ExecPort: " + port);

	ExecPort* ePort = new ExecPort(this, port.c_str());
	ePort->configure(portConfig);

	this->addPort(ePort);
}

void AbstractOpenHAT::setupPulsePort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port) {
	this->logVerbose("Setting up PulsePort: " + port);

	PulsePort* pulsePort = new PulsePort(this, port.c_str());
	pulsePort->configure(portConfig);

	this->addPort(pulsePort);
}

void AbstractOpenHAT::setupSelectorPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port) {
	this->logVerbose("Setting up SelectorPort: " + port);

	SelectorPort* selectorPort = new SelectorPort(this, port.c_str());
	selectorPort->configure(portConfig);

	this->addPort(selectorPort);
}

#ifdef OPENHAT_USE_EXPRTK
void AbstractOpenHAT::setupExpressionPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port) {
	this->logVerbose("Setting up Expression: " + port);

	ExpressionPort* expressionPort = new ExpressionPort(this, port.c_str());
	expressionPort->configure(portConfig);

	this->addPort(expressionPort);
}
#endif	// def OPENHAT_USE_EXPRTK

void AbstractOpenHAT::setupTimerPort(Poco::Util::AbstractConfiguration* portConfig, Poco::Util::AbstractConfiguration* parentConfig, const std::string& port) {
	this->logVerbose("Setting up Timer: " + port);

	TimerPort* timerPort = new TimerPort(this, port.c_str());
	timerPort->configure(portConfig, parentConfig);

	this->addPort(timerPort);
}

void AbstractOpenHAT::setupErrorDetectorPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port) {
	this->logVerbose("Setting up ErrorDetector: " + port);

	ErrorDetectorPort* edPort = new ErrorDetectorPort(this, port.c_str());
	edPort->configure(portConfig);

	this->addPort(edPort);
}

void AbstractOpenHAT::setupSceneSelectPort(Poco::Util::AbstractConfiguration* portConfig, Poco::Util::AbstractConfiguration* parentConfig, const std::string& port) {
	this->logVerbose("Setting up SceneSelect: " + port);

	SceneSelectPort* ssPort = new SceneSelectPort(this, port.c_str());
	ssPort->configure(portConfig, parentConfig);

	this->addPort(ssPort);
}

void AbstractOpenHAT::setupFilePort(Poco::Util::AbstractConfiguration* portConfig, Poco::Util::AbstractConfiguration* parentConfig, const std::string& port) {
	this->logVerbose("Setting up File: " + port);

	FilePort* fiPort = new FilePort(this, port.c_str());
	fiPort->configure(portConfig, parentConfig);

	this->addPort(fiPort);
}

void AbstractOpenHAT::setupAggregatorPort(Poco::Util::AbstractConfiguration* portConfig, Poco::Util::AbstractConfiguration* parentConfig, const std::string& port) {
	this->logVerbose("Setting up Aggregator: " + port);

	AggregatorPort* agPort = new AggregatorPort(this, port.c_str());
	agPort->configure(portConfig, parentConfig);

	this->addPort(agPort);
}

void AbstractOpenHAT::setupTriggerPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port) {
	this->logVerbose("Setting up Trigger: " + port);

	TriggerPort* tPort = new TriggerPort(this, port.c_str());
	tPort->configure(portConfig);

	this->addPort(tPort);
}

void AbstractOpenHAT::setupCounterPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port) {
	this->logVerbose("Setting up Counter: " + port);

	CounterPort* cPort = new CounterPort(this, port.c_str());
	cPort->configure(portConfig);

	this->addPort(cPort);
}

void AbstractOpenHAT::setupNode(Poco::Util::AbstractConfiguration* config, const std::string& node) {
	this->logVerbose("Setting up node: " + node);

	// emit warnings if node IDs deviate from best practices

	// check whether node name is all numeric
	double value = 0;
	if (Poco::NumberParser::tryParseFloat(node, value)) {
		this->logWarning("Node ID can be confused with a number: " + node);
	}
	// check whether there are special characters
	Poco::RegularExpression re(".*\\W.*");
	if (re.match(node)) {
		this->logWarning("Node ID should not contain special characters: " + node);
	}

	// create node section view
	Poco::AutoPtr<Poco::Util::AbstractConfiguration> nodeConfig = config->createView(node);

	std::string nodeType = this->getConfigString(nodeConfig, node, "Type", "", true);
	if (nodeType == "Plugin") {
		// get driver information
		std::string nodeDriver = this->getConfigString(nodeConfig, node, "Driver", "", true);
		// plugins are by default relative to the current working directory
		nodeDriver = this->resolveRelativePath(nodeConfig, node, nodeDriver, "CWD");

		this->logVerbose("Loading plugin driver: " + nodeDriver);

		// try to load the plugin; the driver name is the (platform dependent) library file name
		IOpenHATPlugin* plugin = this->getPlugin(nodeDriver);

		// add plugin to internal list (avoids memory leaks)
		this->pluginList.push_back(plugin);

		// init the plugin
		plugin->setupPlugin(this, node, config);

	} else
	if (nodeType == "Group") {
		this->setupGroup(nodeConfig, node);
	} else
	if (nodeType == "Include") {
		this->setupInclude(nodeConfig, config, node);
	} else
	if (nodeType == "DigitalPort") {
		this->setupEmulatedDigitalPort(nodeConfig, node);
	} else
	if (nodeType == "AnalogPort") {
		this->setupEmulatedAnalogPort(nodeConfig, node);
	} else
	if (nodeType == "SelectPort") {
		this->setupEmulatedSelectPort(nodeConfig, config, node);
	} else
	if (nodeType == "DialPort") {
		this->setupEmulatedDialPort(nodeConfig, node);
	} else
	if (nodeType == "SerialStreamingPort") {
		this->setupSerialStreamingPort(nodeConfig, node);
	} else
	if (nodeType == "Logger") {
		this->setupLoggerPort(nodeConfig, node);
	} else
	if (nodeType == "Logic") {
		this->setupLogicPort(nodeConfig, node);
	} else
	if (nodeType == "Pulse") {
		this->setupPulsePort(nodeConfig, node);
	} else
	if (nodeType == "Selector") {
		this->setupSelectorPort(nodeConfig, node);
#ifdef OPENHAT_USE_EXPRTK
	} else
	if (nodeType == "Expression") {
		this->setupExpressionPort(nodeConfig, node);
#else
#pragma message( "Expression library not included, cannot use the Expression node type" )
#endif	// def OPENHAT_USE_EXPRTK
	} else
	if (nodeType == "Timer") {
		this->setupTimerPort(nodeConfig, config, node);
	} else
	if (nodeType == "ErrorDetector") {
		this->setupErrorDetectorPort(nodeConfig, node);
	} else
	if (nodeType == "Fader") {
		this->setupFaderPort(nodeConfig, node);
	} else
	if (nodeType == "Exec") {
		this->setupExecPort(nodeConfig, node);
	} else
	if (nodeType == "SceneSelect") {
		this->setupSceneSelectPort(nodeConfig, config, node);
	} else
	if (nodeType == "File") {
		this->setupFilePort(nodeConfig, config, node);
	} else
	if (nodeType == "Aggregator") {
		this->setupAggregatorPort(nodeConfig, config, node);
	} else
	if (nodeType == "Trigger") {
		this->setupTriggerPort(nodeConfig, node);
	} else
	if (nodeType == "Counter") {
		this->setupCounterPort(nodeConfig, node);
	}
	else
		throw Poco::DataException("Invalid configuration: Unknown node type", nodeType);
}

void AbstractOpenHAT::setupRoot(Poco::Util::AbstractConfiguration* config) {
	// enumerate section "Root"
	Poco::AutoPtr<Poco::Util::AbstractConfiguration> nodes = config->createView("Root");
	this->logVerbose("Setting up root nodes");

	Poco::Util::AbstractConfiguration::Keys nodeKeys;
	nodes->keys("", nodeKeys);

	typedef Poco::Tuple<int, std::string> Node;
	typedef std::vector<Node> NodeList;
	NodeList orderedNodes;

	// create ordered list of nodes (by priority)
	for (auto it = nodeKeys.begin(), ite = nodeKeys.end(); it != ite; ++it) {
		int nodeNumber = nodes->getInt(*it, 0);
		// check whether the node is active
		if (nodeNumber < 0)
			continue;

		// insert at the correct position to create a sorted list of nodes
		auto nli = orderedNodes.begin();
		auto nlie = orderedNodes.end();
		while (nli != nlie) {
			if (nli->get<0>() > nodeNumber)
				break;
			++nli;
		}
		Node node(nodeNumber, *it);
		orderedNodes.insert(nli, node);
	}

	// warn if no nodes found
	if ((orderedNodes.size() == 0) && (this->logVerbosity >= opdi::LogVerbosity::NORMAL)) {
		// get file name
		std::string filename = config->getString(OPENHAT_CONFIG_FILE_SETTING, "");
		if (filename == "")
			filename = "<unknown configuration file>";
		this->logNormal(filename + ": No nodes in section Root found, is this intended?");
	}

	// go through ordered list, setup nodes by name
	auto nli = orderedNodes.begin();
	auto nlie = orderedNodes.end();
	while (nli != nlie) {
		this->setupNode(config, nli->get<1>());
		++nli;
	}

	// TODO check group hierarchy
}

int AbstractOpenHAT::setupConnection(Poco::Util::AbstractConfiguration* config, bool testMode) {
	this->logVerbose(std::string("Setting up connection for slave: ") + this->slaveName);
	std::string connectionType = this->getConfigString(config, "Connection", "Type", "", true);

	if (connectionType == "TCP") {
		std::string interface_ = this->getConfigString(config, "Connection", "Interface", "*", false);

		if (interface_ != "*")
			throw Poco::DataException("Connection interface: Sorry, values other than '*' are currently not supported");

		int port = config->getInt("Port", DEFAULT_TCP_PORT);

		if (testMode)
			return OPDI_STATUS_OK;

		return this->setupTCP(interface_, port);
	}
	else
		throw Poco::DataException("Invalid configuration; unknown connection type", connectionType);
}

void AbstractOpenHAT::warnIfPluginMoreRecent(const std::string& driver) {
	// parse compile date and time
    std::stringstream compiled;
    compiled << __DATE__ << " " << __TIME__;

	Poco::DateTime buildTime;
	int tzd;

	if (!Poco::DateTimeParser::tryParse("%b %e %Y %H:%M:%s", compiled.str(), buildTime, tzd)) {
		this->logNormal("Warning: Could not parse build date and time to check possible ABI violation of driver " + driver + ": " + __DATE__ + " " + __TIME__);
	} else {
		// check whether the plugin driver file is more recent
		Poco::File driverFile(driver);
		// convert build time to UTC
		buildTime.makeUTC(Poco::Timezone::tzd());
		// assume both files have been built in the same time zone (getLastModified also returns UTC)
		if ((driverFile.getLastModified() < buildTime.timestamp()))
			this->logNormal("Warning: Plugin module " + driver + " is older than the main binary; possible ABI conflict! In case of strange errors please recompile the plugin!");
	}
}

uint8_t AbstractOpenHAT::waiting(uint8_t canSend) {
	uint8_t result;

	// add up microseconds of idle time
	this->totalMicroseconds += this->idleStopwatch.elapsed();
	this->waitingCallsPerSecond++;

	// start local stopwatch
	Poco::Stopwatch stopwatch;
	stopwatch.start();

	// exception-safe processing
	try {
		result = OPDI::waiting(canSend);
	} catch (Poco::Exception &pe) {
		this->logError(std::string("Unhandled exception while housekeeping: ") + pe.message());
		result = OPDI_DEVICE_ERROR;
	} catch (std::exception &e) {
		this->logError(std::string("Unhandled exception while housekeeping: ") + e.what());
		result = OPDI_DEVICE_ERROR;
	} catch (...) {
		this->logError(std::string("Unknown error while housekeeping"));
		result = OPDI_DEVICE_ERROR;
	}
	// TODO decide: ignore errors or abort?

	if (result != OPDI_STATUS_OK)
		return result;

	// add runtime statistics to monitor buffer
	this->monSecondStats[this->monSecondPos] = stopwatch.elapsed();		// microseconds
	// add up microseconds of processing time
	this->totalMicroseconds += this->monSecondStats[this->monSecondPos];
	this->monSecondPos++;
	// buffer rollover?
	if (this->monSecondPos >= maxSecondStats) {
		this->monSecondPos = 0;
	}
	// collect statistics until a second has elapsed
	if (this->totalMicroseconds >= 1000000) {
		uint64_t maxProcTime = -1;
		uint64_t sumProcTime = 0;
		for (int i = 0; i < this->monSecondPos; i++) {
			sumProcTime += this->monSecondStats[i];
			if (this->monSecondStats[i] > maxProcTime)
				maxProcTime = this->monSecondStats[i];
		}
		this->monSecondPos = 0;
		this->framesPerSecond = this->waitingCallsPerSecond * 1000000.0 / this->totalMicroseconds;
		double procAverageUsPerCall = (double)sumProcTime / this->waitingCallsPerSecond;	// microseconds
		double load = sumProcTime * 1.0 / this->totalMicroseconds * 100.0;

		// ignore first calculation results
		if (this->framesPerSecond > 0) {
			if (this->logVerbosity >= opdi::LogVerbosity::EXTREME) {
				this->logExtreme("Elapsed processing time: " + this->to_string(this->totalMicroseconds) + " us");
				this->logExtreme("Loop iterations per second: " + this->to_string(this->framesPerSecond));
				this->logExtreme("Processing time average per iteration: " + this->to_string(procAverageUsPerCall) + " us");
				this->logExtreme("Processing load: " + this->to_string(load) + "%");
			}
			if (load > 90.0)
				this->logDebug("Processing the doWork loop takes very long; load = " + this->to_string(load) + "%");
		}

		// reset counters
		this->totalMicroseconds = 0;
		this->waitingCallsPerSecond = 0;

		// write status file if specified
		if (this->heartbeatFile != "") {
			this->logExtreme("Writing heartbeat file: " + this->heartbeatFile);
			std::string output = this->getTimestampStr() + ": pid=" + this->to_string(Poco::Process::id()) + "; fps=" + this->to_string(this->framesPerSecond) + "; load=" + this->to_string(load) + "%";
			Poco::FileOutputStream fos(this->heartbeatFile);
			fos.write(output.c_str(), output.length());
			fos.close();
		}
	}

	// restart idle stopwatch to measure time until waiting() is called again
	this->idleStopwatch.restart();

	return result;
}

bool icompare_pred(unsigned char a, unsigned char b)
{
    return std::tolower(a) == std::tolower(b);
}

bool icompare(std::string const& a, std::string const& b)
{
    return std::lexicographical_compare(a.begin(), a.end(),
                                        b.begin(), b.end(), icompare_pred);
}

uint8_t AbstractOpenHAT::setPassword(const std::string& password) {
	// TODO fix: username/password side channel leak (timing attack) (?)
	// username must match case insensitive
	if (Poco::UTF8::icompare(this->loginUser, this->username))
		return OPDI_AUTHENTICATION_FAILED;
	// password must match case sensitive
	if (this->loginPassword != password)
		return OPDI_AUTHENTICATION_FAILED;

	return OPDI_STATUS_OK;
}

std::string AbstractOpenHAT::getExtendedDeviceInfo(void) {
	return this->deviceInfo;
}

uint8_t AbstractOpenHAT::refresh(opdi::Port** ports) {
	// base class functionality handles a connected master
	if (this->canSend) {
		uint8_t result = OPDI::refresh(ports);
		if (result != OPDI_STATUS_OK)
			return result;
	}

	if (ports == nullptr) {
		this->allPortsRefreshed(this);
		this->logDebug("Processed refresh for all ports");
		return OPDI_STATUS_OK;
	}

	opdi::Port* port = ports[0];
	uint8_t i = 0;
	while (port != nullptr) {
		this->portRefreshed(this, port);
		this->logDebug("Processed refresh for port: " + port->ID());
		port = ports[++i];
	}

	return OPDI_STATUS_OK;
}

void AbstractOpenHAT::savePersistentConfig() {
	if (this->persistentConfig == nullptr)
		return;

	this->persistentConfig->setString("LastChange", this->getTimestampStr());
	this->persistentConfig->save(this->persistentConfigFile);
}

void AbstractOpenHAT::persist(opdi::Port* port) {
	if (this->persistentConfig == nullptr) {
		this->logWarning(std::string("Unable to persist state for port ") + port->ID() + ": No configuration file specified; use 'PersistentConfig' in the General configuration section");
		return;
	}

	this->logDebug("Trying to persist port state for: " + port->ID());

	try {
		// evaluation depends on port type
		if (port->getType()[0] == OPDI_PORTTYPE_DIGITAL[0]) {
			uint8_t mode;
			uint8_t line;
			((opdi::DigitalPort*)port)->getState(&mode, &line);
			std::string modeStr = "";
			if (mode == OPDI_DIGITAL_MODE_INPUT_FLOATING)
				modeStr = "Input";
			else if (mode == OPDI_DIGITAL_MODE_INPUT_PULLUP)
				modeStr = "Input with pullup";
			else if (mode == OPDI_DIGITAL_MODE_INPUT_PULLDOWN)
				modeStr = "Input with pulldown";
			else if (mode == OPDI_DIGITAL_MODE_OUTPUT)
				modeStr = "Output";
			std::string lineStr = "";
			if (line == 0)
				lineStr = "Low";
			else if (line == 1)
				lineStr = "High";

			if (modeStr != "") {
				this->logDebug("Writing port state for: " + port->ID() + "; mode = " + modeStr);
				this->persistentConfig->setString(port->ID() + ".Mode", modeStr);
			}
			if (lineStr != "") {
				this->logDebug("Writing port state for: " + port->ID() + "; line = " + lineStr);
				this->persistentConfig->setString(port->ID() + ".Line", lineStr);
			}
		} else
		if (port->getType()[0] == OPDI_PORTTYPE_ANALOG[0]) {
			uint8_t mode;
			uint8_t resolution;
			uint8_t reference;
			int32_t value;
			((opdi::AnalogPort*)port)->getState(&mode, &resolution, &reference, &value);
			std::string modeStr = "";
			if (mode == OPDI_ANALOG_MODE_INPUT)
				modeStr = "Input";
			else if (mode == OPDI_ANALOG_MODE_OUTPUT)
				modeStr = "Output";
			// TODO reference
//			std::string refStr = "";

			if (modeStr != "") {
				this->logDebug("Writing port state for: " + port->ID() + "; mode = " + modeStr);
				this->persistentConfig->setString(port->ID() + ".Mode", modeStr);
			}
			this->logDebug("Writing port state for: " + port->ID() + "; resolution = " + this->to_string((int)resolution));
			this->persistentConfig->setInt(port->ID() + ".Resolution", resolution);
			this->logDebug("Writing port state for: " + port->ID() + "; value = " + this->to_string(value));
			this->persistentConfig->setInt(port->ID() + ".Value", value);
		} else
		if (port->getType()[0] == OPDI_PORTTYPE_DIAL[0]) {
			int64_t position;
			((opdi::DialPort*)port)->getState(&position);

			this->logDebug("Writing port state for: " + port->ID() + "; position = " + this->to_string(position));
			this->persistentConfig->setInt64(port->ID() + ".Position", position);
		} else
		if (port->getType()[0] == OPDI_PORTTYPE_SELECT[0]) {
			uint16_t position;
			((opdi::SelectPort*)port)->getState(&position);

			this->logDebug("Writing port state for: " + port->ID() + "; position = " + this->to_string(position));
			this->persistentConfig->setInt(port->ID() + ".Position", position);
		} else {
			this->logDebug("Unable to persist port state for: " + port->ID() + "; unknown port type: " + port->getType());
			return;
		}
	} catch (Poco::Exception &e) {
		this->logWarning("Unable to persist state of port " + port->ID() + ": " + e.message());
		return;
	}
	// save configuration only if not already shutting down
	// the config will be saved on shutdown automatically
	if (!this->shutdownRequested)
		this->savePersistentConfig();
}

std::string AbstractOpenHAT::getPortStateStr(opdi::Port* port) const {
	try {
		if (port->getType()[0] == OPDI_PORTTYPE_DIGITAL[0]) {
			uint8_t line;
			uint8_t mode;
			((opdi::DigitalPort*)port)->getState(&mode, &line);
			char c[] = " ";
			c[0] = line + '0';
			return std::string(c);
		}
		if (port->getType()[0] == OPDI_PORTTYPE_ANALOG[0]) {
			double value = ((opdi::AnalogPort*)port)->getRelativeValue();
			return this->to_string(value);
		}
		if (port->getType()[0] == OPDI_PORTTYPE_SELECT[0]) {
			uint16_t position;
			((opdi::SelectPort*)port)->getState(&position);
			return this->to_string(position);
		}
		if (port->getType()[0] == OPDI_PORTTYPE_DIAL[0]) {
			int64_t position;
			((opdi::DialPort*)port)->getState(&position);
			return this->to_string(position);
		}
		// unknown port type
		return "";
	} catch (...) {
		// in case of error return an empty string
		return "";
	}
}

std::string AbstractOpenHAT::getDeviceInfo(void) {
	return this->deviceInfo;
}

void AbstractOpenHAT::getEnvironment(std::map<std::string, std::string>& mapToFill) {
	mapToFill.insert(this->environment.begin(), this->environment.end());
}

}		// namespace openhat

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The following functions implement the glue code between C and C++.
////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t opdi_slave_callback(OPDIFunctionCode opdiFunctionCode, char* buffer, size_t bufLength) {

	switch (opdiFunctionCode) {
	case OPDI_FUNCTION_GET_CONFIG_NAME:
		strncpy(buffer, Opdi->getSlaveName().c_str(), bufLength);
		break;
	case OPDI_FUNCTION_SET_MASTER_NAME:
		return Opdi->setMasterName(buffer);
	case OPDI_FUNCTION_GET_SUPPORTED_PROTOCOLS:
		strncpy(buffer, SUPPORTED_PROTOCOLS, bufLength);
		break;
	case OPDI_FUNCTION_GET_ENCODING:
		strncpy(buffer, Opdi->getEncoding().c_str(), bufLength);
		break;
	case OPDI_FUNCTION_SET_LANGUAGES:
		return Opdi->setLanguages(buffer);
	case OPDI_FUNCTION_GET_EXTENDED_DEVICEINFO:
		strncpy(buffer, Opdi->getExtendedDeviceInfo().c_str(), bufLength);
		break;
	case OPDI_FUNCTION_GET_EXTENDED_PORTINFO: {
		uint8_t code;
		// port ID in buffer
		std::string exPortInfo = Opdi->getExtendedPortInfo(buffer, &code);
		if (code != OPDI_STATUS_OK)
			return code;
		strncpy(buffer, exPortInfo.c_str(), bufLength);
		break;
	}
	case OPDI_FUNCTION_GET_EXTENDED_PORTSTATE: {
		uint8_t code;
		// port ID in buffer
		std::string exPortState = Opdi->getExtendedPortState(buffer, &code);
		if (code != OPDI_STATUS_OK)
			return code;
		strncpy(buffer, exPortState.c_str(), bufLength);
		break;
	}
#ifndef OPDI_NO_AUTHENTICATION
	case OPDI_FUNCTION_SET_USERNAME: return Opdi->setUsername(buffer);
	case OPDI_FUNCTION_SET_PASSWORD: return Opdi->setPassword(buffer);
#endif
	default: return OPDI_FUNCTION_UNKNOWN;
	}

	// check buffer for possible overflow (strncpy zeroes out the remainder)
	// if the buffer is too small for the required function the device cannot support this function
	// so the returned string is set to empty
	if (buffer[bufLength - 1] != '\0') {
		buffer[0] = '\0';
		Opdi->logDebug("WARNING: Slave callback buffer overflow, cleared the result; requested function = " + Opdi->to_string(opdiFunctionCode));
	}
	return OPDI_STATUS_OK;
}

/** Can be used to send debug messages to a monitor.
 *
 */
uint8_t opdi_debug_msg(const char* message, uint8_t direction) {
	std::string dirChar = "-";
	if (direction == OPDI_DIR_INCOMING)
		dirChar = ">";
	else
	if (direction == OPDI_DIR_OUTGOING)
		dirChar = "<";
	else
	if (direction == OPDI_DIR_INCOMING_ENCR)
		dirChar = "}";
	else
	if (direction == OPDI_DIR_OUTGOING_ENCR)
		dirChar = "{";
	Opdi->logDebug(dirChar + message);
	return OPDI_STATUS_OK;
}

// digital port functions
#ifndef NO_DIGITAL_PORTS

uint8_t opdi_get_digital_port_state(opdi_Port* port, char mode[], char line[]) {
	uint8_t dMode;
	uint8_t dLine;
	opdi::DigitalPort* dPort = (opdi::DigitalPort*)Opdi->findPort(port);
	if (dPort == nullptr)
		return OPDI_PORT_UNKNOWN;

	try {
		dPort->getState(&dMode, &dLine);
		mode[0] = '0' + dMode;
		line[0] = '0' + dLine;
	} catch (opdi::Port::ValueUnavailable) {
		// TODO localize message
		opdi_set_port_message("Value unavailable");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::ValueExpired) {
		// TODO localize message
		opdi_set_port_message("Value expired");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDenied &ad) {
		opdi_set_port_message(ad.message().c_str());
		return OPDI_PORT_ACCESS_DENIED;
	}
	return OPDI_STATUS_OK;
}

uint8_t opdi_set_digital_port_line(opdi_Port* port, const char line[]) {
	uint8_t dLine;

	opdi::DigitalPort* dPort = (opdi::DigitalPort*)Opdi->findPort(port);
	if (dPort == nullptr)
		return OPDI_PORT_UNKNOWN;

	if (dPort->isReadonly())
		return OPDI_PORT_ACCESS_DENIED;

	if (line[0] == '1')
		dLine = 1;
	else
		dLine = 0;
	try {
		dPort->setLine(dLine, opdi::Port::ChangeSource::CHANGESOURCE_USER);
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDenied &ad) {
		opdi_set_port_message(ad.message().c_str());
		return OPDI_PORT_ACCESS_DENIED;
	}
	return OPDI_STATUS_OK;
}

uint8_t opdi_set_digital_port_mode(opdi_Port* port, const char mode[]) {
	uint8_t dMode;

	opdi::DigitalPort* dPort = (opdi::DigitalPort*)Opdi->findPort(port);
	if (dPort == nullptr)
		return OPDI_PORT_UNKNOWN;

	if (dPort->isReadonly())
		return OPDI_PORT_ACCESS_DENIED;

	if ((mode[0] >= '0') && (mode[0] <= '3'))
		dMode = mode[0] - '0';
	else
		// mode not supported
		return OPDI_PROTOCOL_ERROR;
	try {
		dPort->setMode(dMode, opdi::Port::ChangeSource::CHANGESOURCE_USER);
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDenied &ad) {
		opdi_set_port_message(ad.message().c_str());
		return OPDI_PORT_ACCESS_DENIED;
	}
	return OPDI_STATUS_OK;
}

#endif 		// NO_DIGITAL_PORTS

#ifndef NO_ANALOG_PORTS

uint8_t opdi_get_analog_port_state(opdi_Port* port, char mode[], char res[], char ref[], int32_t* value) {
	uint8_t aMode;
	uint8_t aRef;
	uint8_t aRes;

	opdi::AnalogPort* aPort = (opdi::AnalogPort*)Opdi->findPort(port);
	if (aPort == nullptr)
		return OPDI_PORT_UNKNOWN;

	try {
		aPort->getState(&aMode, &aRes, &aRef, value);
	} catch (opdi::Port::ValueUnavailable) {
		// TODO localize message
		opdi_set_port_message("Value unavailable");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::ValueExpired) {
		// TODO localize message
		opdi_set_port_message("Value expired");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDenied &ad) {
		opdi_set_port_message(ad.message().c_str());
		return OPDI_PORT_ACCESS_DENIED;
	}
	mode[0] = '0' + aMode;
	res[0] = '0' + (aRes - 8);
	ref[0] = '0' + aRef;

	return OPDI_STATUS_OK;
}

uint8_t opdi_set_analog_port_value(opdi_Port* port, int32_t value) {
	opdi::AnalogPort* aPort = (opdi::AnalogPort*)Opdi->findPort(port);
	if (aPort == nullptr)
		return OPDI_PORT_UNKNOWN;

	if (aPort->isReadonly())
		return OPDI_PORT_ACCESS_DENIED;

	try {
		aPort->setValue(value, opdi::Port::ChangeSource::CHANGESOURCE_USER);
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDenied &ad) {
		opdi_set_port_message(ad.message().c_str());
		return OPDI_PORT_ACCESS_DENIED;
	}
	return OPDI_STATUS_OK;
}

uint8_t opdi_set_analog_port_mode(opdi_Port* port, const char mode[]) {
	uint8_t aMode;

	opdi::AnalogPort* aPort = (opdi::AnalogPort*)Opdi->findPort(port);
	if (aPort == nullptr)
		return OPDI_PORT_UNKNOWN;

	if ((mode[0] >= '0') && (mode[0] <= '1'))
		aMode = mode[0] - '0';
	else
		// mode not supported
		return OPDI_PROTOCOL_ERROR;

	if (aPort->isReadonly())
		return OPDI_PORT_ACCESS_DENIED;

	try {
		aPort->setMode(aMode, opdi::Port::ChangeSource::CHANGESOURCE_USER);
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDenied &ad) {
		opdi_set_port_message(ad.message().c_str());
		return OPDI_PORT_ACCESS_DENIED;
	}
	return OPDI_STATUS_OK;
}

uint8_t opdi_set_analog_port_resolution(opdi_Port* port, const char res[]) {
	uint8_t aRes;
	opdi::AnalogPort* aPort = (opdi::AnalogPort*)Opdi->findPort(port);
	if (aPort == nullptr)
		return OPDI_PORT_UNKNOWN;

	if ((res[0] >= '0') && (res[0] <= '4'))
		aRes = res[0] - '0' + 8;
	else
		// resolution not supported
		return OPDI_PROTOCOL_ERROR;

	try {
		aPort->setResolution(aRes, opdi::Port::ChangeSource::CHANGESOURCE_USER);
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDenied &ad) {
		opdi_set_port_message(ad.message().c_str());
		return OPDI_PORT_ACCESS_DENIED;
	}
	return OPDI_STATUS_OK;
}

uint8_t opdi_set_analog_port_reference(opdi_Port* port, const char ref[]) {
	uint8_t aRef;
	opdi::AnalogPort* aPort = (opdi::AnalogPort*)Opdi->findPort(port);
	if (aPort == nullptr)
		return OPDI_PORT_UNKNOWN;

	if ((ref[0] >= '0') && (ref[0] <= '1'))
		aRef = ref[0] - '0';
	else
		// mode not supported
		return OPDI_PROTOCOL_ERROR;

	try {
		aPort->setReference(aRef, opdi::Port::ChangeSource::CHANGESOURCE_USER);
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDenied &ad) {
		opdi_set_port_message(ad.message().c_str());
		return OPDI_PORT_ACCESS_DENIED;
	}
	return OPDI_STATUS_OK;
}

#endif	// NO_ANALOG_PORTS

#ifndef OPDI_NO_SELECT_PORTS

uint8_t opdi_get_select_port_state(opdi_Port* port, uint16_t* position) {
	opdi::SelectPort* sPort = (opdi::SelectPort*)Opdi->findPort(port);
	if (sPort == nullptr)
		return OPDI_PORT_UNKNOWN;

	try {
		sPort->getState(position);
	} catch (opdi::Port::ValueUnavailable) {
		// TODO localize message
		opdi_set_port_message("Value unavailable");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::ValueExpired) {
		// TODO localize message
		opdi_set_port_message("Value expired");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDenied &ad) {
		opdi_set_port_message(ad.message().c_str());
		return OPDI_PORT_ACCESS_DENIED;
	}
	return OPDI_STATUS_OK;
}

uint8_t opdi_set_select_port_position(opdi_Port* port, uint16_t position) {
	opdi::SelectPort* sPort = (opdi::SelectPort*)Opdi->findPort(port);
	if (sPort == nullptr)
		return OPDI_PORT_UNKNOWN;

	if (sPort->isReadonly())
		return OPDI_PORT_ACCESS_DENIED;

	try {
		sPort->setPosition(position, opdi::Port::ChangeSource::CHANGESOURCE_USER);
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDenied &ad) {
		opdi_set_port_message(ad.message().c_str());
		return OPDI_PORT_ACCESS_DENIED;
	}
	return OPDI_STATUS_OK;
}

#endif	//  OPDI_NO_SELECT_PORTS

#ifndef OPDI_NO_DIAL_PORTS

uint8_t opdi_get_dial_port_state(opdi_Port* port, int64_t* position) {
	opdi::DialPort* dPort = (opdi::DialPort*)Opdi->findPort(port);
	if (dPort == nullptr)
		return OPDI_PORT_UNKNOWN;

	try {
		dPort->getState(position);
	} catch (opdi::Port::ValueUnavailable) {
		// TODO localize message
		opdi_set_port_message("Value unavailable");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::ValueExpired) {
		// TODO localize message
		opdi_set_port_message("Value expired");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDenied &ad) {
		opdi_set_port_message(ad.message().c_str());
		return OPDI_PORT_ACCESS_DENIED;
	}
	return OPDI_STATUS_OK;
}

uint8_t opdi_set_dial_port_position(opdi_Port* port, int64_t position) {
	opdi::DialPort* dPort = (opdi::DialPort*)Opdi->findPort(port);
	if (dPort == nullptr)
		return OPDI_PORT_UNKNOWN;

	if (dPort->isReadonly())
		return OPDI_PORT_ACCESS_DENIED;

	try {
		dPort->setPosition(position, opdi::Port::ChangeSource::CHANGESOURCE_USER);
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDenied &ad) {
		opdi_set_port_message(ad.message().c_str());
		return OPDI_PORT_ACCESS_DENIED;
	}
	return OPDI_STATUS_OK;
}

#endif	// OPDI_NO_DIAL_PORTS

uint8_t opdi_choose_language(const char* /*languages*/) {
	// TODO

	return OPDI_STATUS_OK;
}

#ifdef OPDI_HAS_MESSAGE_HANDLED

uint8_t opdi_message_handled(channel_t channel, const char** parts) {
	return Opdi->messageHandled(channel, parts);
}

#endif
