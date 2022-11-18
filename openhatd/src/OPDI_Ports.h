//    Copyright (C) 2011-2016 OpenHAT contributors (https://openhat.org, https://github.com/openhat-org)
//    All rights reserved.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <string>
#include <vector>
#include <ostream>
#include <sstream>

#include "Poco/Exception.h"
#include "Poco/Tuple.h"
#include "Poco/RegularExpression.h"
#include "Poco/NumberParser.h"
#include "Poco/Util/AbstractConfiguration.h"

#include "opdi_platformtypes.h"
#include "opdi_configspecs.h"

namespace opdi {
    
    const uint8_t DEFAULT_PORT_PRIORITY = 100;

	/// Defines the available log levels.
	///
	enum class LogVerbosity {
		UNKNOWN,
		QUIET,
		NORMAL,
		VERBOSE,
		DEBUG,
		EXTREME
	};

class OPDI;
class Port;
class DigitalPort;
class AnalogPort;
class SelectPort;
class DialPort;

typedef std::vector<Port*> PortList;
typedef std::vector<DigitalPort*> DigitalPortList;
typedef std::vector<AnalogPort*> AnalogPortList;

/// Base class for OPDI port wrappers.
///
/// This class is not intended to be used or extended directly. Rather, extend one of its
/// direct subclasses, such as DigitalPort or DialPort.
class Port {
	friend class OPDI;

public:
	/// Defines the origin of a state change.
	///
	enum class ChangeSource {
		CHANGESOURCE_INT,		/**< Change originated in internal function */
		CHANGESOURCE_USER		/**< Change originated in user action */
	};

	/// Defines the refresh mode of a port.
	///
	enum class RefreshMode : unsigned int {
		REFRESH_NOT_SET,		/**< Uninitialized value */
		REFRESH_OFF,			/**< No automatic refresh */
		REFRESH_PERIODIC,		/**< Time based refresh */
		REFRESH_AUTO			/**< Automatic refresh on state or error changes */
	};

	/// Possible error values that a port can have.
	///
	enum class Error {
		VALUE_OK,				/**< No error */
		VALUE_EXPIRED,			/**< The value has expired */
		VALUE_NOT_AVAILABLE		/**< The value is not available */
	};

	// disable copy constructor
	Port(const Port& that) = delete;

protected:
	char* id;
	char* label;
	char type[2];	// type constants are one character (see opdi_port.h)
	char caps[2];	// caps constants are one character (port direction constants)
	int32_t flags;
	void* ptr;
	bool hidden;
	bool readonly;
	uint8_t priority;
	double valueAsDouble;

	/// LogVerbosity setting. This setting usually overrides the main program's log verbosity.
	///
	LogVerbosity logVerbosity;

	/// The error value of a port.
	///
	Error error;

	/// Globally unique identifier for the port type. Necessary if another port needs to check
	/// that a given port is indeed of a required type. Is sent to the master as part of the
	/// extended info string.
	std::string typeGUID;

	/// The unit specification of the port. Is sent to the master as part of the
	/// extended info string.
	std::string unit;

	/// The color scheme specification of the port. Is sent to the master as part of the
	/// extended info string.
	std::string colorScheme;

	/// The icon specification of the port. Is sent to the master as part of the
	/// extended info string.
	std::string icon;

	/// The group that this port belongs to. Is sent to the master as part of the
	/// extended info string.
	std::string group;

	/// Total extended info string. Is updated when a setter method of a property
	/// that is a part of the extended info string changes its setting.
	std::string extendedInfo;

	/// Historic values of a port. Is sent to the master as part of the
	/// extended state string.
	std::string history;

        /// Indicates that the value of this port may not be accurate (extended port state).
        bool inaccurate;    

	/// Pointer to OPDI class instance.
	OPDI* opdi;

	/// OPDI implementation management structure. This pointer is managed by the OPDI class.
	/// Do not use this directly.
	void* data;

	/// The refresh mode specifies when this port should generate refresh messages to be sent.
	/// Default is REFRESH_NOT_SET.
	RefreshMode refreshMode;

	/// Can be set true if the need for a refresh is detected.
	/// This base class handles the case for periodic refreshes. If the refresh timer is up
	/// this flag is set to true.
	/// The standard ports set this flag when a state or error change has been detected.
	/// Normally it is not required to use this flag in subclasses directly if the usual
	/// state change functions are being used (e. g. setLine for a DigitalPort).
	/// The doWork method handles this flag and generates refreshes if necessary.
	bool refreshRequired;

