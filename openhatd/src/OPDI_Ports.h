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
#include "Poco/RegularExpression.h"
#include "Poco/NumberParser.h"

#include "opdi_platformtypes.h"
#include "opdi_configspecs.h"

namespace opdi {

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
		VALUE_OK,
		VALUE_EXPIRED,
		VALUE_NOT_AVAILABLE
	};

	// disable copy constructor
	Port(const Port& that) = delete;

protected:
	/// protected constructor - for use by friend classes only
	///
	Port(const char* id, const char* type);

	/// protected constructor: This class can't be instantiated directly
	///
	Port(const char* id, const char* type, const char* dircaps, int32_t flags, void* ptr);

	char* id;
	char* label;
	char type[2];	// type constants are one character (see opdi_port.h)
	char caps[2];	// caps constants are one character (port direction constants)
	int32_t flags;
	void* ptr;

	/// If a port is hidden it is not included in the device capabilities as queried by the master.
	///
	bool hidden;

	/// If a port is readonly its state cannot be changed by the master.
	///
	bool readonly;

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
	/// 
	virtual uint8_t doWork(uint8_t canSend);

	/// Performs actions necessary before shutting down. The default implementation calls persist() if 
	/// persistent is true and ignores any errors.
	virtual void shutdown(void);

	/// Override this method to implement specific persistence mechanisms. This default implementation 
	/// does nothing. Actual persistence handling is defined by subclasses.
	virtual void persist(void);

	/// Causes the port to be refreshed by sending a refresh message to a connected master.
	/// Only if the port is not hidden.
	virtual uint8_t refresh();

	/// Updates the extended info string of a port that will be sent to connected masters.
	///
	virtual void updateExtendedInfo(void);

	/// Utility function for escaping special characters in extended strings.
	///
	std::string escapeKeyValueText(const std::string& str) const;

	/// Checks the error state and throws an exception.
	/// Should be used by subclasses in getState() methods.
	virtual void checkError(void) const;

	/// Sets the ID of the port to the specified value.
	/// It is safe to use this function during the initialization phase only.
	/// Once the system is running the port IDs should not be changed any more.
	virtual void setID(const char* newID);

	/// This method must be called when the state changes in order to
	/// handle the onChange* functionality. The default port implementations
	/// automatically handle this.
	virtual void handleStateChange(ChangeSource changeSource);

	/// Finds the port with the specified portID. Delegates to the OPDI.findPort method.
	/// If portID.isEmpty() and required is true, throws an exceptions whose message refers to the configPort and setting. 
	/// This method is intended to be used during the preparation phase to resolve port specifications. 
	/// configPort will usually be the ID of the resolving port, and setting the 
	/// configuration parameter name that contained the portID specification.
	virtual Port* findPort(const std::string& configPort, const std::string& setting, const std::string& portID, bool required);

	/// Finds the ports with the specified portIDs. Delegates to the OPDI.findPorts method.
	/// This method is intended to be used during the preparation phase to resolve port specifications. 
	/// configPort will usually be the ID of the resolving port, and setting the 
	/// configuration parameter name that contained the portIDs specification.
	virtual void findPorts(const std::string& configPort, const std::string& setting, const std::string& portIDs, PortList& portList);

	virtual DigitalPort* findDigitalPort(const std::string& configPort, const std::string& setting, const std::string& portID, bool required);

	virtual void findDigitalPorts(const std::string& configPort, const std::string& setting, const std::string& portIDs, DigitalPortList& portList);

	virtual AnalogPort* findAnalogPort(const std::string& configPort, const std::string& setting, const std::string& portID, bool required);

	virtual void findAnalogPorts(const std::string& configPort, const std::string& setting, const std::string& portIDs, AnalogPortList& portList);

	virtual SelectPort* findSelectPort(const std::string& configPort, const std::string& setting, const std::string& portID, bool required);

	virtual void logWarning(const std::string& message);

	virtual void logNormal(const std::string& message);

	virtual void logVerbose(const std::string& message);

	virtual void logDebug(const std::string& message);

	virtual void logExtreme(const std::string& message);

	std::string getChangeSourceText(ChangeSource changeSource);
