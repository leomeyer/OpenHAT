#pragma once

#include <sstream>
#include <list>

#include "Poco/Mutex.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/Util/PropertyFileConfiguration.h"
#include "Poco/Logger.h"
#include "Poco/Stopwatch.h"
#include "Poco/BasicEvent.h"
#include "Poco/Delegate.h"
#include "Poco/Exception.h"

#include "opdi_constants.h"
#include "opdi_platformfuncs.h"
#include "opdi_configspecs.h"
#include "OPDI.h"

#include "Configuration.h"

// protocol callback function for the OPDI slave implementation
extern void protocol_callback(uint8_t state);

namespace openhat {
	class AbstractOpenHAT;
}

/// The interface that plugins must implement.
/// 
struct IOpenHATPlugin {
	/// This method is used to setup the plugin from the current configuration.
	/// nodeName is the plugin node's section identifier.
	/// nodeConfig is the configuration view that was created for the plugin section.
	/// parentConfig is the parent configuration view of the plugin section.
	/// driverPath is the actual resolved dynamic library path that has been used to load the plugin.
	virtual void setupPlugin(openhat::AbstractOpenHAT* openHAT, const std::string& nodeName, openhat::ConfigurationView::Ptr nodeConfig, openhat::ConfigurationView::Ptr parentConfig, const std::string& driverPath) = 0;

        virtual void terminate(void) {};
        
	// virtual destructor (called when the plugin is deleted)
	virtual ~IOpenHATPlugin() {}
};

namespace openhat {

#define OPENHATD_INTERRUPTED	127
#define OPENHATD_TEST_FAILED	128
#define OPENHATD_TEST_EXIT	129

#define OPENHAT_CONFIG_FILE_SETTING	"__OPENHATD_CONFIG_FILE_PATH"

constexpr char OPENHAT_VERSION_ID[] =
#include "VERSION"
;
// C++14: when C++11 constexpr limitations have been lifted, this code can be changed 
// to support more than one-digit version number components
constexpr int OPENHAT_MAJOR_VERSION = OPENHAT_VERSION_ID[0] - '0';
constexpr int OPENHAT_MINOR_VERSION = OPENHAT_VERSION_ID[2] - '0';
constexpr int OPENHAT_PATCH_VERSION = OPENHAT_VERSION_ID[4] - '0';

/// This exception is thrown when an error occurs while processing a configuration setting.
///
class SettingException : public Poco::Exception
{
public:
	explicit SettingException(std::string message, const std::string& detail = "") : Poco::Exception(message, detail) {};
};

/// The listener interface for plugin registrations.
///
struct IConnectionListener {
	/// Is called when a master has successfully completed the handshake.
	///
	virtual void masterConnected(void) = 0;

	/// Is called when a master has disconnected.
	///
	virtual void masterDisconnected(void) = 0;
};

/// The abstract base class for OpenHAT implementations.
///
class AbstractOpenHAT: public opdi::OPDI {
protected:
	int majorVersion;
	int minorVersion;
	int patchVersion;

	bool allowHiddenPorts;

	bool prepared;

	// user and password for master authentication (set from the configuration)
	std::string loginUser;
	std::string loginPassword;

	std::string deviceInfo;

	// environment variables for config file substitution (keys prefixed with $)
	std::map<std::string, std::string> environment;

	Poco::Mutex mutex;
	Poco::Logger* logger;

	typedef std::list<IConnectionListener*> ConnectionListenerList;
	ConnectionListenerList connectionListeners;

	typedef std::map<uint8_t, std::string> ResultCodeTexts;
	ResultCodeTexts resultCodeTexts;

	typedef std::map<std::string, std::string> LockedResources;
	LockedResources lockedResources;

	typedef std::vector<IOpenHATPlugin*> PluginList;
	PluginList pluginList;
        
        uint8_t defaultPortPriority;

	// internal status monitoring variables
	static const int maxSecondStats = 1100;
	uint64_t* monSecondStats;				// doWork performance statistics buffer
	int monSecondPos;						// current position in buffer
	Poco::Stopwatch idleStopwatch;			// measures time until doWork() is called again
	uint64_t totalMicroseconds;				// total time (doWork + idle)
	int doWorkCallsPerSecond;				// number of calls to doWork()
	uint64_t currentFrame;					// number of the current doWork iteration ("frame")
	double framesPerSecond;					// average number of doWork iterations processed per second
	int targetFramesPerSecond;				// target number of doWork iterations per second

	std::string heartbeatFile;