	/// For refresh mode Periodic, the time in milliseconds between self-refresh messages.
	///
	uint32_t periodicRefreshTime;

	/// The last time a refresh has been generated for this port. Is used to avoid too many
	/// refreshes within a short time which may cause DOS.
	uint64_t lastRefreshTime;

	/// Indicates whether port state should be written to and loaded from a persistent storage.
	/// 
	bool persistent;

	/// Utility function for string conversion. Can be called directly for most data types
	/// except char which requires a conversion to int first, such as to_string((int)aChar).
	template <class T> std::string to_string(const T& t) const;

	/// protected constructor - for use by friend classes only
	///
	Port(const char* id, const char* type);

	/// protected constructor: This class can't be instantiated directly
	///
	Port(const char* id, const char* type, const char* dircaps, int32_t flags, void* ptr);

	/// Called regularly by the OPDI system. Enables the port to do work.
	/// Override this method in subclasses to implement more complex functionality.
	///
	/// In this method, an implementation may send asynchronous messages to the master ONLY if canSend is true.
	/// This includes messages like Resync, Refresh, Debug etc.
	/// If canSend is 0 (= false), it means that there is no master is connected or the system is in the middle
	/// of sending a message of its own. It is not safe to send messages if canSend = 0!
	/// If a refresh is necessary it should be signalled by setting refreshRequired = true.
	///
	/// Returning a value other than OPDI_STATUS_OK causes the message processing to exit.
	/// This will usually signal a device error to the master or cause the master to time out.
	///
	/// This base class uses doWork to implement the refresh timer. It calls doRefresh when the
	/// periodic refresh time has been reached. Implementations can decide in doRefresh how the
	/// refresh should be dealt with.
	virtual uint8_t doWork(uint8_t canSend);

	/// Performs actions necessary before shutting down. The default implementation calls persist() if 
	/// persistent is true and ignores any errors.
	virtual void shutdown(void);

	/// Override this method to implement specific persistence mechanisms. This default implementation 
	/// does nothing. Actual persistence handling is defined by subclasses.
	virtual void persist(void);

	/// Causes the port to be refreshed by sending a refresh message to a connected master.
	/// Only if the port is not hidden.
	virtual uint8_t refresh(void);

	/// Updates the extended info string of the port that will be sent to connected masters.
	///
	void updateExtendedInfo(void);

	/// Utility function for escaping special characters in extended strings.
	///
	std::string escapeKeyValueText(const std::string& str) const;

	/// Checks the error state and throws an exception.
	/// Should be used by subclasses in getState() methods.
	virtual void checkError(void) const;

	/// Sets the ID of the port to the specified value.
	/// It is safe to use this function during the initialization phase only.
	/// Once the system is running the port IDs should not be changed any more.
	void setID(const char* newID);

	/// This method must be called when the state changes in order to
	/// handle the onChange* functionality. The default port implementations
	/// automatically handle this.
	virtual void handleStateChange(ChangeSource changeSource);

	/// Finds the port with the specified portID. Delegates to the OPDI.findPort method.
	/// If portID.isEmpty() and required is true, throws an exception whose message refers to the configPort and setting. 
	/// This method is intended to be used during the preparation phase to resolve port specifications. 
	/// configPort will usually be the ID of the resolving port, and setting the 
	/// configuration parameter name that contained the portID specification.
	Port* findPort(const std::string& configPort, const std::string& setting, const std::string& portID, bool required);

	/// Finds the ports with the specified portIDs. Delegates to the OPDI.findPorts method.
	/// This method is intended to be used during the preparation phase to resolve port specifications. 
	/// configPort will usually be the ID of the resolving port, and setting the 
	/// configuration parameter name that contained the portIDs specification.
	void findPorts(const std::string& configPort, const std::string& setting, const std::string& portIDs, PortList& portList);

	/// Finds the Digital port with the specified portID. Delegates to the OPDI.findDigitalPort method.
	/// If portID.isEmpty() and required is true, throws an exception whose message refers to the configPort and setting. 
	/// This method is intended to be used during the preparation phase to resolve port specifications. 
	/// configPort will usually be the ID of the resolving port, and setting the 
	/// configuration parameter name that contained the portID specification.
	/// Throws an exception if the port is found but is not a Digital port.
	DigitalPort* findDigitalPort(const std::string& configPort, const std::string& setting, const std::string& portID, bool required);