public:

	/** Virtual destructor for the port. */
	virtual ~Port();

	/** This exception can be used by implementations to indicate an error during a port operation.
	*  Its message will be transferred to the master. */
	class PortError : public Poco::Exception
	{
	public:
		explicit PortError(std::string message) : Poco::Exception(message) {};
	};

	/** This exception can be used by implementations to indicate that the value has expired. */
	class ValueExpired : public PortError
	{
	public:
		ValueExpired() : PortError("The value has expired") {};
	};

	/** This exception can be used by implementations to indicate that no value is available. */
	class ValueUnavailable : public PortError
	{
	public:
		ValueUnavailable() : PortError("The value is unavailable") {};
	};

	/** This exception can be used by implementations to indicate that a port operation is not allowed.
		*  Its message will be transferred to the master. */
	class AccessDenied : public Poco::Exception
	{
	public:
		explicit AccessDenied(std::string message) : Poco::Exception(message) {};
	};

	// used to provide display ordering on ports
	int orderID;

	// a list of space-separated keywords
	std::string tags;

	// Lists of digital ports that are to be set to High if a change occurs
	// The lists are to be set from external code but handled internally (this->handleStateChange).
	std::string onChangeIntPortsStr;
	std::string onChangeUserPortsStr;
	DigitalPortList onChangeIntPorts;
	DigitalPortList onChangeUserPorts;

	virtual const char* getID(void) const;

	std::string ID() const;

	virtual const char* getType(void) const;

	virtual void setHidden(bool hidden);

	virtual bool isHidden(void) const;

	virtual void setReadonly(bool readonly);

	virtual bool isReadonly(void) const;

	virtual void setPersistent(bool persistent);

	virtual bool isPersistent(void) const;

	/** Sets the label of the port. */
	virtual void setLabel(const char* label);

	virtual const char* getLabel(void) const;

	/** Sets the direction capabilities of the port. */
	virtual void setDirCaps(const char* dirCaps);

	virtual const char* getDirCaps(void) const;

	/** Sets the flags of the port. */
	virtual void setFlags(int32_t flags);

	virtual int32_t getFlags(void) const;

	virtual void setTypeGUID(const std::string& guid);

	virtual const std::string& getTypeGUID(void) const;

	virtual void setUnit(const std::string& unit);

	virtual const std::string& getUnit(void) const;

	virtual void setIcon(const std::string& icon);

	virtual const std::string& getIcon(void) const;

	virtual void setGroup(const std::string& group);

	virtual const std::string& getGroup(void) const;

	virtual void setHistory(uint64_t intervalSeconds, int maxCount, const std::vector<int64_t>& values);

	virtual const std::string& getHistory(void) const;

	virtual void clearHistory(void);

	virtual std::string getExtendedState(void) const;

	virtual std::string getExtendedInfo(void) const;

	virtual void setLogVerbosity(LogVerbosity newLogVerbosity);

	virtual LogVerbosity getLogVerbosity(void) const;

	virtual void setRefreshMode(RefreshMode refreshMode);

	virtual RefreshMode getRefreshMode(void) const;

	/** Sets the minimum time in milliseconds between self-refresh messages. If this time is 0 (default),
	* the self-refresh is disabled. */
	virtual void setPeriodicRefreshTime(uint32_t timeInMs);

	/** This method should be called just before the OPDI system is ready to start.
	* It gives the port the chance to do necessary initializations. */
	virtual void prepare(void);

	/** Sets the error state of this port. */
	virtual void setError(Error error);

	/** Gets the error state of this port. */
	virtual Port::Error getError(void) const;

	/** This method returns true if the port is in an error state. This will likely be the case
	*   when the getState() method of the port throws an exception.
	*/
	virtual bool hasError(void) const = 0;
};

inline std::ostream& operator<<(std::ostream& oStream, const Port::Error error) {
	switch (error) {
	case Port::Error::VALUE_OK: oStream << "VALUE_OK";
	case Port::Error::VALUE_EXPIRED: oStream << "VALUE_EXPIRED";
	case Port::Error::VALUE_NOT_AVAILABLE: oStream << "VALUE_NOT_AVAILABLE";
	default: oStream << "<unknown error>";
	}
	return oStream;
};


