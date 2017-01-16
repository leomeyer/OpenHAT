#include "AbstractOpenHAT.h"

#include <vector>
#include <time.h>
#ifdef __GNUG__
#include <cxxabi.h>
#endif

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
	this->prepared = false;

	this->logVerbosity = opdi::LogVerbosity::UNKNOWN;
	this->persistentConfig = nullptr;

	this->logger = nullptr;
	this->timestampFormat = "%Y-%m-%d %H:%M:%S.%i";

	this->monSecondPos = 0;
	this->monSecondStats = (uint64_t*)malloc(this->maxSecondStats * sizeof(uint64_t));
	this->totalMicroseconds = 0;
	this->targetFramesPerSecond = 20;
	this->waitingCallsPerSecond = 0;
	this->framesPerSecond = 0;
	this->allowHiddenPorts = true;
	this->suppressUnusedParameterMessages = false;

	// opdi result codes
	resultCodeTexts[0] = "STATUS_OK";
	resultCodeTexts[1] = "DISCONNECTED";
	resultCodeTexts[2] = "TIMEOUT";
	resultCodeTexts[3] = "CANCELLED";
	resultCodeTexts[4] = "ERROR_MALFORMED_MESSAGE";
	resultCodeTexts[5] = "ERROR_CONVERSION";
	resultCodeTexts[6] = "ERROR_MSGBUF_OVERFLOW";
	resultCodeTexts[7] = "ERROR_DEST_OVERFLOW";
	resultCodeTexts[8] = "ERROR_STRINGS_OVERFLOW";
	resultCodeTexts[9] = "ERROR_PARTS_OVERFLOW";
	resultCodeTexts[10] = "PROTOCOL_ERROR";
	resultCodeTexts[11] = "PROTOCOL_NOT_SUPPORTED";
	resultCodeTexts[12] = "ENCRYPTION_NOT_SUPPORTED";
	resultCodeTexts[13] = "ENCRYPTION_REQUIRED";
	resultCodeTexts[14] = "ENCRYPTION_ERROR";
	resultCodeTexts[15] = "AUTH_NOT_SUPPORTED";
	resultCodeTexts[16] = "AUTHENTICATION_EXPECTED";
	resultCodeTexts[17] = "AUTHENTICATION_FAILED";
	resultCodeTexts[18] = "DEVICE_ERROR";
	resultCodeTexts[19] = "TOO_MANY_PORTS";
	resultCodeTexts[20] = "PORTTYPE_UNKNOWN";
	resultCodeTexts[21] = "PORT_UNKNOWN";
	resultCodeTexts[22] = "WRONG_PORT_TYPE";
	resultCodeTexts[23] = "TOO_MANY_BINDINGS";
	resultCodeTexts[24] = "NO_BINDING";
	resultCodeTexts[25] = "CHANNEL_INVALID";
	resultCodeTexts[26] = "POSITION_INVALID";
	resultCodeTexts[27] = "NETWORK_ERROR";
	resultCodeTexts[28] = "TERMINATOR_IN_PAYLOAD";
	resultCodeTexts[29] = "PORT_ACCESS_DENIED";
	resultCodeTexts[30] = "PORT_ERROR";
	resultCodeTexts[31] = "SHUTDOWN";
	resultCodeTexts[32] = "GROUP_UNKNOWN";
	resultCodeTexts[33] = "MESSAGE_UNKNOWN";
	resultCodeTexts[34] = "FUNCTION_UNKNOWN";

	// openhatd-specific code texts
	resultCodeTexts[OPENHATD_INTERRUPTED] = "INTERRUPTED";
	resultCodeTexts[OPENHATD_TEST_FAILED] = "TEST_FAILED";
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

	this->logNormal(this->appName + " version " + this->to_string(this->majorVersion) + "." + this->to_string(this->minorVersion) + "." + this->to_string(this->patchVersion));
	this->logNormal("Copyright (c) OpenHAT contributors (https://openhat.org, https://github.com/openhat-org)");
	this->logVerbose("Build: " + std::string(__DATE__) + " " + std::string(__TIME__));
	this->logDebug("Running as user: " + this->getCurrentUser());
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