	/// Finds the Digital ports with the specified portIDs. Delegates to the OPDI.findDigitalPorts method.
	/// This method is intended to be used during the preparation phase to resolve port specifications. 
	/// configPort will usually be the ID of the resolving port, and setting the 
	/// configuration parameter name that contained the portIDs specification.
	/// Throws an exception if a port is not found or a found port is not a Digital port.
	void findDigitalPorts(const std::string& configPort, const std::string& setting, const std::string& portIDs, DigitalPortList& portList);

	/// Finds the Analog port with the specified portID. Delegates to the OPDI.findAnalogPort method.
	/// If portID.isEmpty() and required is true, throws an exception whose message refers to the configPort and setting. 
	/// This method is intended to be used during the preparation phase to resolve port specifications. 
	/// configPort will usually be the ID of the resolving port, and setting the 
	/// configuration parameter name that contained the portID specification.
	/// Throws an exception if the port is found but is not an Analog port.
	AnalogPort* findAnalogPort(const std::string& configPort, const std::string& setting, const std::string& portID, bool required);

	/// Finds the Analog ports with the specified portIDs. Delegates to the OPDI.findAnalogPorts method.
	/// This method is intended to be used during the preparation phase to resolve port specifications. 
	/// configPort will usually be the ID of the resolving port, and setting the 
	/// configuration parameter name that contained the portIDs specification.
	/// Throws an exception if a port is not found or a found port is not an Analog port.
	void findAnalogPorts(const std::string& configPort, const std::string& setting, const std::string& portIDs, AnalogPortList& portList);

	/// Finds the Select port with the specified portID. Delegates to the OPDI.findSelectPort method.
	/// If portID.isEmpty() and required is true, throws an exception whose message refers to the configPort and setting. 
	/// This method is intended to be used during the preparation phase to resolve port specifications. 
	/// configPort will usually be the ID of the resolving port, and setting the 
	/// configuration parameter name that contained the portID specification.
	/// Throws an exception if the port is found but is not a Select port.
	SelectPort* findSelectPort(const std::string& configPort, const std::string& setting, const std::string& portID, bool required);

	/// Finds the Dial port with the specified portID. Delegates to the OPDI.findDialPort method.
	/// If portID.isEmpty() and required is true, throws an exception whose message refers to the configPort and setting. 
	/// This method is intended to be used during the preparation phase to resolve port specifications. 
	/// configPort will usually be the ID of the resolving port, and setting the 
	/// configuration parameter name that contained the portID specification.
	/// Throws an exception if the port is found but is not a Dial port.
	DialPort* findDialPort(const std::string& configPort, const std::string& setting, const std::string& portID, bool required);

	/// Logs a message with log verbosity Warning.
	///
	void logWarning(const std::string& message);

	/// Logs a message with log verbosity Normal.
	///
	void logNormal(const std::string& message);

	/// Logs a message with log verbosity Verbose.
	///
	void logVerbose(const std::string& message);

	/// Logs a message with log verbosity Debug.
	///
	void logDebug(const std::string& message);

	/// Logs a message with log verbosity Extreme.
	///
	void logExtreme(const std::string& message);

	/// Returns the text for the specified changeSource.
	///
	std::string getChangeSourceText(ChangeSource changeSource);

	/// Used by testValue to reduce code duplication.
	///
	void compareProperty(const std::string& property, const std::string& expectedValue, const std::string& actualValue);

	/// Used by testValue to reduce code duplication.
	///
	void compareProperty(const std::string& property, const std::string& expectedValue, bool actualValue);

public:
	/// Virtual destructor for the port. Required for destructors of derived classes to be called.
	///
	virtual ~Port();

	/// This exception can be used by implementations to indicate an error during a port operation.
	/// Its message will be transferred to the master.
	class PortError : public Poco::Exception
	{
	public:
		explicit PortError(std::string message) : Poco::Exception(message) {};
	};

	/// This exception can be used by implementations to indicate that the value has expired.
	///
	class ValueExpiredException : public PortError
	{
	public:
		ValueExpiredException(const std::string& portID) : PortError(portID + ": The value has expired") {};
	};