template <class T> inline std::string Port::to_string(const T& t) const {
	std::stringstream ss;
	ss << t;
	return ss.str();
}

class PortGroup {
	friend class OPDI;

protected:
	char* id;
	char* label;
	char* parent;
	int32_t flags;
	char* extendedInfo;

	// pointer to OPDI class instance
	OPDI* opdi;

	// OPDI implementation management structure
	void* data;

	// linked list of port groups - pointer to next port group
	PortGroup* next;

	std::string icon;

	virtual void updateExtendedInfo(void);

public:
	explicit PortGroup(const char* id);

	/** Virtual destructor for the port. */
	virtual ~PortGroup();

	virtual const char* getID(void);

	/** Sets the label of the port group. */
	virtual void setLabel(const char* label);

	virtual const char* getLabel(void);

	/** Sets the parent of the port group. */
	virtual void setParent(const char* parent);

	virtual const char* getParent(void);

	/** Sets the flags of the port group. */
	virtual void setFlags(int32_t flags);

	virtual void setIcon(const std::string& icon);
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Port definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Defines a digital port.
	*/
class DigitalPort : public Port {
	friend class OPDI;

protected:
	uint8_t mode;
	uint8_t line;

public:
	explicit DigitalPort(const char* id);

	// Initialize a digital port. Specify one of the OPDI_PORTDIRCAPS_* values for dircaps.
	// Specify one or more of the OPDI_DIGITAL_PORT_* values for flags, or'ed together, to specify pullup/pulldown resistors.
	DigitalPort(const char* id, const char*  dircaps, const int32_t flags);

	virtual ~DigitalPort();

	virtual void setDirCaps(const char* dirCaps);

