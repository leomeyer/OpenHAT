#include "TimerPort.h"

#include "Poco/Tuple.h"
#include "Poco/Timezone.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/NumberParser.h"
#include "Poco/Format.h"

#include "SunRiseSet.h"

namespace openhat {

///////////////////////////////////////////////////////////////////////////////
// Timer Port
///////////////////////////////////////////////////////////////////////////////

int TimerPort::ScheduleComponent::ParseValue(Type type, std::string value, AbstractOpenHAT* openhat) {
	std::string compName;
	switch (type) {
	case MONTH: compName = "Month"; break;
	case DAY: compName = "Day"; break;
	case HOUR: compName = "Hour"; break;
	case MINUTE: compName = "Minute"; break;
	case SECOND: compName = "Second"; break;
	case WEEKDAY: compName = "Weekday"; break;
	}
	// parse as integer
	int number = Poco::NumberParser::parse(value);
	bool valid = true;
	switch (type) {
	case MONTH: valid = (number >= 1) && (number <= 12); break;
	case DAY: valid = (number >= 1) && (number <= 31); break;
	case HOUR: valid = (number >= 0) && (number <= 23); break;
	case MINUTE: valid = (number >= 0) && (number <= 59); break;
	case SECOND: valid = (number >= 0) && (number <= 59); break;
	case WEEKDAY: valid = (number >= 0) && (number <= 6); break;
	}
	if (!valid)
		openhat->throwSettingException("The specification '" + value + "' is not valid for the date/time component " + compName);
	return number;
}

TimerPort::ScheduleComponent TimerPort::ScheduleComponent::Parse(Type type, std::string def, AbstractOpenHAT* openhat) {
	ScheduleComponent result;
	result.type = type;

	std::string compName;
	switch (type) {
	case MONTH: result.values.resize(13); compName = "Month"; break;
	case DAY: result.values.resize(32); compName = "Day"; break;
	case HOUR: result.values.resize(24); compName = "Hour"; break;
	case MINUTE: result.values.resize(60); compName = "Minute"; break;
	case SECOND: result.values.resize(60); compName = "Second"; break;
	case WEEKDAY: result.values.resize(7); compName = "Weekday"; break;
	}

	// split definition at blanks
	std::stringstream ss(def);
	std::string item;
	while (std::getline(ss, item, ' ')) {
		bool val = true;
		size_t dashPos;
		// inversion?
		if (item[0] == '!') {
			val = false;
			item = item.substr(1);
		}
		if (item == "*") {
			// set all values
			for (size_t i = 0; i < result.values.size(); i++)
				result.values[i] = val;
		} else
		// range specified?
		if ((dashPos = item.find('-')) != std::string::npos) {
			int range1;
			int range2;
			// dash at first position?
			if (dashPos == 0)
				range1 = result.getMinimum();
			else
				range1 = ParseValue(type, item.substr(0, dashPos), openhat);
			// dash at last position?
			if (dashPos == item.size() - 1)
				range2 = result.getMaximum();
			else
				range2 = ParseValue(type, item.substr(dashPos + 1), openhat);
			if (range1 > range2)
				openhat->throwSettingException("The range specification '" + item + "' is not valid for the date/time component " + compName);
			// set values of the range
			for (int i = range1; i <= range2; i++)
				result.values[i] = val;
		} else {
			// parse as integer
			result.values[ParseValue(type, item, openhat)] = val;
		}
	}

	// check that at least one value is set
	for (size_t i = 0; i < result.values.size(); i++)
		if (result.values[i])
			return result;

	openhat->throwSettingException("Timer port schedule component " + compName + " requires at least one allowed value");
	return result;
}

bool TimerPort::ScheduleComponent::getNextPossibleValue(int* currentValue, bool* rollover, bool* changed, int month, int year) {
	*rollover = false;
	*changed = false;
	// find a match
	for (int i = *currentValue; i < (int)this->values.size(); i++) {
		if (this->values[i]) {
			// for days, make a special check whether the month has as many days
			if ((this->type != DAY) || (i < Poco::DateTime::daysOfMonth(year, month))) {
				*changed = *currentValue != i;
				*currentValue = i;
				return true;
			}
			// day is out of range; need to rollover
			break;
		}
	}
	// not found? rollover; return first possible value
	*rollover = true;
	return this->getFirstPossibleValue(currentValue, month, year);
}

bool TimerPort::ScheduleComponent::getFirstPossibleValue(int* currentValue, int /*month*/, int /*year*/) {
	// find a match
	int i = 0;
	switch (type) {
	case MONTH: i = 1; break;
	case DAY: i = 1; break;
	case HOUR: break;
	case MINUTE: break;
	case SECOND: break;
	case WEEKDAY:
		throw Poco::ApplicationException("Weekday is not supported");
	}
	for (; i < (int)this->values.size(); i++) {
		if (this->values[i]) {
			*currentValue = i;
			return true;
		}
	}
	// nothing found
	return false;
}

bool TimerPort::ScheduleComponent::hasValue(int value) {
	return this->values[value];
}

int TimerPort::ScheduleComponent::getMinimum(void) {
	switch (this->type) {
	case MONTH: return 1;
	case DAY: return 1;
	case HOUR: return 0;
	case MINUTE: return 0;
	case SECOND: return 0;
	case WEEKDAY: return 0;
	}
	return -1;
}

int TimerPort::ScheduleComponent::getMaximum(void) {
	switch (this->type) {
	case MONTH: return 12;
	case DAY: return 31;
	case HOUR: return 23;
	case MINUTE: return 59;
	case SECOND: return 59;
	case WEEKDAY: return 6;
	}
	return -1;
}


void TimerPort::ManualSchedulePort::setPosition(int64_t position, ChangeSource changeSource) {
	DialPort::setPosition(position, changeSource);

	// notify timer port: schedule has changed
	this->timerPort->recalculateSchedules();
}


TimerPort::TimerPort(AbstractOpenHAT* openhat, const char* id) : DigitalPort(id, OPDI_PORTDIRCAP_OUTPUT, 0) {
	this->opdi = this->openhat = openhat;

	DigitalPort::setMode(OPDI_DIGITAL_MODE_OUTPUT);

	// default: enabled
	this->line = 1;
	this->masterLoggedIn = false;

	// set default icon
	this->icon = "alarmclock";

	// set default texts
	this->deactivatedText = "Deactivated";
	this->notScheduledText = "Not scheduled";
	this->nextEventText = "Next event: ";
	this->timestampFormat = openhat->timestampFormat;

	this->lastWorkTimestamp = 0;
}

TimerPort::~TimerPort() {
}

void TimerPort::configure(ConfigurationView::Ptr config, ConfigurationView::Ptr parentConfig) {
	this->openhat->configureDigitalPort(config, this);
	this->logVerbosity = this->openhat->getConfigLogVerbosity(config, opdi::LogVerbosity::UNKNOWN);

	this->outputPortStr = this->openhat->getConfigString(config, this->ID(), "OutputPorts", "", true);
	this->propagateSwitchOff = config->getBool("PropagateSwitchOff", false);
	this->deactivatedText = config->getString("DeactivatedText", this->deactivatedText);
	this->notScheduledText = config->getString("NotScheduledText", this->notScheduledText);
	this->nextEventText = config->getString("NextEventText", this->nextEventText);
	this->timestampFormat = config->getString("TimestampFormat", this->timestampFormat);

	// enumerate schedules of the <timer>.Schedules node
	this->logVerbose(std::string("Enumerating Timer schedules: ") + this->ID() + ".Schedules");

	Poco::AutoPtr<ConfigurationView> nodes = this->openhat->createConfigView(config, "Schedules");
	config->addUsedKey("Schedules");

	// get ordered list of schedules
	ConfigurationView::Keys scheduleKeys;
	nodes->keys("", scheduleKeys);

	typedef Poco::Tuple<int, std::string> Item;
	typedef std::vector<Item> ItemList;
	ItemList orderedItems;

	// create ordered list of schedule keys (by priority)
	for (auto it = scheduleKeys.begin(), ite = scheduleKeys.end(); it != ite; ++it) {

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
		this->logNormal(std::string("Warning: No schedules configured in node ") + this->ID() + ".Schedules; is this intended?");
	}

	// go through items, create schedules in specified order
	auto nli = orderedItems.begin();
	auto nlie = orderedItems.end();
	while (nli != nlie) {

		std::string nodeName = nli->get<1>();

		this->logVerbose("Setting up timer schedule for node: " + nodeName);

		// get schedule section from the configuration
		Poco::AutoPtr<ConfigurationView> scheduleConfig = this->openhat->createConfigView(parentConfig, nodeName);

		Schedule schedule = Schedule();
		schedule.nodeName = nodeName;
		schedule.maxOccurrences = scheduleConfig->getInt("MaxOccurrences", -1);
		schedule.duration = scheduleConfig->getInt("Duration", 1000);	// default duration: 1 second
		schedule.action = SET_HIGH;										// default action
		schedule.astroEvent = NONE;

		std::string action = this->openhat->getConfigString(scheduleConfig, nodeName, "Action", "", false);
		if (action == "SetHigh") {
			schedule.action = SET_HIGH;
		} else
		if (action == "SetLow") {
			schedule.action = SET_LOW;
		} else
		if (action == "Toggle") {
			schedule.action = TOGGLE;
		} else
			if (!action.empty())
				this->openhat->throwSettingException(nodeName + ": Unknown schedule action; expected: 'SetHigh', 'SetLow' or 'Toggle': " + action);

		// get schedule type (required)
		std::string scheduleType = this->openhat->getConfigString(scheduleConfig, nodeName, "Type", "", true);

		if (scheduleType == "Once") {
			schedule.type = ONCE;
			schedule.data.time.year = scheduleConfig->getInt("Year", -1);
			schedule.data.time.month = scheduleConfig->getInt("Month", -1);
			schedule.data.time.day = scheduleConfig->getInt("Day", -1);
			schedule.data.time.weekday = scheduleConfig->getInt("Weekday", -1);
			schedule.data.time.hour = scheduleConfig->getInt("Hour", -1);
			schedule.data.time.minute = scheduleConfig->getInt("Minute", -1);
			schedule.data.time.second = scheduleConfig->getInt("Second", -1);

			if (schedule.data.time.weekday > -1)
				this->openhat->throwSettingException(nodeName + ": You cannot use the Weekday setting with schedule type Once");
			if ((schedule.data.time.year < 0) || (schedule.data.time.month < 0)
				|| (schedule.data.time.day < 0)
				|| (schedule.data.time.hour < 0) || (schedule.data.time.minute < 0)
				|| (schedule.data.time.second < 0))
				this->openhat->throwSettingException(nodeName + ": You have to specify all time components (except Weekday) for schedule type Once");
		} else
		if (scheduleType == "Interval") {
			schedule.type = INTERVAL;
			schedule.data.time.year = scheduleConfig->getInt("Year", -1);
			schedule.data.time.month = scheduleConfig->getInt("Month", -1);
			schedule.data.time.day = scheduleConfig->getInt("Day", -1);
			schedule.data.time.weekday = scheduleConfig->getInt("Weekday", -1);
			schedule.data.time.hour = scheduleConfig->getInt("Hour", -1);
			schedule.data.time.minute = scheduleConfig->getInt("Minute", -1);
			schedule.data.time.second = scheduleConfig->getInt("Second", -1);

			if (schedule.data.time.year > -1)
				this->openhat->throwSettingException(nodeName + ": You cannot use the Year setting with schedule type Interval");
			if (schedule.data.time.month > -1)
				this->openhat->throwSettingException(nodeName + ": You cannot use the Month setting with schedule type Interval");
			if (schedule.data.time.weekday > -1)
				this->openhat->throwSettingException(nodeName + ": You cannot use the Weekday setting with schedule type Interval");

			if ((schedule.data.time.day < 0)
				&& (schedule.data.time.hour < 0) && (schedule.data.time.minute < 0)
				&& (schedule.data.time.second < 0))
				this->openhat->throwSettingException(nodeName + ": You have to specify at least one of Day, Hour, Minute or Second for schedule type Interval");
		} else
		if (scheduleType == "Periodic") {
			schedule.type = PERIODIC;
			if (scheduleConfig->getString("Year", "") != "")
				this->openhat->throwSettingException(nodeName + ": You cannot use the Year setting with schedule type Periodic");
			schedule.monthComponent = ScheduleComponent::Parse(ScheduleComponent::MONTH, scheduleConfig->getString("Month", "*"), this->openhat);
			schedule.dayComponent = ScheduleComponent::Parse(ScheduleComponent::DAY, scheduleConfig->getString("Day", "*"), this->openhat);
			schedule.hourComponent = ScheduleComponent::Parse(ScheduleComponent::HOUR, scheduleConfig->getString("Hour", "*"), this->openhat);
			schedule.minuteComponent = ScheduleComponent::Parse(ScheduleComponent::MINUTE, scheduleConfig->getString("Minute", "*"), this->openhat);
			schedule.secondComponent = ScheduleComponent::Parse(ScheduleComponent::SECOND, scheduleConfig->getString("Second", "*"), this->openhat);
			schedule.weekdayComponent = ScheduleComponent::Parse(ScheduleComponent::WEEKDAY, scheduleConfig->getString("Weekday", "*"), this->openhat);
		} else
		if (scheduleType == "Astronomical") {
			schedule.type = ASTRONOMICAL;
			std::string astroEventStr = this->openhat->getConfigString(scheduleConfig, nodeName, "AstroEvent", "", true);
			if (astroEventStr == "Sunrise") {
				schedule.astroEvent = SUNRISE;
			} else
			if (astroEventStr == "Sunset") {
				schedule.astroEvent = SUNSET;
			} else
				this->openhat->throwSettingException(nodeName + ": Parameter AstroEvent must be specified; use either 'Sunrise' or 'Sunset'");
			schedule.astroOffset = scheduleConfig->getInt("AstroOffset", 0);
			schedule.astroLon = scheduleConfig->getDouble("Longitude", -999);
			schedule.astroLat = scheduleConfig->getDouble("Latitude", -999);
			if ((schedule.astroLon < -180) || (schedule.astroLon > 180))
				this->openhat->throwSettingException(nodeName + ": Parameter Longitude must be specified and within -180..180");
			if ((schedule.astroLat < -90) || (schedule.astroLat > 90))
				this->openhat->throwSettingException(nodeName + ": Parameter Latitude must be specified and within -90..90");
			if ((schedule.astroLat < -65) || (schedule.astroLat > 65))
				this->openhat->throwSettingException(nodeName + ": Sorry. Latitudes outside -65..65 are currently not supported (library crash). You can either relocate or try and fix the bug.");
		} else
		if (scheduleType == "OnLogin") {
			schedule.type = ONLOGIN;
		} else
		if (scheduleType == "OnLogout") {
			schedule.type = ONLOGOUT;
		} else
		if (scheduleType == "Manual") {
			schedule.type = MANUAL;
			// a manual schedule is represented by a DialPort with unit=unixTime
			// the user can set the scheduled instant using this port
			// This port is a dependent node, i. e. it is not included by root but created by this port.

			std::string nodeID = this->openhat->getConfigString(scheduleConfig, nodeName, "NodeID", "", true);

			Poco::AutoPtr<ConfigurationView> manualPortNode = this->openhat->createConfigView(parentConfig, nodeID);

			// port must be of type "DialPort"
			if (this->openhat->getConfigString(manualPortNode, nodeName, "Type", "", true) != "DialPort")
				this->openhat->throwSettingException(nodeName + "The dependent port node for a manual timer schedule must be of type 'DialPort' (referenced by NodeID '" + nodeID + "')");

			// create the manual schedule dial port
			schedule.data.manualPort = new ManualSchedulePort(nodeID.c_str(), this);

			// the new port inherits the group from this port (can be overridden)
			schedule.data.manualPort->setGroup(this->group);

			this->openhat->configureDialPort(manualPortNode, schedule.data.manualPort);

			// add the port
			this->openhat->addPort(schedule.data.manualPort);
		} else
			this->openhat->throwSettingException(nodeName + ": Schedule type is not supported: " + scheduleType);

		this->schedules.push_back(schedule);

		++nli;
	}
}

void TimerPort::prepare() {
	this->logDebug("Preparing port");
	DigitalPort::prepare();

	// find ports; throws errors if something required is missing
	this->findDigitalPorts(this->ID(), "OutputPorts", this->outputPortStr, this->outputPorts);

	if (this->line == 1) {
		// calculate all schedules
		this->recalculateSchedules();
	}

	/*
			// test cases
			if (schedule.type == ASTRONOMICAL) {
				CSunRiseSet sunRiseSet;
				Poco::DateTime now;
				Poco::DateTime today(now.julianDay());
				for (size_t i = 0; i < 365; i++) {
					Poco::DateTime result = sunRiseSet.GetSunrise(schedule.astroLat, schedule.astroLon, Poco::DateTime(now.julianDay() + i));
					this->openhat->println("Sunrise: " + Poco::DateTimeFormatter::format(result, this->openhat->timestampFormat));
					result = sunRiseSet.GetSunset(schedule.astroLat, schedule.astroLon, Poco::DateTime(now.julianDay() + i));
					this->openhat->println("Sunset: " + Poco::DateTimeFormatter::format(result, this->openhat->timestampFormat));
				}
			}
	*/

	// remember calculation timestamp
	this->lastWorkTimestamp = Poco::Timestamp();
}

bool TimerPort::matchWeekday(int day, int month, int year, ScheduleComponent* weekdayScheduleComponent) {
	// determine day of week
	Poco::DateTime dt(year, month, day);
	int dayOfWeek = dt.dayOfWeek();	// 0 = Sunday
	//  weekdayScheduleComponent must match
	return weekdayScheduleComponent->hasValue(dayOfWeek);
}

Poco::Timestamp TimerPort::calculateNextOccurrence(Schedule* schedule) {
	if (schedule->type == ONCE) {
		// validate
		if ((schedule->data.time.month < 1) || (schedule->data.time.month > 12))
			this->openhat->throwSettingException(this->ID() + ": Invalid Month specification in schedule " + schedule->nodeName);
		if ((schedule->data.time.day < 1) || (schedule->data.time.day > Poco::DateTime::daysOfMonth(schedule->data.time.year, schedule->data.time.month)))
			this->openhat->throwSettingException(this->ID() + ": Invalid Day specification in schedule " + schedule->nodeName);
		if (schedule->data.time.hour > 23)
			this->openhat->throwSettingException(this->ID() + ": Invalid Hour specification in schedule " + schedule->nodeName);
		if (schedule->data.time.minute > 59)
			this->openhat->throwSettingException(this->ID() + ": Invalid Minute specification in schedule " + schedule->nodeName);
		if (schedule->data.time.second > 59)
			this->openhat->throwSettingException(this->ID() + ": Invalid Second specification in schedule " + schedule->nodeName);
		// create timestamp via datetime
		Poco::DateTime result = Poco::DateTime(schedule->data.time.year, schedule->data.time.month, schedule->data.time.day, 
			schedule->data.time.hour, schedule->data.time.minute, schedule->data.time.second);
		// ONCE values are specified in local time; convert to UTC
		result.makeUTC(Poco::Timezone::tzd());
		return result.timestamp();
	} else
	if (schedule->type == INTERVAL) {
		Poco::Timestamp result;	// now
		// add interval values (ignore year and month because those are not well defined in seconds)
		if (schedule->data.time.second > 0)
			result += schedule->data.time.second * result.resolution();
		if (schedule->data.time.minute > 0)
			result += schedule->data.time.minute * 60 * result.resolution();
		if (schedule->data.time.hour > 0)
			result += schedule->data.time.hour * 60 * 60 * result.resolution();
		if (schedule->data.time.day > 0)
			result += schedule->data.time.day * 24 * 60 * 60 * result.resolution();
		return result;
	} else
	if (schedule->type == PERIODIC) {

#define correctValues	if (second > 59) { second = 0; minute++; }                                \
						if (minute > 59) { minute = 0; hour++; }                                  \
						if (hour > 23) { hour = 0; day++; }                                       \
						if (day > Poco::DateTime::daysOfMonth(year, month)) { day = 1; month++; } \
						if (month > 12) { month = 1; year++; }

		Poco::LocalDateTime now;
		// start from the next second
		int second = now.second() + 1;
		int minute = now.minute();
		int hour = now.hour();
		int day = now.day();
		int month = now.month();
		int year = now.year();
		correctValues;

		bool rollover = false;
		bool changed = false;
		// calculate next possible seconds
		if (!schedule->secondComponent.getNextPossibleValue(&second, &rollover, &changed, month, year))
			return Poco::Timestamp();
		// rolled over into next minute?
		if (rollover) {
			minute++;
			correctValues;
		}
		// get next possible minute
		if (!schedule->minuteComponent.getNextPossibleValue(&minute, &rollover, &changed, month, year))
			return Poco::Timestamp();
		// rolled over into next hour?
		if (rollover) {
			hour++;
			correctValues;
		}
		// if the minute was changed the second value is now invalid; set it to the first
		// possible value (because the sub-component range starts again with every new value)
		if (rollover || changed) {
			schedule->secondComponent.getFirstPossibleValue(&second, month, year);
		}
		// get next possible hour
		if (!schedule->hourComponent.getNextPossibleValue(&hour, &rollover, &changed, month, year))
			return Poco::Timestamp();
		// rolled over into next day?
		if (rollover) {
			day++;
			correctValues;
		}
		if (rollover || changed) {
			schedule->secondComponent.getFirstPossibleValue(&second, month, year);
			schedule->minuteComponent.getFirstPossibleValue(&minute, month, year);
		}
		// determine possible dates until the weekday matches
		// avoid an infinite loop by terminating after too many iterations
		int maxIter = 100;
		do {
			// terminate after too many iterations
			if (--maxIter < 0)
				return Poco::Timestamp();
			// get next possible day
			if (!schedule->dayComponent.getNextPossibleValue(&day, &rollover, &changed, month, year))
				return Poco::Timestamp();
			// rolled over into next month?
			if (rollover) {
				month++;
				correctValues;
			}
			if (rollover || changed) {
				schedule->secondComponent.getFirstPossibleValue(&second, month, year);
				schedule->minuteComponent.getFirstPossibleValue(&minute, month, year);
				schedule->hourComponent.getFirstPossibleValue(&hour, month, year);
			}
			// get next possible month
			if (!schedule->monthComponent.getNextPossibleValue(&month, &rollover, &changed, month, year))
				return Poco::Timestamp();
			// rolled over into next year?
			if (rollover) {
				year++;
				correctValues;
			}
			if (rollover || changed) {
				schedule->secondComponent.getFirstPossibleValue(&second, month, year);
				schedule->minuteComponent.getFirstPossibleValue(&minute, month, year);
				schedule->hourComponent.getFirstPossibleValue(&hour, month, year);
				schedule->dayComponent.getFirstPossibleValue(&day, month, year);
			}
			// does the weekday match?
			if (this->matchWeekday(day, month, year, &schedule->weekdayComponent))
				break;
			// no - try next day
			day++;
			correctValues;
			schedule->secondComponent.getFirstPossibleValue(&second, month, year);
			schedule->minuteComponent.getFirstPossibleValue(&minute, month, year);
			schedule->hourComponent.getFirstPossibleValue(&hour, month, year);
		} while (true);

		Poco::DateTime result = Poco::DateTime(year, month, day, hour, minute, second);
		// values are specified in local time; convert to UTC
		result.makeUTC(Poco::Timezone::tzd());
		return result.timestamp();
	} else
	if (schedule->type == ASTRONOMICAL) {
		CSunRiseSet sunRiseSet;
		Poco::DateTime now;
		now.makeLocal(Poco::Timezone::tzd());
		Poco::DateTime today(now.julianDay());
		switch (schedule->astroEvent) {
		case NONE: break;
		case SUNRISE: {
			// find today's sunrise
			Poco::DateTime result = sunRiseSet.GetSunrise(schedule->astroLat, schedule->astroLon, today);
			// sun already risen?
			if (result < now)
				// find tomorrow's sunrise
				result = sunRiseSet.GetSunrise(schedule->astroLat, schedule->astroLon, Poco::DateTime(now.julianDay() + 1));
			// values are specified in local time; convert to UTC
			result.makeUTC(Poco::Timezone::tzd());
			return result.timestamp() + schedule->astroOffset * Poco::Timestamp::resolution();		// add offset in microseconds
		}
		case SUNSET: {
			// find today's sunset
			Poco::DateTime result = sunRiseSet.GetSunset(schedule->astroLat, schedule->astroLon, today);
			// sun already set?
			if (result < now)
				// find tomorrow's sunset
				result = sunRiseSet.GetSunset(schedule->astroLat, schedule->astroLon, Poco::DateTime(now.julianDay() + 1));
			// values are specified in local time; convert to UTC
			result.makeUTC(Poco::Timezone::tzd());
			return result.timestamp() + schedule->astroOffset * Poco::Timestamp::resolution();		// add offset in microseconds
		}
		}
		return Poco::Timestamp();
	} else
	if (schedule->type == MANUAL) {
		// try to get the value from the dependent dial port
		int64_t position;
		try {
			// the dial port contains the UTC timestamp
			schedule->data.manualPort->getState(&position);
			Poco::Timestamp timestamp(position * Poco::Timestamp::resolution());
			return timestamp;
		} catch (...) {
			// ignore errors, can't schedule from this port
			return Poco::Timestamp();
		}
	} else
		// return default value (now; must not be enqueued)
		return Poco::Timestamp();
}

void TimerPort::addNotification(ScheduleNotification::Ptr notification, Poco::Timestamp timestamp) {
	Poco::Timestamp now;

	// for debug output: convert UTC timestamp to local time
	Poco::LocalDateTime ldt(timestamp);
	if (timestamp > now) {
		std::string timeText = Poco::DateTimeFormatter::format(ldt, this->openhat->timestampFormat);
		if (!notification->deactivate)
			this->logVerbose("Next scheduled time for node " + 
					notification->schedule->nodeName + " is: " + timeText);
		// add with the specified activation time
		this->queue.enqueueNotification(notification, timestamp);
		if (!notification->deactivate)
			notification->schedule->nextEvent = timestamp;
	} else {
		this->logNormal("Warning: Scheduled time for node " + 
				notification->schedule->nodeName + " lies in the past, ignoring: " + Poco::DateTimeFormatter::format(ldt, this->openhat->timestampFormat));
/*
		this->log(this->ID() + ": Timestamp is: " + Poco::DateTimeFormatter::format(timestamp, "%Y-%m-%d %H:%M:%S"));
		this->log(this->ID() + ": Now is      : " + Poco::DateTimeFormatter::format(now, "%Y-%m-%d %H:%M:%S"));
*/
	}
}

void TimerPort::setOutputs(int8_t outputLine) {
	auto it = this->outputPorts.begin();
	auto ite = this->outputPorts.end();
	while (it != ite) {
		try {
			// toggle?
			if (outputLine < 0) {
				// get current output port state
				uint8_t mode;
				uint8_t line;
				(*it)->getState(&mode, &line);
				// set new state
				outputLine = (line == 1 ? 0 : 1);
			}

			// set output line
			(*it)->setLine(outputLine);
		} catch (Poco::Exception &e) {
			this->logNormal("Error setting output port state: " + (*it)->ID() + ": " + this->openhat->getExceptionMessage(e));
		}
		++it;
	}
}

uint8_t TimerPort::doWork(uint8_t canSend)  {
	DigitalPort::doWork(canSend);

	// detect connection status change
	bool connected = this->openhat->isConnected() != 0;
	bool connectionStateChanged = connected != this->masterLoggedIn;
	this->masterLoggedIn = connected;

	// timer not active?
	if (this->line != 1)
		return OPDI_STATUS_OK;

	Poco::Notification::Ptr notification = nullptr;

	if (connectionStateChanged) {
		// check whether a schedule is specified for this event
		auto it = this->schedules.begin();
		auto ite = this->schedules.end();
		while (it != ite) {
			if ((*it).type == (connected ? ONLOGIN : ONLOGOUT)) {
				this->logDebug("Connection status change detected; executing schedule " + (*it).nodeName + ((*it).type == ONLOGIN ? " (OnLogin)" : " (OnLogout)"));
				// schedule found; create event notification
				notification = new ScheduleNotification(&*it, false);
				break;
			}
			++it;
		}
	}

	// time correction since last calculation or doWork iteration?
	// this may happen due to daylight saving time or timezone changes
	// or due to system time corrections (user action, NTP etc)
	if (abs(Poco::Timestamp() - this->lastWorkTimestamp) > Poco::Timestamp::resolution() * 5) {
		this->logVerbose("Relevant system time change detected; recalculating schedules");
		this->recalculateSchedules();
	}

	this->lastWorkTimestamp = Poco::Timestamp();
	
	// notification created due to status change?
	if (notification.isNull())
		// no, get next object from the priority queue
		notification = this->queue.dequeueNotification();

	// notification will only be a valid object if a schedule is due
	if (notification) {
		try {
			ScheduleNotification::Ptr workNf = notification.cast<ScheduleNotification>();
			Schedule* schedule = workNf->schedule;

			this->logVerbose(std::string("Timer reached scheduled ") + (workNf->deactivate ? "deactivation " : "")
				+ "time for node: " + schedule->nodeName);

			schedule->occurrences++;

			// cause master's UI state refresh if no deactivate
			this->refreshRequired = !workNf->deactivate;

			// calculate next occurrence depending on type; maximum ocurrences must not have been reached
			if ((!workNf->deactivate) && (schedule->type != ONCE) 
				&& ((schedule->maxOccurrences < 0) || (schedule->occurrences < schedule->maxOccurrences))) {

				Poco::Timestamp nextOccurrence = this->calculateNextOccurrence(schedule);
				if (nextOccurrence > Poco::Timestamp()) {
					// add with the specified occurrence time
					this->addNotification(workNf, nextOccurrence);
				} else {
					// warn if unable to calculate next occurrence; except if login or logout event
					if ((schedule->type != ONLOGIN) && (schedule->type != ONLOGOUT))
						this->logNormal("Warning: Next scheduled time for " + schedule->nodeName + " could not be determined");
				}
			}

			// need to deactivate?
			if ((!workNf->deactivate) && (schedule->duration > 0)) {
				// enqueue the notification for the deactivation
				ScheduleNotification* notification = new ScheduleNotification(schedule, true);
				Poco::Timestamp deacTime;
				Poco::Timestamp::TimeDiff timediff = schedule->duration * Poco::Timestamp::resolution() / 1000;
				deacTime += timediff;
				Poco::DateTime deacLocal(deacTime);
				deacLocal.makeLocal(Poco::Timezone::tzd());
				this->logVerbose("Scheduled deactivation time for node " + schedule->nodeName + " is at: " + 
						Poco::DateTimeFormatter::format(deacLocal, this->openhat->timestampFormat)
						+ "; in " + this->to_string(timediff / 1000000) + " second(s)");
				// add with the specified deactivation time
				this->addNotification(notification, deacTime);
			}

			// set the output ports' state
			int8_t outputLine = -1;	// assume: toggle
			if (schedule->action == SET_HIGH)
				outputLine = (workNf->deactivate ? 0 : 1);
			if (schedule->action == SET_LOW)
				outputLine = (workNf->deactivate ? 1 : 0);

			this->setOutputs(outputLine);
		} catch (Poco::Exception &e) {
			this->logNormal("Error processing timer schedule: " + this->openhat->getExceptionMessage(e));
		}
	}

	// determine next scheduled time text
	this->nextOccurrenceStr = "";

	if (this->line == 1) {
		// go through schedules
		Poco::Timestamp ts = Poco::Timestamp::TIMEVAL_MAX;
		Poco::Timestamp now;
		auto it = this->schedules.begin();
		auto ite = this->schedules.end();
		// select schedule with the earliest nextEvent timestamp
		while (it != ite) {
			if (((*it).nextEvent > now) && ((*it).nextEvent < ts)) {
				ts = (*it).nextEvent;
			}
			++it;
		}

		if (ts < Poco::Timestamp::TIMEVAL_MAX) {
			// calculate extended port info text
			Poco::LocalDateTime ldt(ts);
			this->nextOccurrenceStr = this->nextEventText + Poco::DateTimeFormatter::format(ldt, this->timestampFormat);
		}
	}

	return OPDI_STATUS_OK;
}

void TimerPort::recalculateSchedules(Schedule* /*activatingSchedule*/) {
	// clear all schedules
	this->queue.clear();
	for (auto it = this->schedules.begin(), ite = this->schedules.end(); it != ite; ++it) {
		Schedule* schedule = &*it;
		// calculate
		Poco::Timestamp nextOccurrence = this->calculateNextOccurrence(schedule);
		if (nextOccurrence > Poco::Timestamp()) {
			// add with the specified occurrence time
			this->addNotification(new ScheduleNotification(schedule, false), nextOccurrence);
		} else {
			if ((schedule->type != ONLOGIN) && (schedule->type != ONLOGOUT))
				this->logVerbose("Next scheduled time for " + schedule->nodeName + " could not be determined");
			schedule->nextEvent = Poco::Timestamp();
		}
	}
	this->refreshRequired = true;
}

void TimerPort::setLine(uint8_t line, ChangeSource changeSource) {
	bool wasLow = (this->line == 0);

	DigitalPort::setLine(line, changeSource);

	// set to Low?
	if (this->line == 0) {
		if (!wasLow) {
			// clear all schedules
			this->queue.clear();
			if (this->propagateSwitchOff)
				this->setOutputs(0);
		}
	}

	// set to High?
	if (this->line == 1) {
		if (wasLow)
			this->recalculateSchedules();
	}

	this->refreshRequired = true;
}

std::string TimerPort::getExtendedState(void) const {
	std::string result = DigitalPort::getExtendedState();
	std::string myText;
	if (this->line != 1) {
		myText = this->deactivatedText;
	} else {
		if (this->nextOccurrenceStr == "")
			myText = this->notScheduledText;
		else
			myText = this->nextOccurrenceStr;
	}
	myText = "text=" + this->escapeKeyValueText(myText);
	// append own text to base class text if available
	return result.empty() ? myText : result + ";" + myText;
}

}		// namespace openhat