	/// This exception can be used by implementations to indicate that no value is available.
	///
	class ValueUnavailableException : public PortError
	{
	public:
		ValueUnavailableException(const std::string& portID) : PortError(portID + ": The value is unavailable") {};
	};

	/// This exception can be used by implementations to indicate that a port operation is not allowed.
	/// Its message will be transferred to the master.
	class AccessDeniedException : public Poco::Exception
	{
	public:
		explicit AccessDeniedException(const std::string& message) : Poco::Exception(message) {};
	};

	/// This exception is thrown by the testValue() method when an unknown property is encountered.
	class UnknownPropertyException : public Poco::Exception
	{
	public:
		explicit UnknownPropertyException(const std::string& portID, const std::string& property) :
			Poco::Exception(portID + ": Unknown property '" + property + "'") {};
	};


	/// This exception is thrown by the testValue() method when a mismatching value is encountered.
	class TestValueMismatchException : public Poco::Exception
	{
	public:
		explicit TestValueMismatchException(const std::string& portID, const std::string& property, const std::string& expectedValue, const std::string& actualValue) : 
			Poco::Exception(portID + ": The expected value of property '" + property + "' ('" + expectedValue + "') did not match the actual value of '" + actualValue + "'") {};
	};

	/// Used internally to provide display ordering on ports
	///
	int orderID;

	/// A list of space-separated keywords
	///
	std::string tags;

	std::string onChangeIntPortsStr;
	// List of digital ports that are to be set to High if a change with source "internal" occurs
	// The lists are to be set from external code but handled internally (this->handleStateChange).
	DigitalPortList onChangeIntPorts;

	std::string onChangeUserPortsStr;
	// List of digital ports that are to be set to High if a change with source "user" occurs
	// The lists are to be set from external code but handled internally (this->handleStateChange).
	DigitalPortList onChangeUserPorts;

	/// Returns the port ID as a char*.
	///
	const char* getID(void) const;

	/// Returns the port ID as a std::string.
	///
	std::string ID(void) const;

	/// Returns the port type as a constant of value OPDI_PORTTYPE_*.
	///
	const char* getType(void) const;

	/// Sets the hidden flag of the port.
	/// If a port is hidden it is not included in the device capabilities as queried by a master.
	///
	void setHidden(bool hidden);

	/// Returns the hidden flag of the port.
	///
	bool isHidden(void) const;

	/// Sets the readonly flag of the port.
	/// If a port is readonly its state cannot be changed by a master.
	///
	void setReadonly(bool readonly);

	/// Returns the readonly flag of the port.
	///
	bool isReadonly(void) const;

	/// Sets the persistent flag of the port.
	///
	void setPersistent(bool persistent);

	/// Returns the persistent flag of the port.
	///
	bool isPersistent(void) const;

	/// Sets the label of the port.
	///
	void setLabel(const char* label);

	/// Returns the label of the port.
	///
	const char* getLabel(void) const;

	/// Sets the direction capabilities of the port.
	///
	void setDirCaps(const char* dirCaps);

	/// Returns the direction capabilities of the port.
	///
	const char* getDirCaps(void) const;

	/// Sets the flags of the port.
	///
	void setFlags(int32_t flags);

	/// Returns the flags of the port.
	///
	int32_t getFlags(void) const;

	/// Sets the type GUID of the port.
	///
	void setTypeGUID(const std::string& guid);

	/// Returns the type GUID of the port.
	///
	const std::string& getTypeGUID(void) const;

	/// Sets the unit of the port.
	/// The unit is an identifier that needs to be correctly interpreted by a GUI.
	void setUnit(const std::string& unit);

	/// Returns the unit of the port.
	///
	const std::string& getUnit(void) const;

	/// Sets the color scheme of the port.
	/// The color scheme is a specification that needs to be correctly interpreted by a GUI.
	void setColorScheme(const std::string& colorScheme);

	/// Returns the color scheme of the port.
	///
	const std::string& getColorScheme(void) const;

	/// Sets the icon of the port.
	/// The icon is an identifier that needs to be correctly interpreted by a GUI.
	void setIcon(const std::string& icon);

	/// Returns the icon of the port.
	///
	const std::string& getIcon(void) const;

	/// Sets the group of the port.
	///
	void setGroup(const std::string& group);

	/// Returns the group of the port.
	///
	const std::string& getGroup(void) const;