	// function that handles the set mode command (opdi_set_digital_port_mode)
	// mode = 0: floating input
	// mode = 1: input with pullup on
	// mode = 2: not supported
	// mode = 3: output
	virtual void setMode(uint8_t mode, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	// function that handles the set line command (opdi_set_digital_port_line)
	// line = 0: state low
	// line = 1: state high
	virtual void setLine(uint8_t line, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	// function that fills in the current port state
	virtual void getState(uint8_t* mode, uint8_t* line) const;

	virtual uint8_t getMode(void);

	virtual bool hasError(void) const override;
};

/** Defines an analog port.
	*/
class AnalogPort : public Port {
	friend class OPDI;

protected:
	uint8_t mode;
	uint8_t reference;
	uint8_t resolution;
	int32_t value;

	virtual int32_t validateValue(int32_t value) const;

public:
	explicit AnalogPort(const char* id);

	AnalogPort(const char* id, const char*  dircaps, const int32_t flags);

	virtual ~AnalogPort();

	// mode = 0: input
	// mode = 1: output
	virtual void setMode(uint8_t mode, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	virtual void setResolution(uint8_t resolution, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	// reference = 0: internal voltage reference
	// reference = 1: external voltage reference
	virtual void setReference(uint8_t reference, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	// value: an integer value ranging from 0 to 2^resolution - 1
	virtual void setValue(int32_t value, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	virtual void getState(uint8_t* mode, uint8_t* resolution, uint8_t* reference, int32_t* value) const;

	// returns the value as a factor between 0 and 1 of the maximum resolution
	virtual double getRelativeValue(void);

	// sets the value from a factor between 0 and 1
	virtual void setRelativeValue(double value);

	virtual uint8_t getMode(void);

	virtual bool hasError(void) const override;
};

/** Defines a select port.
	*
	*/
class SelectPort : public Port {
	friend class OPDI;

protected:
	char** items;
	uint16_t count;
	uint16_t position;

	// frees the internal items memory
	void freeItems();

public:
	explicit SelectPort(const char* id);

	// Initialize a select port. The direction of a select port is output only.
	// You have to specify a list of items that are the labels of the different select positions. The last element must be NULL.
	// The items are copied into the privately managed data structure of this class.
	SelectPort(const char* id, const char** items);

	virtual ~SelectPort();

	// Copies the items into the privately managed data structure of this class.
	virtual void setItems(const char* *items);

	// function that handles position setting; position may be in the range of 0..(items.length - 1)
	virtual void setPosition(uint16_t position, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	// function that fills in the current port state
	virtual void getState(uint16_t* position) const;

	virtual const char* getPositionLabel(uint16_t position);

	virtual uint16_t getMaxPosition(void);

	virtual bool hasError(void) const override;
};

/** Defines a dial port.
	*
	*/
class DialPort : public Port {
	friend class OPDI;

protected:
	int64_t minValue;
	int64_t maxValue;
	uint64_t step;
	int64_t position;

public:
	explicit DialPort(const char* id);

	// Initialize a dial port. The direction of a dial port is output only.
	// You have to specify boundary values and a step size.
	DialPort(const char* id, int64_t minValue, int64_t maxValue, uint64_t step);
	virtual ~DialPort();

	virtual int64_t getMin(void);
	virtual int64_t getMax(void);
	virtual int64_t getStep(void);

	virtual void setMin(int64_t min);
	virtual void setMax(int64_t max);
	virtual void setStep(uint64_t step);

	// function that handles position setting; position may be in the range of minValue..maxValue
	virtual void setPosition(int64_t position, ChangeSource changeSource = Port::ChangeSource::CHANGESOURCE_INT);

	// function that fills in the current port state
	virtual void getState(int64_t* position) const;

	virtual bool hasError(void) const override;
};

/** Defines a streaming port.
	* A streaming port represents a serial data connection on the device. It can send and receive bytes.
	* Examples are RS232 or I2C ports.
	* If a master is connected it can bind to a streaming port. Binding means that received bytes are
	* transferred to the master and bytes received from the master are sent to the connection.
	* The streaming port can thus act as an internal connection data source and sink, as well as a
	* transparent connection which directly connects the master and the connection partner.
	* Data sources can be read-only or write-only. Data may be also transformed before it is transmitted
	* to the master. How exactly this is done depends on the concrete implementation.
	*/
class StreamingPort : public Port {
	friend class OPDI;

public:
	// Initialize a streaming port. A streaming port is always bidirectional.
	explicit StreamingPort(const char* id);

	virtual ~StreamingPort();

	// Writes the specified bytes to the data sink. Returns the number of bytes written.
	// If the returned value is less than 0 it is considered an (implementation specific) error.
	virtual int write(char* bytes, size_t length) = 0;

	// Checks how many bytes are available from the data source. If count is > 0
	// it is used as a value to request the number of bytes if the underlying system
	// supports this type of request. Otherwise it has no meaning.
	// If no bytes are available the result is 0. If the returned
	// value is less than 0 it is considered an (implementation specific) error.
	virtual int available(size_t count) = 0;

	// Reads one byte from the data source and places it in result. If the returned
	// value is less than 1 it is considered an (implementation specific) error.
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
	ValueResolver(OPDI* opdi) {
		this->opdi = opdi;
		this->fixedValue = 0;
		this->isFixed = false;
		this->useScaleValue = false;
		this->scaleValue = 0;
		this->useErrorDefault = false;
		this->errorDefault = 0;
	}

	ValueResolver(OPDI* opdi, T initialValue) : ValueResolver(opdi) {
		this->isFixed = true;
		this->fixedValue = initialValue;
	}

	void initialize(Port* origin, const std::string& paramName, const std::string& value, bool allowErrorDefault = true) {
		this->origin = origin;
		this->useScaleValue = false;
		this->useErrorDefault = false;
		this->port = nullptr;

		this->opdi->logDebug(origin->ID() + ": Parsing ValueResolver expression of parameter '" + paramName + "': " + value);
		// try to convert the value to a double
		double d;
		if (Poco::NumberParser::tryParseFloat(value, d)) {
			this->fixedValue = (T)d;
			this->isFixed = true;
			this->opdi->logDebug(origin->ID() + ": ValueResolver expression resolved to fixed value: " + this->opdi->to_string(value));
		}
		else {
			this->isFixed = false;
			Poco::RegularExpression::MatchVec matches;
			std::string portName;
			std::string scaleStr;
			std::string defaultStr;
			// try to match full syntax
			Poco::RegularExpression reFull("(.*)\\((.*)\\)\\/(.*)");
			if (reFull.match(value, 0, matches) == 4) {
				if (!allowErrorDefault)
					throw Poco::ApplicationException(origin->ID() + ": Parameter " + paramName + ": Specifying an error default value is not allowed: " + value);
				portName = value.substr(matches[1].offset, matches[1].length);
				scaleStr = value.substr(matches[2].offset, matches[2].length);
				defaultStr = value.substr(matches[3].offset, matches[3].length);
			}
			else {
				// try to match default value syntax
				Poco::RegularExpression reDefault("(.*)\\/(.*)");
				if (reDefault.match(value, 0, matches) == 3) {
					if (!allowErrorDefault)
						throw Poco::ApplicationException(origin->ID() + ": Parameter " + paramName + ": Specifying an error default value is not allowed: " + value);
					portName = value.substr(matches[1].offset, matches[1].length);
					defaultStr = value.substr(matches[2].offset, matches[2].length);
				}
				else {
					// try to match scale value syntax
					Poco::RegularExpression reScale("(.*)\\((.*)\\)");
					if (reScale.match(value, 0, matches) == 3) {
						portName = value.substr(matches[1].offset, matches[1].length);
						scaleStr = value.substr(matches[2].offset, matches[2].length);
					}
					else {
						// could not match a pattern - use value as port name
						portName = value;
					}
				}
			}
			// parse values if specified
			if (scaleStr != "") {
				if (Poco::NumberParser::tryParseFloat(scaleStr, this->scaleValue)) {
					this->useScaleValue = true;
				}
				else
					throw Poco::ApplicationException(origin->ID() + ": Parameter " + paramName + ": Invalid scale value specified; must be numeric: " + scaleStr);
			}
			if (defaultStr != "") {
				double e;
				if (Poco::NumberParser::tryParseFloat(defaultStr, e)) {
					this->errorDefault = (T)e;
					this->useErrorDefault = true;
				}
				else
					throw Poco::ApplicationException(origin->ID() + ": Parameter " + paramName + ": Invalid error default value specified; must be numeric: " + defaultStr);
			}

			this->portID = portName;

			this->opdi->logDebug(origin->ID() + ": ValueResolver expression resolved to port ID: " + this->portID
				+ (this->useScaleValue ? ", scale by " + this->opdi->to_string(this->scaleValue) : "")
				+ (this->useErrorDefault ? ", error default is " + this->opdi->to_string(this->errorDefault) : ""));
		}
	}

	bool validate(T min, T max) const {
		// no fixed value? assume it's valid
		if (!this->isFixed)
			return true;

		return ((this->fixedValue >= min) && (this->fixedValue <= max));
	}

	T value() const {
		if (isFixed)
			return fixedValue;
		else {
			// port not yet resolved?
			if (this->port == nullptr) {
				if (this->portID == "")
					throw Poco::ApplicationException(this->origin->ID() + ": Parameter " + paramName + ": ValueResolver not initialized (programming error)");
				// try to resolve the port
				this->port = this->opdi->findPort(this->origin->ID(), this->paramName, this->portID, true);
			}
			// resolve port value to a double
			double result = 0;

			try {
				result = this->opdi->getPortValue(this->port);
			}
			catch (Poco::Exception &pe) {
				this->opdi->logExtreme(this->origin->ID() + ": Unable to get the value of the port " + port->ID() + ": " + pe.message());
				if (this->useErrorDefault) {
					return this->errorDefault;
				}
				else
					// propagate exception
					throw Poco::ApplicationException(this->origin->ID() + ": Unable to get the value of the port " + port->ID() + ": " + pe.message());
			}

			// scale?
			if (this->useScaleValue) {
				result *= this->scaleValue;
			}

			return (T)result;
		}
	}
};

}	// namespace opdi