	bool suppressUnusedParameterMessages;
	std::string lastConfigKeyAccessMessage;		// used to suppress subsequent identical messages from nested configurations

	virtual uint8_t idleTimeoutReached(void) override;

	virtual ConfigurationView::Ptr readConfiguration(const std::string& fileName, const std::map<std::string, std::string>& parameters);

	/** Outputs a log message with a timestamp. */
	virtual void log(const std::string& text);

	virtual void logErr(const std::string& message);

	virtual void logWarn(const std::string& message);
        
        virtual uint8_t shutdownInternal(void) override;
public:
	std::string appName;
	
	std::string timestampFormat;

	Poco::BasicEvent<void> allPortsRefreshed;
	Poco::BasicEvent<opdi::Port*> portRefreshed;

	// configuration file for port state persistence
	std::string persistentConfigFile;
	Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> persistentConfig;

	opdi::LogVerbosity connectionLogVerbosity;

	AbstractOpenHAT(void);

	inline bool isPrepared() { return this->prepared; };

	virtual void protocolCallback(uint8_t protState);

	virtual void connected(void);

	virtual void disconnected(void);

	virtual void addConnectionListener(IConnectionListener* listener);

	virtual void removeConnectionListener(IConnectionListener* listener);

	/** Outputs a hello message. */
	virtual void sayHello(void);

	virtual void showHelp(void);

	/** Returns the current frame number. */
	virtual uint64_t getCurrentFrame(void);

	/** Returns the current user ID or name. */
	virtual std::string getCurrentUser(void) = 0;

	/** Modify the current process credentials to a less privileged user. */
	virtual void switchToUser(const std::string& newUser) = 0;

	virtual std::string getTimestampStr(void);

	virtual std::string getResultCodeText(uint8_t code);

	/** Returns the key's value from the configuration or the default value, if it is missing. If missing and isRequired is true, throws an exception. */
	virtual std::string getConfigString(Poco::Util::AbstractConfiguration::Ptr config, const std::string &section, const std::string &key, const std::string &defaultValue, const bool isRequired);

	/** Outputs the specified text to an implementation-dependent output with an appended line break. */
	virtual void println(const char* text) = 0;

	/** Outputs the specified text to an implementation-dependent output with an appended line break. */
	virtual void println(const std::string& text);

	/** Outputs the specified error text to an implementation-dependent error output with an appended line break. */
	virtual void printlne(const char* text) = 0;

	/** Outputs the specified error text to an implementation-dependent error output with an appended line break. */
	virtual void printlne(const std::string& text);

	/** Starts processing the supplied arguments. */
	virtual int startup(const std::vector<std::string>& args, const std::map<std::string, std::string>& environment);

	/** Attempts to lock the resource with the specified ID. The resource can be anything, a pin number, a file name or whatever.
	 *  When the same resource is attempted to be locked twice this method throws an exception.
	 *  Use this mechanism to avoid resource conflicts. */
	virtual void lockResource(const std::string& resourceID, const std::string& lockerID);

	virtual opdi::LogVerbosity getConfigLogVerbosity(ConfigurationView::Ptr config, opdi::LogVerbosity defaultVerbosity);

	/** Creates a configuration view with the specified view name. */
	virtual ConfigurationView::Ptr createConfigView(Poco::Util::AbstractConfiguration::Ptr baseConfig, const std::string& viewName);

	/** Returns the configuration that should be used for querying a port's state. This is the baseConfig if no
	 *  persistent configuration has been specified, or a layered configuration otherwise.
	 *	This configuration must be freed after use. The view name is optional. */
	virtual Poco::Util::AbstractConfiguration::Ptr getConfigForState(ConfigurationView::Ptr baseConfig, const std::string& viewName);

	/** Called by the destructor of ConfigurationView, do not throw exceptions in this method! */
	virtual void unusedConfigKeysDetected(const std::string& sourceFile, const std::string& viewName, const std::vector<std::string>& unusedKeys);

	/** Throws a SettingException. Suppresses unused config key messages afterwards. */
	virtual void throwSettingException(const std::string& message, const std::string& detail = "");

	/** Returns the name of the user to switch to, if specified in SwitchToUser */
	std::string setupGeneralConfiguration(ConfigurationView::Ptr config);

	virtual void configureEncryption(ConfigurationView::Ptr config);

	virtual void configureAuthentication(ConfigurationView::Ptr config);

	/** Reads common properties from the configuration and configures the port group. */
	virtual void configureGroup(ConfigurationView::Ptr groupConfig, opdi::PortGroup* group, int defaultFlags);