	/// Sets the history of the port.
	/// The history consists of an ordered set of values that have been collected in the specified interval.
	/// maxCount specifies the total number of values that are collected for this port; the size of values may be less.
	/// The history is not collected by the port itself. It needs to be set by an external component.
	void setHistory(uint64_t intervalSeconds, int maxCount, const std::vector<int64_t>& values);

	/// Returns a string representation of the port's history.
	///
	const std::string& getHistory(void) const;

	/// Clears the port history.
	///
	void clearHistory(void);
        
        bool isInaccurate(void) const;
        
        void setInaccurate(bool inaccurate);

	/// Returns the extended state of the port.
	/// The extended state is a set of name=value pairs.
	virtual std::string getExtendedState(bool withHistory = false) const;

	/// Returns the extended info of the port.
	/// The extended state is a set of name=value pairs.
	virtual std::string getExtendedInfo(void) const;

	/// Sets the log verbosity of the port.
	///
	void setLogVerbosity(LogVerbosity newLogVerbosity);

	/// Returns the log verbosity of the port.
	///
	LogVerbosity getLogVerbosity(void) const;

	/// Sets the refresh mode of the port.
	///
	void setRefreshMode(RefreshMode refreshMode);

	/// Sets the refresh mode of the port.
	///
	RefreshMode getRefreshMode(void) const;

	/// Sets the minimum time in milliseconds between self-refresh messages. If this time is 0 (default),
	/// the self-refresh is disabled. Only effective if the refresh mode equals "Periodic".
	virtual void setPeriodicRefreshTime(uint32_t timeInMs);

	/// This method should be called before the OPDI system is ready to start the doWork loop.
	/// It gives the port the chance to do necessary initializations.
	/// Port implementation may override this method to implement their own preparations.
	virtual void prepare(void);

	/// Sets the error state of the port.
	///
	virtual void setError(Error error);

	/// Returns the error state of the port.
	///
	virtual Port::Error getError(void) const;

	/// This method returns true if the port is in an error state. This will likely be the case
	/// when the getState() method of the port throws an exception.
	/// Subclasses may override this method to implement their own behaviour.
	virtual bool hasError(void) const = 0;

	/// This method is used to test values of port properties. Subclasses can override this method
	/// to implement support for their own properties. If a property values does not match the 
	/// expected value the method should throw a TestValueMismatchException. It may also throw
	/// other exceptions that should be derived from Poco::Exception.
	/// The property names should match the configuration setting names as specified in the 
	/// respective configure() method if applicable.
	virtual void testValue(const std::string& property, const std::string& expectedValue);
        
    /// Sets the priority requirement of this port. 0 means real time.
    ///
    virtual void setPriority(uint8_t priority);

    /// Returns the priority requirement of this port. 0 means real time.
    ///
    virtual uint8_t getPriority(void);

	/// <summary>
	/// Returns the value pointer (for ExpressionPort optimization).
	/// Is only valid as long as the port has not been freed!
	/// </summary>
	/// <returns></returns>
	double& getValuePtr() { return this->valueAsDouble; }
};

inline std::ostream& operator<<(std::ostream& oStream, const Port::Error error) {
	switch (error) {
	case Port::Error::VALUE_OK: oStream << "VALUE_OK"; break;
	case Port::Error::VALUE_EXPIRED: oStream << "VALUE_EXPIRED"; break;
	case Port::Error::VALUE_NOT_AVAILABLE: oStream << "VALUE_NOT_AVAILABLE"; break;
	default: oStream << "<unknown error>"; break;
	}
	return oStream;
};


template <class T> inline std::string Port::to_string(const T& t) const {
	std::stringstream ss;
	ss << t;
	return ss.str();
}

/// This class encapsulates the functions and behaviour of a port group.
///
class PortGroup {
	friend class OPDI;

protected:
	/// Internal variable. Do not use.
	///
	char* id;
	/// Internal variable. Do not use.
	///
	char* label;
	/// Internal variable. Do not use.
	///
	char* parent;
	/// Internal variable. Do not use.
	///
	int32_t flags;
	/// Internal variable. Do not use.
	///
	char* extendedInfo;

	/// Pointer to OPDI class instance.
	///
	OPDI* opdi;

	/// OPDI implementation management structure.
	///
	void* data;

	/// Linked list of port groups - pointer to next port group.
	///
	PortGroup* next;