uint64_t AbstractOpenHAT::getCurrentFrame(void) {
	return this->currentFrame;
}

std::string AbstractOpenHAT::getTimestampStr(void) {
	return Poco::DateTimeFormatter::format(Poco::LocalDateTime(), this->timestampFormat);
}

std::string AbstractOpenHAT::getResultCodeText(uint8_t code) {
	ResultCodeTexts::const_iterator it = this->resultCodeTexts.find(code);
	if (it == this->resultCodeTexts.end())
		return std::string("Unknown status code: ") + to_string((int)code);
	return it->second;
}

ConfigurationView* AbstractOpenHAT::readConfiguration(const std::string& filename, const std::map<std::string, std::string>& parameters) {
	// will throw an exception if something goes wrong
	OpenHATConfigurationFile* fileConfig = new OpenHATConfigurationFile(filename, parameters);
	// remember config file location
	Poco::Path filePath(filename);
	fileConfig->setString(OPENHAT_CONFIG_FILE_SETTING, filePath.absolute().toString());

	ConfigurationView* result = new ConfigurationView(this, fileConfig, "", false);
	return result;
}

std::string AbstractOpenHAT::getConfigString(Poco::Util::AbstractConfiguration* config, const std::string &section, const std::string &key, const std::string &defaultValue, const bool isRequired) {
	if (isRequired) {
		if (!config->hasProperty(key)) {
			this->throwSettingException("Expected configuration parameter not found in section '" + section + "'", key);
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
	Poco::AutoPtr<ConfigurationView> configuration = nullptr;

	bool testMode = false;

	// add default environment parameters
	this->environment["$OPENHAT_VERSION"] = OPENHAT_VERSION_ID;
	this->environment["$DATETIME"] = Poco::DateTimeFormatter::format(Poco::LocalDateTime(), this->timestampFormat);
	this->environment["$LOG_DATETIME"] = Poco::DateTimeFormatter::format(Poco::LocalDateTime(), "%Y%m%d_%H%M%S");
	this->environment["$CWD"] = Poco::Path::current();

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

	opdi::LogVerbosity savedVerbosity = this->logVerbosity;

	// modify verbosity for the following output
	if (this->logVerbosity == opdi::LogVerbosity::UNKNOWN)
		this->logVerbosity = opdi::LogVerbosity::NORMAL;

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

	// restore saved verbosity
	this->logVerbosity = savedVerbosity;

	std::string switchToUserName = this->setupGeneralConfiguration(configuration);

	this->setupRoot(configuration);

	this->logVerbose("Node setup complete, preparing ports");

	this->sortPorts();
	this->preparePorts();

	this->prepared = true;

	// startup has been done using the process owner
	// if specified, change process privileges to a different user
	if (!switchToUserName.empty()) {
		this->switchToUser(switchToUserName);
	}

	int result = this->setupConnection(configuration, testMode);

	// special case: shutdown requested by test port?
	if (result == OPENHATD_TEST_EXIT)
		// return status code 0 (for automatic testing)
		result = OPDI_STATUS_OK;

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
		this->throwSettingException("Resource requested by " + lockerID + " is already in use by " + it->second + ": " + resourceID);

	// store the resource
	this->lockedResources[resourceID] = lockerID;
}

opdi::LogVerbosity AbstractOpenHAT::getConfigLogVerbosity(ConfigurationView* config, opdi::LogVerbosity defaultVerbosity) {
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

Poco::AutoPtr<ConfigurationView> AbstractOpenHAT::createConfigView(Poco::Util::AbstractConfiguration* baseConfig, const std::string& viewName) {
	return new ConfigurationView(this, baseConfig->createView(viewName), viewName);
}

Poco::Util::AbstractConfiguration* AbstractOpenHAT::getConfigForState(ConfigurationView* baseConfig, const std::string& viewName) {
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
			Poco::AutoPtr<ConfigurationView> configView = this->createConfigView(persistentConfig, viewName);
			newConfig->add(configView);
		}
	}
	// standard config has low priority
	newConfig->add(baseConfig, 10);

	return newConfig;
}

void AbstractOpenHAT::unusedConfigKeysDetected(const std::string & viewName, const std::vector<std::string>& unusedKeys) {
	if (this->suppressUnusedParameterMessages)
		return;
	auto ite = unusedKeys.cend();
	for (auto it = unusedKeys.cbegin(); it != ite; ++it) {
		this->logWarning("Unused configuration parameter in node " + viewName + ": " + *it);
	}
}

void AbstractOpenHAT::throwSettingException(const std::string & message, const std::string& detail) {
	// do not output unused parameter messages after this call
	this->suppressUnusedParameterMessages = true;

	throw SettingException(message, detail);
}

std::string AbstractOpenHAT::setupGeneralConfiguration(ConfigurationView* config) {
	this->logVerbose("Setting up general configuration");
	// enumerate section "Root"
	Poco::AutoPtr<ConfigurationView> general = this->createConfigView(config, "General");

	// set log verbosity only if it's not already set
	if (this->logVerbosity == opdi::LogVerbosity::UNKNOWN)
		this->logVerbosity = this->getConfigLogVerbosity(general, opdi::LogVerbosity::NORMAL);

	// initialize persistent configuration if specified
	std::string persistentFile = this->getConfigString(general, "General", "PersistentConfig", "", false);
	if (persistentFile != "") {
		// determine CWD-relative path depending on location of config file
		persistentFile = this->resolveRelativePath(general, "RelativeTo", persistentFile, "CWD");
		Poco::File file(persistentFile);

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
		Poco::AutoPtr<ConfigurationView> encryptionConfig = this->createConfigView(config, encryptionNode);
		this->configureEncryption(encryptionConfig);
	}

	// authentication defined?
	std::string authenticationNode = general->getString("Authentication", "");
	if (authenticationNode != "") {
		Poco::AutoPtr<ConfigurationView> authenticationConfig = this->createConfigView(config, authenticationNode);
		this->configureAuthentication(authenticationConfig);
	}

	// initialize OPDI slave
	this->setup(slaveName.c_str(), idleTimeout);

	// return the user name to switch to (if specified)
	return general->getString("SwitchToUser", "");
}

void AbstractOpenHAT::configureEncryption(ConfigurationView* config) {
	this->logVerbose("Configuring encryption");

	std::string type = config->getString("Type", "");
	if (type == "AES") {
		std::string key = config->getString("Key", "");
		if (key.length() != 16)
			this->throwSettingException("AES encryption setting 'Key' must be specified and 16 characters long");

		strncpy(opdi_encryption_method, "AES", ENCRYPTION_METHOD_MAXLENGTH);
		strncpy(opdi_encryption_key, key.c_str(), ENCRYPTION_KEY_MAXLENGTH);
	} else
		this->throwSettingException("Encryption type not supported, expected 'AES': " + type);
}

void AbstractOpenHAT::configureAuthentication(ConfigurationView* config) {
	this->logVerbose("Configuring authentication");

	std::string type = config->getString("Type", "");
	if (type == "Login") {
		std::string username = config->getString("Username", "");
		if (username == "")
			this->throwSettingException("Authentication setting 'Username' must be specified");
		this->loginUser = username;

		this->loginPassword = config->getString("Password", "");

		// flag the device: expect authentication
		opdi_device_flags |= OPDI_FLAG_AUTHENTICATION_REQUIRED;
	} else
		this->throwSettingException("Authentication type not supported, expected 'Login': " + type);
}

/** Reads common properties from the configuration and configures the port group. */
void AbstractOpenHAT::configureGroup(ConfigurationView* groupConfig, opdi::PortGroup* group, int defaultFlags) {
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

void AbstractOpenHAT::setupGroup(ConfigurationView* groupConfig, const std::string& group) {
	this->logDebug("Setting up group: " + group);

	opdi::PortGroup* portGroup = new opdi::PortGroup(group.c_str());
	this->configureGroup(groupConfig, portGroup, 0);

	this->addPortGroup(portGroup);
}

std::string AbstractOpenHAT::resolveRelativePath(ConfigurationView* config, const std::string& source, const std::string& path, const std::string& defaultValue, const std::string& manualPath, const std::string& settingName) {
	// determine path type
	std::string relativeTo = this->getConfigString(config, source, settingName, defaultValue, false);

	if (relativeTo == "CWD") {
		Poco::Path filePath(path);
		Poco::Path absPath(filePath.absolute());
		// absolute path specified?
		if (filePath.toString() == absPath.toString()) {
			this->logWarning("Path specified as relative to CWD, but absolute path given: '" + path + "'");
			return absPath.toString();
		}
		return path;
	}
	else if (relativeTo == "Plugin") {
		Poco::Path filePath(path);
		Poco::Path absPath(filePath.absolute());
		// absolute path specified?
		if (filePath.toString() == absPath.toString()) {
			this->logWarning("Path specified as relative to Plugin, but absolute path given: '" + path + "'");
			return absPath.toString();
		}
		// determine plugin-relative path depending on location of plugin file
		std::string pluginFile = manualPath;
		if (pluginFile == "")
			this->throwSettingException("Programming error: Plugin file path not specified");
		Poco::Path pluginFilePath(pluginFile);
		Poco::Path parentPath = pluginFilePath.parent();
		this->logDebug("Resolving path '" + path + "' relative to plugin file path '" + parentPath.toString() + "'");
		// append or replace new file path to path of previous config file
		Poco::Path finalPath = parentPath.resolve(path);
		this->logDebug("Resolved path: '" + finalPath.toString() + "'");
		return finalPath.toString();
	}
	else if (relativeTo == "Config") {
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
			this->throwSettingException("Programming error: Configuration file path not specified in config settings");
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

    this->throwSettingException("Unknown RelativeTo property specified; expected 'CWD', 'Config' or 'Plugin'", relativeTo);
	return "";
}

void AbstractOpenHAT::setupInclude(ConfigurationView* config, ConfigurationView* parentConfig, const std::string& node) {
	this->logVerbose("Setting up include: " + node);

	// filename must be present; include files are by default relative to the current configuration file
	std::string filename = this->resolveRelativePath(config, node, this->getConfigString(config, node, "Filename", "", true), "Config");

	// read parameters and build a map, based on the environment parameters
	std::map<std::string, std::string> parameters;
	this->getEnvironment(parameters);

	// the include node requires a section "<node>.Parameters"
	this->logVerbose(node + ": Evaluating include parameters section: " + node + ".Parameters");

	Poco::AutoPtr<ConfigurationView> paramConfig = this->createConfigView(parentConfig, node + ".Parameters");

	// get list of parameters
	ConfigurationView::Keys paramKeys;
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
	ConfigurationView* includeConfig = this->readConfiguration(filename, parameters);

	// setup the root node of the included configuration
	this->setupRoot(includeConfig);

	if (this->logVerbosity >= opdi::LogVerbosity::VERBOSE) {
		this->logVerbose(node + ": Include file " + filename + " processed successfully.");
		std::string configFilePath = parentConfig->getString(OPENHAT_CONFIG_FILE_SETTING, "");
		if (configFilePath != "")
			this->logVerbose("Continuing with parent configuration file: " + configFilePath);
	}
}

void AbstractOpenHAT::configurePort(ConfigurationView* portConfig, opdi::Port* port, int defaultFlags) {
	// ports can be hidden if allowed
	if (this->allowHiddenPorts)
		port->setHidden(portConfig->getBool("Hidden", port->isHidden()));

	// ports can be readonly
	port->setReadonly(portConfig->getBool("Readonly", port->isReadonly()));

	// ports can be persistent
	port->setPersistent(portConfig->getBool("Persistent", port->isPersistent()));

	std::string portLabel = this->getConfigString(portConfig, port->ID(), "Label", port->getLabel(), false);
	port->setLabel(portLabel.c_str());

	std::string portDirCaps = this->getConfigString(portConfig, port->ID(), "DirCaps", "", false);
	if (portDirCaps == "Input") {
		port->setDirCaps(OPDI_PORTDIRCAP_INPUT);
	} else if (portDirCaps == "Output") {
		port->setDirCaps(OPDI_PORTDIRCAP_OUTPUT);
	} else if (portDirCaps == "Bidi") {
		port->setDirCaps(OPDI_PORTDIRCAP_BIDI);
	} else if (portDirCaps != "")
		this->throwSettingException(port->ID() + ": Unknown DirCaps specified; expected 'Input', 'Output' or 'Bidi'", portDirCaps);

/*	port flags are managed internally

	int flags = portConfig->getInt("Flags", -1);
	if (flags >= 0) {
		port->setFlags(flags);
	} else
		// default flags specified?
		// avoid calling setFlags unnecessarily because subclasses may implement specific behavior
		if (defaultFlags > 0)
			port->setFlags(defaultFlags);
*/
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
			this->throwSettingException(port->ID() + ": Unknown RefreshMode specified; expected 'Off', 'Periodic' or 'Auto': " + refreshMode);

	if (port->getRefreshMode() == opdi::Port::RefreshMode::REFRESH_PERIODIC) {
		int time = portConfig->getInt("RefreshTime", -1);
		if (time >= 0) {
			port->setPeriodicRefreshTime(time);
		} else {
			this->throwSettingException(port->ID() + ": A RefreshTime > 0 must be specified in Periodic refresh mode: " + to_string(time));
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

	port->tags = this->getConfigString(portConfig, port->ID(), "Tags", "", false);

	port->orderID = portConfig->getInt("OrderID", -1);

	port->onChangeIntPortsStr = this->getConfigString(portConfig, port->ID(), "OnChangeInt", "", false);
	port->onChangeUserPortsStr = this->getConfigString(portConfig, port->ID(), "OnChangeUser", "", false);

	port->setLogVerbosity(this->getConfigLogVerbosity(portConfig, this->logVerbosity));
}

void AbstractOpenHAT::configureDigitalPort(ConfigurationView* portConfig, opdi::DigitalPort* port, bool stateOnly) {
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
		this->throwSettingException("Unknown Mode specified; expected 'Input', 'Input with pullup', 'Input with pulldown', or 'Output'", portMode);

	std::string portLine = this->getConfigString(stateConfig, port->ID(), "Line", "", false);
	if (portLine == "High") {
		port->setLine(1);
	} else if (portLine == "Low") {
		port->setLine(0);
	} else if (portLine != "")
		this->throwSettingException("Unknown Line specified; expected 'Low' or 'High'", portLine);
}

void AbstractOpenHAT::setupEmulatedDigitalPort(ConfigurationView* portConfig, const std::string& port) {
	this->logDebug("Setting up emulated digital port: " + port);

	opdi::DigitalPort* digPort = new opdi::DigitalPort(port.c_str());
	this->configureDigitalPort(portConfig, digPort);

	this->addPort(digPort);
}

void AbstractOpenHAT::configureAnalogPort(ConfigurationView* portConfig, opdi::AnalogPort* port, bool stateOnly) {
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

void AbstractOpenHAT::setupEmulatedAnalogPort(ConfigurationView* portConfig, const std::string& port) {
	this->logDebug("Setting up emulated analog port: " + port);

	opdi::AnalogPort* anaPort = new opdi::AnalogPort(port.c_str());
	this->configureAnalogPort(portConfig, anaPort);

	this->addPort(anaPort);
}

void AbstractOpenHAT::configureSelectPort(ConfigurationView* portConfig, ConfigurationView* parentConfig, opdi::SelectPort* port, bool stateOnly) {
	if (!stateOnly) {
		this->configurePort(portConfig, port, 0);

		// the select port requires a prefix or section "<portID>.Labels"
		Poco::AutoPtr<ConfigurationView> portLabels = this->createConfigView(portConfig, "Labels");
		portConfig->addUsedKey("Labels");

		// get ordered list of items
		ConfigurationView::Keys labelKeys;
		portLabels->keys("", labelKeys);

		typedef Poco::Tuple<int, std::string> Label;
		typedef std::vector<Label> LabelList;
		LabelList orderedLabels;

		// create ordered list of labels (by priority)
		for (auto it = labelKeys.begin(), ite = labelKeys.end(); it != ite; ++it) {
			int labelNumber = portLabels->getInt(*it, 0);
			// check whether the label is active
			if (labelNumber < 0)
				continue;

			// insert at the correct position to create a sorted list of items
			auto nli = orderedLabels.begin();
			auto nlie = orderedLabels.end();
			while (nli != nlie) {
				if (nli->get<0>() > labelNumber)
					break;
				++nli;
			}
			Label label(labelNumber, *it);
			orderedLabels.insert(nli, label);
		}

		if (orderedLabels.size() == 0)
			this->throwSettingException("The select port " + std::string(port->ID()) + " requires at least one label in its config section", std::string(port->ID()) + ".Label");

		// go through items, create ordered list of char* items
		std::vector<const char*> charLabels;
		auto nli = orderedLabels.begin();
		auto nlie = orderedLabels.end();
		while (nli != nlie) {
			charLabels.push_back(nli->get<1>().c_str());
			++nli;
		}
		charLabels.push_back(nullptr);

		// set port labels
		port->setLabels(&charLabels[0]);
	}

	Poco::AutoPtr<Poco::Util::AbstractConfiguration> stateConfig = this->getConfigForState(portConfig, port->ID());

	if (stateConfig->getString("Position", "") != "") {
		int16_t position = stateConfig->getInt("Position", 0);
		if ((position < 0) || (position > port->getMaxPosition()))
			this->throwSettingException("Wrong select port setting: Position is out of range: " + to_string(position));
		port->setPosition(position);
	}
}

void AbstractOpenHAT::setupEmulatedSelectPort(ConfigurationView* portConfig, ConfigurationView* parentConfig, const std::string& port) {
	this->logDebug("Setting up emulated select port: " + port);

	opdi::SelectPort* selPort = new opdi::SelectPort(port.c_str());
	this->configureSelectPort(portConfig, parentConfig, selPort);

	this->addPort(selPort);
}

void AbstractOpenHAT::configureDialPort(ConfigurationView* portConfig, opdi::DialPort* port, bool stateOnly) {
	if (!stateOnly) {
		this->configurePort(portConfig, port, 0);

		int64_t min = portConfig->getInt64("Minimum", port->getMin());
		int64_t max = portConfig->getInt64("Maximum", port->getMax());
		if (min >= max)
			this->throwSettingException("Wrong dial port setting: Maximum (" + to_string(max) + ") must be greater than Minimum (" + to_string(min) + ")");
		int64_t step = portConfig->getInt64("Step", port->getStep());
		if (step < 1)
			this->throwSettingException("Wrong dial port setting: Step may not be negative or zero: " + to_string(step));

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
				this->throwSettingException(port->ID() + ": Value for 'History' not supported, expected 'Hour' or 'Day': " + history);
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

	int64_t position = stateConfig->getInt64("Position", port->getPosition());
	// set port error to invalid if the value is out of range
	if ((position < port->getMin()) || (position > port->getMax()))
		port->setError(opdi::Port::Error::VALUE_NOT_AVAILABLE);
		//this->throwSettingException("Wrong dial port setting: Position is out of range: " + to_string(position));
	else
		port->setPosition(position);
}

void AbstractOpenHAT::setupEmulatedDialPort(ConfigurationView* portConfig, const std::string& port) {
	this->logDebug("Setting up emulated dial port: " + port);

	opdi::DialPort* dialPort = new opdi::DialPort(port.c_str());
	this->configureDialPort(portConfig, dialPort);

	this->addPort(dialPort);
}

void AbstractOpenHAT::configureStreamingPort(ConfigurationView* portConfig, opdi::StreamingPort* port) {
	this->configurePort(portConfig, port, 0);
}

void AbstractOpenHAT::setupSerialStreamingPort(ConfigurationView* portConfig, const std::string& port) {
	this->logDebug("Setting up serial streaming port: " + port);

	SerialStreamingPort* ssPort = new SerialStreamingPort(this, port.c_str());
	ssPort->configure(portConfig);

	this->addPort(ssPort);
}


template <typename PortType>
void AbstractOpenHAT::setupPort(ConfigurationView* portConfig, const std::string& portID) {
#ifdef __GNUG__
	int status;
	char* realname;
	realname = abi::__cxa_demangle(typeid(PortType).name(), 0, 0, &status);
	std::string typeName(realname);
	free(realname);
#else
	std::string typeName(typeid(PortType).name());
#endif
	this->logDebug("Setting up port: " + portID + " of type: " + typeName);

	PortType* port = new PortType(this, portID.c_str());
	port->configure(portConfig);

	this->addPort(port);
}

template <typename PortType>
void AbstractOpenHAT::setupPortEx(ConfigurationView* portConfig, ConfigurationView* parentConfig, const std::string& portID) {
#ifdef __GNUG__
	int status;
	char* realname;
	realname = abi::__cxa_demangle(typeid(PortType).name(), 0, 0, &status);
	std::string typeName(realname);
	free(realname);
#else
	std::string typeName(typeid(PortType).name());
#endif
	this->logDebug("Setting up port: " + portID + " of type: " + typeName);

	PortType* port = new PortType(this, portID.c_str());
	port->configure(portConfig, parentConfig);

	this->addPort(port);
}

void AbstractOpenHAT::setupNode(ConfigurationView* config, const std::string& node) {
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
	Poco::AutoPtr<ConfigurationView> nodeConfig = this->createConfigView(config, node);

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

		// plugins check their own settings usage, so the nodeConfig does not need to
		// check for unused keys
		nodeConfig->setCheckUnused(false);

		// init the plugin
		plugin->setupPlugin(this, node, nodeConfig, config, nodeDriver);

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
		this->setupPort<LoggerPort>(nodeConfig, node);
	} else
	if (nodeType == "Logic") {
		this->setupPort<LogicPort>(nodeConfig, node);
	} else
	if (nodeType == "Pulse") {
		this->setupPort<PulsePort>(nodeConfig, node);
	} else
	if (nodeType == "Selector") {
		this->setupPort<SelectorPort>(nodeConfig, node);
#ifdef OPENHAT_USE_EXPRTK
	} else
	if (nodeType == "Expression") {
		this->setupPort<ExpressionPort>(nodeConfig, node);
#else
#pragma message( "Expression library not included, cannot use the Expression node type" )
#endif	// def OPENHAT_USE_EXPRTK
	} else
	if (nodeType == "Timer") {
		this->setupPortEx<TimerPort>(nodeConfig, config, node);
	} else
	if (nodeType == "ErrorDetector") {
		this->setupPort<ErrorDetectorPort>(nodeConfig, node);
	} else
	if (nodeType == "Fader") {
		this->setupPort<FaderPort>(nodeConfig, node);
	} else
	if (nodeType == "Exec") {
		this->setupPort<ExecPort>(nodeConfig, node);
	} else
	if (nodeType == "SceneSelect") {
		this->setupPortEx<SceneSelectPort>(nodeConfig, config, node);
	} else
	if (nodeType == "File") {
		this->setupPortEx<FilePort>(nodeConfig, config, node);
	} else
	if (nodeType == "Aggregator") {
		this->setupPortEx<AggregatorPort>(nodeConfig, config, node);
	} else
	if (nodeType == "Trigger") {
		this->setupPort<TriggerPort>(nodeConfig, node);
	} else
	if (nodeType == "Counter") {
		this->setupPort<CounterPort>(nodeConfig, node);
	} else
	if (nodeType == "InfluxDB") {
		this->setupPort<InfluxDBPort>(nodeConfig, node);
	}
	else
	if (nodeType == "Test") {
		this->setupPortEx<TestPort>(nodeConfig, config, node);
	}
	else
		this->throwSettingException("Invalid configuration: Unknown node type: " + nodeType);
}

void AbstractOpenHAT::setupRoot(ConfigurationView* config) {
	// enumerate section "Root"
	Poco::AutoPtr<ConfigurationView> nodes = this->createConfigView(config, "Root");
	this->logVerbose("Setting up root nodes");

	ConfigurationView::Keys nodeKeys;
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

int AbstractOpenHAT::setupConnection(ConfigurationView* configuration, bool testMode) {
	this->logVerbose(std::string("Setting up connection for slave: ") + this->slaveName);
	Poco::AutoPtr<ConfigurationView> config = this->createConfigView(configuration, "Connection");

	std::string connectionTransport = this->getConfigString(config, "Connection", "Transport", "", true);
	this->connectionLogVerbosity = this->getConfigLogVerbosity(config, this->logVerbosity);

	if (connectionTransport == "TCP") {
		std::string interface_ = this->getConfigString(config, "Connection", "Interface", "*", false);

		if (interface_ != "*")
			this->throwSettingException("Connection interface: Sorry, values other than '*' are currently not supported");

		int port = config->getInt("Port", DEFAULT_TCP_PORT);

		// test mode causes immediate exit
		if (testMode)
			return OPDI_STATUS_OK;

		// check unused keys here (before starting the connection loop)
		std::vector<std::string> unusedKeys;
		config->getUnusedKeys(unusedKeys);
		if (unusedKeys.size() > 0)
			this->unusedConfigKeysDetected("Connection", unusedKeys);
		config->setCheckUnused(false);	// no check needed afterwards

		return this->setupTCP(interface_, port);
	}
	else
		this->throwSettingException("Invalid configuration; unknown connection transport", connectionTransport);
	return OPDI_STATUS_OK;
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
		// allow some grace period (five minutes)
		if (buildTime.timestamp() - driverFile.getLastModified() > 5 * 60 * 1000000)	// microseconds
			this->logNormal("Warning: Plugin module " + driver + " is older than the main binary; possible ABI conflict! In case of strange errors please recompile the plugin!");
	}
}

uint8_t AbstractOpenHAT::waiting(uint8_t canSend) {
	uint8_t result;

	this->currentFrame++;

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
		this->logError(std::string("Unhandled exception while housekeeping: ") + this->getExceptionMessage(pe));
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
	} catch (Poco::Exception& e) {
		this->logWarning("Unable to persist state of port " + port->ID() + ": " + this->getExceptionMessage(e));
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

std::string AbstractOpenHAT::getExceptionMessage(Poco::Exception& e) {
	std::string what(e.what());
#ifdef __GNUG__
	int status;
	char* realname;
	realname = abi::__cxa_demangle(e.className(), 0, 0, &status);
	std::string typeName(realname);
	free(realname);
#else
	std::string typeName(e.className());
#endif
	return (e.message().empty() ? "" : ": " + e.message()) + (what.empty() ? "" : " " + what)
		+ (typeName.empty() ? "" : " (" + typeName + ")");
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
	if (Opdi->connectionLogVerbosity < opdi::LogVerbosity::DEBUG)
		return OPDI_STATUS_OK;
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
	} catch (opdi::Port::ValueUnavailableException) {
		// TODO localize message
		opdi_set_port_message("Value unavailable");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::ValueExpiredException) {
		// TODO localize message
		opdi_set_port_message("Value expired");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDeniedException &ad) {
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
	} catch (opdi::Port::AccessDeniedException &ad) {
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
	} catch (opdi::Port::AccessDeniedException &ad) {
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
	} catch (opdi::Port::ValueUnavailableException) {
		// TODO localize message
		opdi_set_port_message("Value unavailable");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::ValueExpiredException) {
		// TODO localize message
		opdi_set_port_message("Value expired");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDeniedException &ad) {
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
	} catch (opdi::Port::AccessDeniedException &ad) {
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
	} catch (opdi::Port::AccessDeniedException &ad) {
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
	} catch (opdi::Port::AccessDeniedException &ad) {
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
	} catch (opdi::Port::AccessDeniedException &ad) {
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
	} catch (opdi::Port::ValueUnavailableException) {
		// TODO localize message
		opdi_set_port_message("Value unavailable");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::ValueExpiredException) {
		// TODO localize message
		opdi_set_port_message("Value expired");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDeniedException &ad) {
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
	} catch (opdi::Port::AccessDeniedException &ad) {
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
	} catch (opdi::Port::ValueUnavailableException) {
		// TODO localize message
		opdi_set_port_message("Value unavailable");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::ValueExpiredException) {
		// TODO localize message
		opdi_set_port_message("Value expired");
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::PortError &pe) {
		opdi_set_port_message(pe.message().c_str());
		return OPDI_PORT_ERROR;
	} catch (opdi::Port::AccessDeniedException &ad) {
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
	} catch (opdi::Port::AccessDeniedException &ad) {
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