	virtual void setupGroup(ConfigurationView::Ptr groupConfig, const std::string& group);

	virtual std::string resolveRelativePath(ConfigurationView::Ptr config, const std::string& source, const std::string& path, 
		const std::string& defaultValue, const std::string& manualPath = "", const std::string& settingName = "RelativeTo");

	virtual void setupInclude(ConfigurationView::Ptr groupConfig, ConfigurationView::Ptr parentConfig, const std::string& node);

	/** Reads common properties from the configuration and configures the port. */
	virtual void configurePort(ConfigurationView::Ptr portConfig, opdi::Port* port, int defaultFlags);

	/** Reads special properties from the configuration and configures the digital port. */
	virtual void configureDigitalPort(ConfigurationView::Ptr portConfig, opdi::DigitalPort* port, bool stateOnly = false);

	virtual void setupDigitalPort(ConfigurationView::Ptr portConfig, const std::string& port);

	/** Reads special properties from the configuration and configures the analog port. */
	virtual void configureAnalogPort(ConfigurationView::Ptr portConfig, opdi::AnalogPort* port, bool stateOnly = false);

	virtual void setupAnalogPort(ConfigurationView::Ptr portConfig, const std::string& port);

	/** Reads special properties from the configuration and configures the select port. */
	virtual void configureSelectPort(ConfigurationView::Ptr portConfig, ConfigurationView::Ptr parentConfig, opdi::SelectPort* port, bool stateOnly = false);

	virtual void setupSelectPort(ConfigurationView::Ptr portConfig, ConfigurationView::Ptr parentConfig, const std::string& port);

	/** Reads special properties from the configuration and configures the dial port. */
	virtual void configureDialPort(ConfigurationView::Ptr portConfig, opdi::DialPort* port, bool stateOnly = false);

	virtual void setupDialPort(ConfigurationView::Ptr portConfig, const std::string& port);

	/** Reads special properties from the configuration and configures the streaming port. */
	virtual void configureStreamingPort(ConfigurationView::Ptr portConfig, opdi::StreamingPort* port);

	virtual void setupSerialStreamingPort(ConfigurationView::Ptr portConfig, const std::string& port);

	template <typename PortType> 
	void setupPort(ConfigurationView::Ptr portConfig, const std::string& port);

	template <typename PortType> 
	void setupPortEx(ConfigurationView::Ptr portConfig, ConfigurationView::Ptr parentConfig, const std::string& port);

	/** Configures the specified node. */
	virtual void setupNode(ConfigurationView::Ptr config, const std::string& node);

	/** Starts enumerating the nodes of the Root section and configures the nodes. */
	virtual void setupRoot(ConfigurationView::Ptr config);

	/** Sets up the connection from "Connection" section of the specified configuration. */
	virtual int setupConnection(ConfigurationView::Ptr configuration, bool testMode);

	/** Sets up a TCP listener and listens for incoming requests. This method does not return unless the program should exit. */
	virtual int setupTCP(const std::string& interface_, int port) = 0;

	/** Checks whether the supplied file is more recent than the current binary and logs a warning if yes. */
	virtual void warnIfPluginMoreRecent(const std::string& driver);

	/** Returns a pointer to the plugin object instance specified by the given driver. */
	virtual IOpenHATPlugin* getPlugin(const std::string& driver) = 0;

        /** Processes the ports' doWork methods. */
	virtual uint8_t doWork(uint8_t canSend, uint8_t* sleepTimeMs) override;

	/* Authenticate comparing the login data with the configuration login data. */
	virtual uint8_t setPassword(const std::string& password) override;

	virtual std::string getExtendedDeviceInfo(void) override;

	/** This implementation also logs the refreshed ports. */
	virtual uint8_t refresh(opdi::Port** ports) override;

	virtual void savePersistentConfig();

	/** Implements a persistence mechanism for port states. */
	virtual void persist(opdi::Port* port) override;

	/** Returns a string representing the port state; empty in case of errors. */
	virtual std::string getPortStateStr(opdi::Port* port) const;

	virtual std::string getDeviceInfo(void);

	virtual void getEnvironment(std::map<std::string, std::string>& mapToFill);

	virtual std::string getExceptionMessage(const Poco::Exception& e);

	/// Logs access to a config key with Debug log verbosity.
	/// Suppresses subsequent identical messages.
	virtual void logConfigKeyAccess(const std::string& message);
};

}		// namespace openhat

/** Define external singleton instance */
extern openhat::AbstractOpenHAT* Opdi;