	/// Internal variable. Do not use.
	///
	std::string icon;

	/// Updates the extended info string of the port that will be sent to connected masters.
	///
	virtual void updateExtendedInfo(void);

public:
	/// Port group constructor. Requires a port group ID.
	///
	explicit PortGroup(const char* id);

	/// Virtual destructor for the port group.
	///
	virtual ~PortGroup();

	/// Returns the ID of the port group.
	///
	const char* getID(void);

	/// Sets the label of the port group.
	///
	void setLabel(const char* label);

	/// Returns the label of the port group.
	///
	const char* getLabel(void);

	/// Sets the parent of the port group.
	///
	void setParent(const char* parent);

	/// Returns the parent of the port group.
	///
	const char* getParent(void);

	/// Sets the flags of the port group.
	///
	void setFlags(int32_t flags);

	/// Sets the icon of the port group.
	/// The icon is an identifier that needs to be correctly interpreted by a GUI.
	void setIcon(const std::string& icon);
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Port definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Defines a Digital port.
///
class DigitalPort : public Port {
	friend class OPDI;

private:
	uint8_t mode;
	uint8_t line;

public:
	/// Constructs a Digital port with default settings. The ID is required.
	/// Default settings are: DirCaps = BiDi, Mode = Input, Flags = 0.
	explicit DigitalPort(const char* id);

	/// Constructs a Digital port. Specify one of the OPDI_PORTDIRCAPS_* values for dircaps.
	/// Specify one or more of the OPDI_DIGITAL_PORT_* values for flags, or'ed together, to specify pullup/pulldown resistors.
	DigitalPort(const char* id, const char*  dircaps, const int32_t flags);

	/// Sets the direction capabilities of the port.
	///
	void setDirCaps(const char* dirCaps);

	/// Sets the port mode (opdi_set_digital_port_mode).
	/// mode = 0: floating input
	/// mode = 1: input with pullup on
	/// mode = 2: not supported
	/// mode = 3: output
	virtual void setMode(uint8_t mode, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	/// Returns the current port mode.
	///
	virtual uint8_t getMode(void);

	/// Sets the port line (opdi_set_digital_port_line).
	/// line = 0: low
	/// line = 1: high
	/// Returns whether the state has changed.
	virtual bool setLine(uint8_t line, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	/// Retrieves the current line state.
	///
	virtual uint8_t getLine(void) const;

	/// Retrieves the current port state.
	///
	virtual void getState(uint8_t* mode, uint8_t* line) const;

	/// Returns true if the port is in an error state.
	///
	virtual bool hasError(void) const override;

	/// This method is used to test values of a Digital port.
	virtual void testValue(const std::string& property, const std::string& expectedValue);
};

/// Defines an Analog port.
///
class AnalogPort : public Port {
	friend class OPDI;

private:
	uint8_t mode;
	uint8_t reference;
	uint8_t resolution;
	int32_t value;

	/// Validates and adjusts the supplied value if necessary.
	int32_t validateValue(int32_t value) const;

public:
	/// Constructs an Analog port with default settings. The ID is required.
	/// Default settings are: DirCaps = BiDi, Mode = Input, supported resolutions 8..12 bits, Reference = Internal.
	explicit AnalogPort(const char* id);

	/// Constructs an Analog port. Specify one of the OPDI_PORTDIRCAPS_* values for dircaps.
	/// Specify one or more of the OPDI_ANALOG_PORT_RESOLUTION_* values for flags, or'ed together, to specify supported resolutions.
	/// Default settings are: Mode = Input, Reference = Internal.
	AnalogPort(const char* id, const char*  dircaps, const int32_t flags);

	/// Sets the port mode (opdi_set_analog_port_mode).
	/// mode = 0: input
	/// mode = 1: output
	virtual void setMode(uint8_t mode, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	/// Returns the current port mode.
	///
	virtual uint8_t getMode(void);

	/// Sets the port resolution in bits.
	/// Supported values are 8..12.
	virtual void setResolution(uint8_t resolution, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	/// Sets the port's voltage reference source.
	/// reference = 0: internal voltage reference
	/// reference = 1: external voltage reference
	virtual void setReference(uint8_t reference, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	/// Retrieves the current port state.
	///
	virtual void getState(uint8_t* mode, uint8_t* resolution, uint8_t* reference, int32_t* value) const;

	/// Returns the current value as a factor between 0 and 1 of the maximum resolution.
	///
	virtual double getRelativeValue(void);

	/// Sets the value from a factor between 0 and 1.
	/// Uses setAbsoluteValue() internally.
	void setRelativeValue(double value, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	/// Sets the port's absolute value.
	/// value: an integer value ranging from 0 to 2^resolution - 1
	virtual void setAbsoluteValue(int32_t value, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	/// Returns true if the port is in an error state.
	///
	virtual bool hasError(void) const override;

	/// This method is used to test values of an Analog port.
	virtual void testValue(const std::string& property, const std::string& expectedValue);
};

/// Defines a select port.
///
class SelectPort : public Port {
	friend class OPDI;
public:
	typedef Poco::Tuple<int, std::string> Label;
	typedef std::vector<Label> LabelList;

private:
	LabelList orderedLabels;
	char** labels;
	uint16_t count;
	uint16_t position;

	/// Frees the internal items memory. Do not use.
	void freeItems();

protected:
	/// Sets the port's labels. The last element must be NULL.
	/// Copies the labels into the privately managed data structure of this class.
	virtual void setLabels(const char** labels);

	/// <summary>
	/// 
	/// </summary>
	virtual uint16_t getPositionByLabelOrderID(int orderID);

	/// <summary>
	/// 
	/// </summary>
	virtual Label getLabelAt(uint16_t position);

public:
	/// Constructs a Select port with the given ID.
	///
	explicit SelectPort(const char* id);

	/// Constructs a Select port. The direction of a select port is output only.
	/// You have to specify a list of labels for the different select positions. The last element must be NULL.
	/// The labels are copied into the privately managed data structure of this class.
	SelectPort(const char* id, const char** labels);

	/// Virtual destructor.
	///
	virtual ~SelectPort();

	/// Sets the port's labels.
	/// Copies the labels into the privately managed data structure of this class.
	virtual void setLabels(LabelList& orderedLabels);

	/// Handles position setting; position may be in the range of 0..(labels.length - 1).
	/// Returns whether the state has changed.
	virtual bool setPosition(uint16_t position, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	/// Returns the current port position.
	///
	virtual uint16_t getPosition(void) const;

	/// Returns the current port state, i. e. the selected label's position.
	///
	virtual void getState(uint16_t* position) const;

	/// Returns the label at the specified position.
	///
	virtual const char* getPositionLabel(uint16_t position);

	/// Returns the maximum position this select port can be set to.
	/// Positions are 0-based, i. e. the maximum position is the number of labels minus 1.
	virtual uint16_t getMaxPosition(void);

	/// Returns true if the port is in an error state.
	///
	virtual bool hasError(void) const override;

	/// This method is used to test values of a Select port.
	virtual void testValue(const std::string& property, const std::string& expectedValue);
};

/// Defines a Dial port.
///
class DialPort : public Port {
	friend class OPDI;

private:
	int64_t minValue;
	int64_t maxValue;
	uint64_t step;
	int64_t position;

public:
	/// Constructs a Select port with the given ID.
	/// Default values are: Minimum = 0, Maximum = 100, Step = 1.
	explicit DialPort(const char* id);

	/// Constructs a Select port with the given ID.
	/// The direction of a dial port is output only.
	/// You have to specify boundary values and a step size.
	DialPort(const char* id, int64_t minValue, int64_t maxValue, uint64_t step);

	/// Virtual destructor.
	///
	virtual ~DialPort();

	/// Sets the minimum value.
	///
	virtual void setMin(int64_t min);

	/// Returns the minimum value.
	///
	virtual int64_t getMin(void);

	/// Sets the maximum value.
	///
	virtual void setMax(int64_t max);

	/// Returns the maximum value.
	///
	virtual int64_t getMax(void);

	/// Sets the step.
	///
	virtual void setStep(uint64_t step);

	/// Returns the step.
	///
	virtual int64_t getStep(void);

	/// Sets the port's position; position may be in the range of minValue..maxValue.
	/// Returns whether the state has changed.
	virtual bool setPosition(int64_t position, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	/// Returns the current port position.
	///
	virtual int64_t getPosition(void) const;

	/// Returns the current port position.
	///
	virtual void getState(int64_t* position) const;

	/// Returns true if the port is in an error state.
	///
	virtual bool hasError(void) const override;

	/// This method is used to test values of a Dial port.
	virtual void testValue(const std::string& property, const std::string& expectedValue);
};

#ifdef OPDI_USE_CUSTOM_PORTS
/// Defines a Custom port.
///
class CustomPort : public Port {
	friend class OPDI;

protected:
	std::string value;

public:
	/// Constructs a Custom port with the given ID and typeGUID.
	explicit CustomPort(const std::string& id, const std::string& typeGUID);

	/// Virtual destructor.
	///
	virtual ~CustomPort();
        
        virtual void configure(Poco::Util::AbstractConfiguration::Ptr portConfig); 

	/// Sets the value.
	/// Returns whether the state has changed.
	virtual bool setValue(const std::string& value, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	/// Returns the value.
	///
	virtual std::string getValue(void) const;
        
        virtual bool hasError(void) const override;

	/// This method is used to test values of a Custom port.
	virtual void testValue(const std::string& property, const std::string& expectedValue);
};

#endif

/// Defines an abstract Streaming port.
/// This class can not be used directly. Use one of its subclasses instead.
///
/// A Streaming port represents a serial data connection on the device. It can send and receive bytes.
/// Examples are RS232 or I2C ports, but Streaming ports may also be entirely virtual or emulated.
/// If a master is connected it can bind to a streaming port. Binding means that received bytes are
/// transferred to the master and bytes received from the master are sent to the port.
/// The streaming port can thus act as an internal connection data source and sink, as well as a
/// transparent connection which directly connects the master and the connection partner.
/// Data sources can be read-only or write-only. Data may be also transformed before it is transmitted
/// to the master. How exactly this is done depends on the concrete implementation.
class StreamingPort : public Port {
	friend class OPDI;

public:
	/// Constructs a Streaming port with the given ID. A streaming port is always bidirectional.
	///
	explicit StreamingPort(const char* id);

	/// Virtual destructor.
	///
	virtual ~StreamingPort();

	/// Writes the specified bytes to the data sink. Returns the number of bytes written.
	/// If the returned value is less than 0 it is considered an (implementation specific) error.
	virtual int write(char* bytes, size_t length) = 0;

	/// Checks how many bytes are available from the data source. If count is > 0
	/// it is used as a value to request the number of bytes if the underlying system
	/// supports this type of request. Otherwise it has no meaning.
	/// If no bytes are available the result is 0. If the returned
	/// value is less than 0 it is considered an (implementation specific) error.
	virtual int available(size_t count) = 0;

	/// Reads one byte from the data source and places it in result. If the returned
	/// value is less than 1 it is considered an (implementation specific) error.
	virtual int read(char* result) = 0;
};

/// This class wraps either a fixed value or a port name. It is used when port configuration parameters
/// can be either fixed or dynamic values.
/// A port name can be followed by a double value in brackets. This indicates that the port's value
/// should be scaled, i. e. multiplied, with the given factor. This is especially useful when the value
/// of an analog port, which is by definition always between 0 and 1 inclusive, should be mapped to
/// a different range of numbers.
/// The port name can be optionally followed by a slash (/) plus a double value that specifies the value
/// in case the port returns an error. This allows for a reasonable reaction in case port errors are to
/// be expected. If this value is omitted the error will be propagated via an exception to the caller
/// which must then be react to the error condition. 
/// Syntax examples:
/// 10               - fixed value 10
/// MyPort1          - dynamic value of MyPort1
/// MyPort1(100)     - dynamic value of MyPort1, multiplied by 100
/// MyPort1/0        - dynamic value of MyPort1, 0 in case of an error
/// MyPort1(100)/0   - dynamic value of MyPort1, multiplied by 100, 0 in case of an error
template<typename T>
class ValueResolver {

	OPDI* opdi;
	Port* origin;
	std::string paramName;
	std::string portID;
	bool useScaleValue;
	double scaleValue;
	bool useErrorDefault;
	T errorDefault;
	mutable opdi::Port* port;
	bool isFixed;
	T fixedValue;

public:
	ValueResolver(OPDI* opdi);

	ValueResolver(OPDI* opdi, T initialValue);

	void initialize(Port* origin, const std::string& paramName, const std::string& value, bool allowErrorDefault = true);

	bool validate(T min, T max) const;

	T value() const;
};

}	// namespace opdi
