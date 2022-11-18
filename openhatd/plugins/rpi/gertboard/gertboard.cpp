
// OpenHAT plugin that supports the Gertboard on Raspberry Pi

#include <stdio.h>
#include <unistd.h>			//Used for UART
#include <fcntl.h>			//Used for UART
#include <termios.h>		//Used for UART

#include "Poco/Tuple.h"

#ifdef CMAKE_BUILD
extern "C" {
#endif
#include "gb_common.h"
#include "gb_spi.h"
#include "gb_pwm.h"
#ifdef CMAKE_BUILD
}
#endif

#include "../rpi.h"

#include "LinuxOpenHAT.h"

// mapping to different revisions of RPi
// first number indicates Gertboard label (GPxx), second specifies internal pin number
static int pinMapRev1[][2] = {
	{0, 0}, {1, 1}, {4, 4}, {7, 7}, {8, 8}, {9, 9},
	{10, 10}, {11, 11}, {14, 14}, {15, 15}, {17, 17},
	{18, 18}, {21, 21}, {22, 22}, {23, 23}, {24, 24}, {25, 25},
	{-1, -1}
};

static int pinMapRev2[][2] = {
	{0, 2}, {1, 3}, {4, 4}, {7, 7}, {8, 8}, {9, 9},
	{10, 10}, {11, 11}, {14, 14}, {15, 15}, {17, 17},
	{18, 18}, {21, 27}, {22, 22}, {23, 23}, {24, 24}, {25, 25},
	{-1, -1}
};

namespace {

///////////////////////////////////////////////////////////////////////////////
// GertboardPlugin: Plugin for providing Gertboard resources to OpenHAT
///////////////////////////////////////////////////////////////////////////////

class GertboardPlugin : public IOpenHATPlugin, public openhat::IConnectionListener {

protected:
	openhat::AbstractOpenHAT* openhat;
	std::string nodeID;
	opdi::LogVerbosity logVerbosity;

	int (*pinMap)[][2];		// the map to use for mapping Gertboard pins to internal pins

	std::string serialDevice;
	uint32_t serialTimeoutMs;
	// At bootup, pins 8 and 10 are already set to UART0_TXD, UART0_RXD (ie the alt0 function) respectively
	int uart0_filestream;
	bool expanderInitialized;

	// translates external pin IDs to an internal pin; throws an exception if the pin cannot
	// be mapped or the resource is already used
	int mapAndLockPin(int pinNumber, std::string forNode);

public:
	virtual void setupPlugin(openhat::AbstractOpenHAT* openhat, const std::string& node, openhat::ConfigurationView::Ptr nodeConfig, openhat::ConfigurationView::Ptr parentConfig, const std::string& driverPath);

	virtual void masterConnected(void) override;
	virtual void masterDisconnected(void) override;

	virtual void sendExpansionPortCode(uint8_t code);
	virtual uint8_t receiveExpansionPortCode(void);
};

///////////////////////////////////////////////////////////////////////////////
// DigitalGertboardPort: Represents a digital input/output pin on the Gertboard.
///////////////////////////////////////////////////////////////////////////////

class DigitalGertboardPort : public opdi::DigitalPort {
friend class GertboardPlugin;
protected:
	openhat::AbstractOpenHAT* openhat;
	int pin;
public:
	DigitalGertboardPort(openhat::AbstractOpenHAT* openhat, const char* ID, int pin);
	virtual ~DigitalGertboardPort(void);
	virtual bool setLine(uint8_t line, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
	virtual void setMode(uint8_t mode, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
	virtual void getState(uint8_t* mode, uint8_t* line) const override;
};

///////////////////////////////////////////////////////////////////////////////
// AnalogGertboardOutput: Represents an analog output pin on the Gertboard.
///////////////////////////////////////////////////////////////////////////////

class AnalogGertboardOutput : public opdi::AnalogPort {
friend class GertboardPlugin;
protected:
	openhat::AbstractOpenHAT* openhat;
	int output;
public:
	AnalogGertboardOutput(openhat::AbstractOpenHAT* openhat, const char* id, int output);

	virtual void setMode(uint8_t mode, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
	virtual void setResolution(uint8_t resolution, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
	virtual void setReference(uint8_t reference, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
	// value: an integer value ranging from 0 to 2^resolution - 1
	virtual void setAbsoluteValue(int32_t value, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
	virtual void getState(uint8_t* mode, uint8_t* resolution, uint8_t* reference, int32_t* value) const override;
};

///////////////////////////////////////////////////////////////////////////////
// AnalogGertboardInput: Represents an analog input pin on the Gertboard.
///////////////////////////////////////////////////////////////////////////////

class AnalogGertboardInput : public opdi::AnalogPort {
friend class GertboardPlugin;
protected:
	openhat::AbstractOpenHAT* openhat;
	int input;
public:
	AnalogGertboardInput(openhat::AbstractOpenHAT* openhat, const char* id, int input);

	virtual void setMode(uint8_t mode, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
	virtual void setResolution(uint8_t resolution, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
	virtual void setReference(uint8_t reference, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
	// value: an integer value ranging from 0 to 2^resolution - 1
	virtual void setAbsoluteValue(int32_t value, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
	virtual void getState(uint8_t* mode, uint8_t* resolution, uint8_t* reference, int32_t* value) const override;
};

///////////////////////////////////////////////////////////////////////////////
// GertboardButton: Represents a button on the Gertboard.
// If the button is pressed, a connected master will be notified to update
// its state. This port permanently queries the state of the button's pin.
///////////////////////////////////////////////////////////////////////////////

class GertboardButton : public opdi::DigitalPort {
friend class GertboardPlugin;
protected:
	openhat::AbstractOpenHAT* openhat;
	int pin;
	uint8_t lastQueriedState;
	uint64_t lastQueryTime;
	uint64_t queryInterval;
	uint64_t lastRefreshTime;
	uint64_t refreshInterval;

	virtual uint8_t doWork(uint8_t canSend) override;
	virtual uint8_t queryState(void);
public:
	GertboardButton(openhat::AbstractOpenHAT* openhat, const char* ID, int pin);
	virtual bool setLine(uint8_t line, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
	virtual void setMode(uint8_t mode, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
	virtual void getState(uint8_t* mode, uint8_t* line) const override;
};

///////////////////////////////////////////////////////////////////////////////
// GertboardPWM: Represents a PWM pin on the Gertboard.
// Pin 18 is currently the only pin that supports hardware PWM.
///////////////////////////////////////////////////////////////////////////////

class GertboardPWM: public opdi::DialPort {
friend class GertboardPlugin;
protected:
	openhat::AbstractOpenHAT* openhat;
	int pin;
	bool inverse;
public:
	GertboardPWM(openhat::AbstractOpenHAT* openhat, const int pin, const char* ID, bool inverse);
	virtual ~GertboardPWM(void);
	virtual bool setPosition(int64_t position, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
};

///////////////////////////////////////////////////////////////////////////////
// DigitalExpansionPort: A digital input/output pin on the Atmega328P on the Gertboard.
// Requires that the AtmegaPortExpander software runs on the microcontroller.
// Communicates via RS232, so the connections between GP14/15 and MCRX/TX (pins PD0/PD1)
// must be present. The virtual port definitions are:

#define VP0(reg) BIT(D,2,reg)
#define VP1(reg) BIT(D,3,reg)
#define VP2(reg) BIT(D,4,reg)
#define VP3(reg) BIT(D,5,reg)
#define VP4(reg) BIT(D,6,reg)
#define VP5(reg) BIT(D,7,reg)
#define VP6(reg) BIT(B,0,reg)
#define VP7(reg) BIT(B,1,reg)
#define VP8(reg) BIT(B,2,reg)
#define VP9(reg) BIT(B,3,reg)
#define VP10(reg) BIT(B,4,reg)
#define VP11(reg) BIT(B,5,reg)
#define VP12(reg) BIT(C,0,reg)
#define VP13(reg) BIT(C,1,reg)
#define VP14(reg) BIT(C,2,reg)
#define VP15(reg) BIT(C,3,reg)

///////////////////////////////////////////////////////////////////////////////

#define OUTPUT	6
#define PULLUP	5
#define LINESTATE 4
#define PORTMASK 0x0f

#define SIGNALCODE	0xff
#define MAGIC		"OPDIDGBPEINIT"
#define DEACTIVATE	128

class DigitalExpansionPort : public opdi::DigitalPort {
friend class GertboardPlugin;
protected:
	// Specifies the pin behaviour in output mode.
	enum DriverType {
		/* Standard behaviour: if Low, the pin is connected to internal ground (current sink).
		If High, the pin is connected to internal Vcc (current source). */
		STANDARD,
		/* Low side driver: if Low, the pin is floating.
		If High, the pin is connected to internal ground (current sink). */
		LOW_SIDE,
		/* High side driver: if Low, the pin is floating.
		If High, the pin is connected to internal Vcc (current source). */
		HIGH_SIDE
	};

	openhat::AbstractOpenHAT* openhat;
	GertboardPlugin* gbPlugin;
	int pin;
	DriverType driverType;

public:
	DigitalExpansionPort(openhat::AbstractOpenHAT* openhat, GertboardPlugin* gbPlugin, const char* ID, int pin);
	virtual ~DigitalExpansionPort(void);
	virtual bool setLine(uint8_t line, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
	virtual void setMode(uint8_t mode, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
	virtual void getState(uint8_t* mode, uint8_t* line) const override;
};

}	// end anonymous namespace

////////////////////////////////////////////////////////////////////////
// Implementations
////////////////////////////////////////////////////////////////////////

DigitalGertboardPort::DigitalGertboardPort(openhat::AbstractOpenHAT* openhat, const char* ID, int pin) : opdi::DigitalPort(ID, OPDI_PORTDIRCAP_BIDI,	// default: input
	0) {
	this->openhat = openhat;
	this->pin = pin;
}

DigitalGertboardPort::~DigitalGertboardPort(void) {
	// release resources; configure as floating input
	// configure as floating input
	INP_GPIO(this->pin);

	GPIO_PULL = 0;
	short_wait();
	GPIO_PULLCLK0 = (1 < this->pin);
	short_wait();
	GPIO_PULL = 0;
	GPIO_PULLCLK0 = 0;
}

bool DigitalGertboardPort::setLine(uint8_t line, ChangeSource /*changeSource*/) {
	bool changed = opdi::DigitalPort::setLine(line);

	if (line == 0) {
		GPIO_CLR0 = (1 << this->pin);
	} else {
		GPIO_SET0 = (1 << this->pin);
	}
	return changed;
}

void DigitalGertboardPort::setMode(uint8_t mode, ChangeSource /*changeSource*/) {

	// cannot set pulldown mode
	if (mode == OPDI_DIGITAL_MODE_INPUT_PULLDOWN)
		throw PortError("Digital Gertboard Port does not support pulldown mode");

	opdi::DigitalPort::setMode(mode);

	if (this->getMode() == OPDI_DIGITAL_MODE_INPUT_FLOATING) {
		// configure as floating input
		INP_GPIO(this->pin);

		GPIO_PULL = 0;
		short_wait();
		GPIO_PULLCLK0 = (1 < this->pin);
		short_wait();
		GPIO_PULL = 0;
		GPIO_PULLCLK0 = 0;
	} else
	if (this->getMode() == OPDI_DIGITAL_MODE_INPUT_PULLUP) {
		// configure as input with pullup
		INP_GPIO(this->pin);

		GPIO_PULL = 2;
		short_wait();
		GPIO_PULLCLK0 = (1 < this->pin);
		short_wait();
		GPIO_PULL = 0;
		GPIO_PULLCLK0 = 0;
	} else {
		// configure as output
		INP_GPIO(this->pin); 
		OUT_GPIO(this->pin);
	}
}

void DigitalGertboardPort::getState(uint8_t* mode, uint8_t* line) const {
	*mode = this->getMode();

	// read line
	unsigned int b = GPIO_IN0;
	// bit set?
	*line = (b & (1 << this->pin)) == (unsigned int)(1 << this->pin) ? 1 : 0;
}


AnalogGertboardOutput::AnalogGertboardOutput(openhat::AbstractOpenHAT* openhat, const char* id, int output) : opdi::AnalogPort(id, 
	OPDI_PORTDIRCAP_OUTPUT, 
	// possible resolutions - set correct value in configuration (depends on your hardware)
	OPDI_ANALOG_PORT_RESOLUTION_8 | OPDI_ANALOG_PORT_RESOLUTION_10 | OPDI_ANALOG_PORT_RESOLUTION_12) {

	this->openhat = openhat;
	this->setMode(1);
	this->setResolution(8);	// most Gertboards apparently use an 8 bit DAC; but this can be changed in the configuration

	// check valid output
	if ((output < 0) || (output > 1))
		throw Poco::DataException("Invalid analog output channel number (expected 0 or 1): " + to_string(output));
	this->output = output;

	// setup analog output port
	INP_GPIO(7);  SET_GPIO_ALT(7,0);
	INP_GPIO(9);  SET_GPIO_ALT(9,0);
	INP_GPIO(10); SET_GPIO_ALT(10,0);
	INP_GPIO(11); SET_GPIO_ALT(11,0);

	// Setup SPI bus
	setup_spi();

	write_dac(this->output, this->getValue());
}

// function that handles the set direction command (opdi_set_digital_port_mode)
void AnalogGertboardOutput::setMode(uint8_t mode, ChangeSource /*changeSource*/) {
	throw PortError("Gertboard analog output mode cannot be changed");
}

void AnalogGertboardOutput::setResolution(uint8_t resolution, ChangeSource /*changeSource*/) {
	opdi::AnalogPort::setResolution(resolution);
}

void AnalogGertboardOutput::setReference(uint8_t reference, ChangeSource /*changeSource*/) {
	throw PortError("Gertboard analog output reference cannot be changed");
}

void AnalogGertboardOutput::setAbsoluteValue(int32_t value, ChangeSource /*changeSource*/) {
	opdi::AnalogPort::setAbsoluteValue(value);

	write_dac(this->output, this->getValue());
}

// function that fills in the current port state
void AnalogGertboardOutput::getState(uint8_t* mode, uint8_t* resolution, uint8_t* reference, int32_t* value) const {
	*mode = this->getMode();
	*resolution = this->getResolution();
	*reference = this->getReference();
	// set remembered value
	*value = this->getValue();
}


AnalogGertboardInput::AnalogGertboardInput(openhat::AbstractOpenHAT* openhat, const char* id, int input) : opdi::AnalogPort(id, 
	OPDI_PORTDIRCAP_INPUT, 
	// possible resolutions - set correct value in configuration (depends on your hardware)
	OPDI_ANALOG_PORT_RESOLUTION_8 | OPDI_ANALOG_PORT_RESOLUTION_10 | OPDI_ANALOG_PORT_RESOLUTION_12) {

	this->openhat = openhat;
	this->setResolution(8);	// most Gertboards apparently use an 8 bit DAC; but this can be changed in the configuration

	// check valid input
	if ((input < 0) || (input > 1))
		throw Poco::DataException("Invalid analog input channel number (expected 0 or 1): " + to_string(input));
	this->input = input;

	// setup analog input port
	INP_GPIO(8);  SET_GPIO_ALT(8,0);
	INP_GPIO(9);  SET_GPIO_ALT(9,0);
	INP_GPIO(10); SET_GPIO_ALT(10,0);
	INP_GPIO(11); SET_GPIO_ALT(11,0);

	// Setup SPI bus
	setup_spi();
}

// function that handles the set direction command (opdi_set_digital_port_mode)
void AnalogGertboardInput::setMode(uint8_t mode, ChangeSource /*changeSource*/) {
	throw PortError("Gertboard analog input mode cannot be changed");
}

void AnalogGertboardInput::setResolution(uint8_t resolution, ChangeSource /*changeSource*/) {
	opdi::AnalogPort::setResolution(resolution);
}

void AnalogGertboardInput::setReference(uint8_t reference, ChangeSource /*changeSource*/) {
	throw PortError("Gertboard analog input reference cannot be changed");
}

void AnalogGertboardInput::setAbsoluteValue(int32_t value, ChangeSource /*changeSource*/) {
	throw PortError("Gertboard analog input value cannot be set");
}

// function that fills in the current port state
void AnalogGertboardInput::getState(uint8_t* mode, uint8_t* resolution, uint8_t* reference, int32_t* value) const {
	*mode = this->getMode();
	*resolution = this->getResolution();
	*reference = this->getReference();
	// read value from ADC; correct range
	*value = opdi::AnalogPort::validateValue(read_adc(this->input));
}


GertboardButton::GertboardButton(openhat::AbstractOpenHAT* openhat, const char* ID, int pin) : opdi::DigitalPort(ID, 
	OPDI_PORTDIRCAP_INPUT,	// default: input with pullup always on
	OPDI_DIGITAL_PORT_HAS_PULLUP | OPDI_DIGITAL_PORT_PULLUP_ALWAYS) {
	this->openhat = openhat;
	this->pin = pin;
	this->setMode(OPDI_DIGITAL_MODE_INPUT_PULLUP);

	// configure as input with pullup
	INP_GPIO(this->pin);
	GPIO_PULL = 2;
	short_wait();
	GPIO_PULLCLK0 = (1 << this->pin);
	short_wait();
	GPIO_PULL = 0;
	GPIO_PULLCLK0 = 0;

	this->lastQueryTime = opdi_get_time_ms();
	this->lastQueriedState = this->queryState();
	this->queryInterval = 10;	// milliseconds
	// set the refresh interval to a reasonably high number
	// to avoid dos'ing the master with refresh requests in case
	// the button pin toggles too fast
	this->refreshInterval = 10;
}

// main work function of the button port - regularly called by the OpenHAT system
uint8_t GertboardButton::doWork(uint8_t canSend) {
	opdi::DigitalPort::doWork(canSend);

	// query interval not yet reached?
	if (opdi_get_time_ms() - this->lastQueryTime < this->queryInterval)
		return OPDI_STATUS_OK;

	// query now
	this->lastQueryTime = opdi_get_time_ms();
	uint8_t line = this->queryState();
	// current state different from last submitted state?
	if (this->lastQueriedState != line) {
		this->lastQueriedState = line;
		this->logDebug(std::string("Gertboard Button change detected (now: ")
			+ (this->lastQueriedState == 0 ? "off" : "on") + ")");
		// refresh interval not exceeded?
		if (opdi_get_time_ms() - this->lastRefreshTime > this->refreshInterval) {
			this->lastRefreshTime = opdi_get_time_ms();
			// notify master to refresh this port's state
			this->refreshRequired = true;
			this->lastRefreshTime = opdi_get_time_ms();
		}
	}

	return OPDI_STATUS_OK;
}

uint8_t GertboardButton::queryState(void) {
//		openhat->log("Querying Gertboard button pin " + to_string(this->pin) + " (" + this->id + ")");

	// read line
	unsigned int b = GPIO_IN0;
	// bit set?
	// button logic is inverse (low = pressed)
	uint8_t result = (b & (1 << this->pin)) == (unsigned int)(1 << this->pin) ? 0 : 1;

//		openhat->log("Gertboard button pin state is " + to_string((int)result));

	return result;
}

bool GertboardButton::setLine(uint8_t line, ChangeSource /*changeSource*/) {
	openhat->logNormal("Warning: Gertboard Button has no output to be changed, ignoring");
	return false;
}

void GertboardButton::setMode(uint8_t mode, ChangeSource /*changeSource*/) {
	openhat->logNormal("Warning: Gertboard Button mode cannot be changed, ignoring");
}

void GertboardButton::getState(uint8_t* mode, uint8_t* line) const {
	*mode = this->getMode();
	// remember queried line state
	*line = this->lastQueriedState;
}


GertboardPWM::GertboardPWM(openhat::AbstractOpenHAT* openhat, const int pin, const char* ID, bool inverse) 
    : opdi::DialPort(ID, 0, 1024, 1) {  // use 10 bit PWM
	if (pin != 18)
		throw Poco::ApplicationException("GertboardPWM only supports pin 18");
	this->openhat = openhat;
	this->pin = pin;
	this->inverse = inverse;

	// initialize PWM
	INP_GPIO(this->pin);  SET_GPIO_ALT(this->pin, 5);
	setup_pwm();
	force_pwm0(0, PWM0_ENABLE);
}

GertboardPWM::~GertboardPWM(void) {
	// stop PWM when the port is freed
//	this->openhat->logNormal("Freeing GertboardPWM port; stopping PWM");

	pwm_off();
}

bool GertboardPWM::setPosition(int64_t position, ChangeSource /*changeSource*/) {
	// calculate nearest position according to step
	bool changed = opdi::DialPort::setPosition(position);

	// set PWM value; inverse polarity if specified
	force_pwm0(this->getPosition(), PWM0_ENABLE | (this->inverse ? PWM0_REVPOLAR : 0));

	return changed;
}


DigitalExpansionPort::DigitalExpansionPort(openhat::AbstractOpenHAT* openhat, GertboardPlugin* gbPlugin, const char* ID, int pin) : opdi::DigitalPort(ID,
	OPDI_PORTDIRCAP_BIDI,	// default: bidirectional
	0) {
	this->openhat = openhat;
	this->gbPlugin = gbPlugin;
	this->pin = pin;
	this->driverType = STANDARD;
}

DigitalExpansionPort::~DigitalExpansionPort(void) {
}

bool DigitalExpansionPort::setLine(uint8_t line, ChangeSource /*changeSource*/) {
	bool changed =opdi::DigitalPort::setLine(line);

	uint8_t code = this->pin;

	if (this->driverType == STANDARD) {
		// set output flag
		code |= (1 << OUTPUT);
		// set High or Low accordingly
		if (this->getLine() == 1) {
			code |= (1 << LINESTATE);
		}
	} else
	if (this->driverType == LOW_SIDE) {
		// active (High)?
		if (this->getLine() == 1) {
			// set to a Low output
			code |= (1 << OUTPUT);
		} else {
			// set to a floating input (no action required)
		}
	} else
	if (this->driverType == HIGH_SIDE) {
		// active (High)?
		if (this->getLine() == 1) {
			// set to a Low output
			code |= (1 << OUTPUT);
			code |= (1 << LINESTATE);
		} else {
			// set to a floating input (no action required)
		}
	} else
		throw PortError("Unknown driver type: " + to_string(driverType));

	this->gbPlugin->sendExpansionPortCode(code);
	uint8_t returnCode = this->gbPlugin->receiveExpansionPortCode();
	if ((returnCode & PORTMASK) != (code & PORTMASK))
		throw PortError("Expansion port communication failure");

	return changed;
}

void DigitalExpansionPort::setMode(uint8_t mode, ChangeSource /*changeSource*/) {
	// cannot set pulldown mode
	if (mode == OPDI_DIGITAL_MODE_INPUT_PULLDOWN)
		throw PortError("Digital Expansion Port does not support pulldown mode");

	opdi::DigitalPort::setMode(mode);

	uint8_t code = this->pin;

	if (this->getMode() == OPDI_DIGITAL_MODE_INPUT_FLOATING) {
		// configure as floating input
	} else
	if (this->getMode() == OPDI_DIGITAL_MODE_INPUT_PULLUP) {
		code |= (1 << PULLUP);
	} else {
		if (this->driverType == STANDARD) {
			// set output flag
			code |= (1 << OUTPUT);
			// set High or Low accordingly
			if (this->getLine() == 1) {
				code |= (1 << LINESTATE);
			}
		} else
		if (this->driverType == LOW_SIDE) {
			// active (High)?
			if (this->getLine() == 1) {
				// set to a Low output
				code |= (1 << OUTPUT);
			} else {
				// set to a floating input (no action required)
			}
		} else
		if (this->driverType == HIGH_SIDE) {
			// active (High)?
			if (this->getLine() == 1) {
				// set to a Low output
				code |= (1 << OUTPUT);
				code |= (1 << LINESTATE);
			} else {
				// set to a floating input (no action required)
			}
		} else
			throw PortError("Unknown driver type: " + to_string(driverType));
	}

	this->gbPlugin->sendExpansionPortCode(code);
	uint8_t returnCode = this->gbPlugin->receiveExpansionPortCode();
	if ((returnCode & PORTMASK) != (code & PORTMASK))
		throw PortError("Expansion port communication failure");
}

void DigitalExpansionPort::getState(uint8_t* mode, uint8_t* line) const {
	*mode = this->getMode();

	if (this->getMode() == OPDI_DIGITAL_MODE_OUTPUT) {
		// return remembered state
		*line = this->getLine();
	} else {
		// read line state from the port expander
		uint8_t code = this->pin;

		if (this->getMode() == OPDI_DIGITAL_MODE_INPUT_PULLUP) {
			code |= (1 << PULLUP);
		}

		this->gbPlugin->sendExpansionPortCode(code);
		uint8_t returnCode = this->gbPlugin->receiveExpansionPortCode();

		if ((returnCode & ~(1 << LINESTATE)) != code)
			throw PortError("Expansion port communication failure");

		if ((returnCode & (1 << LINESTATE)) == (1 << LINESTATE))
			*line = 1;
		else
			*line = 0;
	}
}

///////////////////////////////////////////////////////////////////////////////
// GertboardPlugin: Plugin for providing Gertboard resources to OpenHAT
///////////////////////////////////////////////////////////////////////////////

int GertboardPlugin::mapAndLockPin(int pinNumber, std::string forNode) {
	int i = 0;
	int internalPin = -1;
	// linear search through pin definitions; map to internal pin if found
	while ((*pinMap)[i][0] >= 0) {
		if ((*pinMap)[i][0] == pinNumber) {
			internalPin = (*pinMap)[i][1];
			break;
		}
		i++;
	}
	if (internalPin < 0)
		throw Poco::DataException(std::string("The pin is not supported: ") + this->openhat->to_string(pinNumber));

	// try to lock the pin as a resource
	this->openhat->lockResource(RPI_GPIO_PREFIX + this->openhat->to_string(internalPin), forNode);

	return internalPin;
}

void GertboardPlugin::setupPlugin(openhat::AbstractOpenHAT* openhat, const std::string& node, openhat::ConfigurationView::Ptr nodeConfig, openhat::ConfigurationView::Ptr parentConfig, const std::string& /*driverPath*/) {
	this->openhat = openhat;
	this->nodeID = node;
	this->expanderInitialized = false;

	this->logVerbosity = openhat->getConfigLogVerbosity(nodeConfig, opdi::LogVerbosity::UNKNOWN);

	// try to lock the whole Gertboard as a resource
	// to avoid trouble repeatedly initializing IO
	this->openhat->lockResource(std::string("Gertboard"), node);

	// prepare Gertboard IO (requires root permissions)
	setup_io();

	// determine pin map to use
	this->pinMap = (int (*)[][2])&pinMapRev1;
	std::string revision = openhat->getConfigString(nodeConfig, node, "Revision", "1", false);
	if (revision == "1") {
		// default
	} else
	if (revision == "2") {
		this->pinMap = (int (*)[][2])&pinMapRev2;
	} else
		throw Poco::DataException("Gertboard revision not supported (no pin map; use 1 or 2); Revision = " + revision);	

	// store main node's group (will become the default of ports)
	std::string group = nodeConfig->getString("Group", "");

	// to use the expansion ports we need to have a serial device name
	// if this is not configured we can't use the serial port expansion
	this->serialDevice = nodeConfig->getString("SerialDevice", "");

	// if serial device specified, open and configure it
	if (this->serialDevice != "") {
		
		this->openhat->logDebug(node + ": Configuring SerialDevice '" + this->serialDevice + "'");

		// try to lock the serial device as a resource
		this->openhat->lockResource(this->serialDevice, node);

		int timeout = nodeConfig->getInt("SerialTimeout", 100);
		if (timeout < 0)
			throw Poco::DataException("SerialTimeout may not be negative: " + this->openhat->to_string(timeout));

		this->serialTimeoutMs = timeout;

		this->uart0_filestream = open(this->serialDevice.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);		// open in non blocking read/write mode
		if (this->uart0_filestream == -1)
			throw Poco::Exception("Unable to open serial device " + this->serialDevice);

		struct termios options;
		tcgetattr(uart0_filestream, &options);
		options.c_cflag = B19200 | CS8 | CLOCAL | CREAD;		// set baud rate
		options.c_iflag = IGNPAR;
		options.c_oflag = 0;
		options.c_lflag = 0;
		tcflush(uart0_filestream, TCIFLUSH);
		tcsetattr(uart0_filestream, TCSANOW, &options);

		// the serial device, if present, uses pins 14 and 15, so these need to be locked
		// if other ports try to use these ports it will fail
		this->mapAndLockPin(14, node + " SerialDevice Port Expansion");
		this->mapAndLockPin(15, node + " SerialDevice Port Expansion");

		this->openhat->logDebug(node + ": SerialDevice " + this->serialDevice + " setup successfully with a timeout of " + this->openhat->to_string(this->serialTimeoutMs) + " ms");
	}

	// the Gertboard plugin node expects a list of node names that determine the ports that this plugin provides

	// enumerate keys of the plugin's nodes (in specified order)
	this->openhat->logVerbose(node + ": Enumerating Gertboard nodes: " + node + ".Nodes");

	Poco::AutoPtr<openhat::ConfigurationView> nodes = this->openhat->createConfigView(nodeConfig, "Nodes");
	nodeConfig->addUsedKey("Nodes");

	// get ordered list of ports
	openhat::ConfigurationView::Keys portKeys;
	nodes->keys("", portKeys);

	typedef Poco::Tuple<int, std::string> Item;
	typedef std::vector<Item> ItemList;
	ItemList orderedItems;

	// create ordered list of port keys (by priority)
	for (openhat::ConfigurationView::Keys::const_iterator it = portKeys.begin(); it != portKeys.end(); ++it) {
		int itemNumber = nodes->getInt(*it, 0);
		// check whether the item is active
		if (itemNumber < 0)
			continue;

		// insert at the correct position to create a sorted list of items
		ItemList::iterator nli = orderedItems.begin();
		while (nli != orderedItems.end()) {
			if (nli->get<0>() > itemNumber)
				break;
			nli++;
		}
		Item item(itemNumber, *it);
		orderedItems.insert(nli, item);
	}

	if (orderedItems.size() == 0) {
		this->openhat->logNormal(node + ": Warning: No ports configured in node " + node + ".Nodes; is this intended?");
	}

	// go through items, create ports in specified order
	ItemList::const_iterator nli = orderedItems.begin();
	while (nli != orderedItems.end()) {

		std::string nodeName = nli->get<1>();

		this->openhat->logVerbose(node + ": Setting up Gertboard port(s) for node: " + nodeName);

		// get port section from the configuration
		Poco::AutoPtr<openhat::ConfigurationView> portConfig = this->openhat->createConfigView(parentConfig, nodeName);

		// get port type (required)
		std::string portType = openhat->getConfigString(portConfig, nodeName, "Type", "", true);

		if (portType == "DigitalPort") {
			// read pin number
			int pinNumber = portConfig->getInt("Pin", -1);
			// check whether the pin is valid; determine internal pin
			if (pinNumber < 0)
				throw Poco::DataException("A 'Pin' greater or equal than 0 must be specified for a Gertboard Digital port");

			int internalPin = this->mapAndLockPin(pinNumber, nodeName);

			// setup the port instance and add it; use internal pin number
			DigitalGertboardPort* port = new DigitalGertboardPort(openhat, nodeName.c_str(), internalPin);
			// set default group: Gertboard's node's group
			port->setGroup(group);
			port->logVerbosity = openhat->getConfigLogVerbosity(portConfig, this->logVerbosity);
			openhat->configureDigitalPort(portConfig, port);
			openhat->addPort(port);
		} else
		if (portType == "Button") {
			// read pin number
			int pinNumber = portConfig->getInt("Pin", -1);
			// check whether the pin is valid; determine internal pin
			if (pinNumber < 0)
				throw Poco::DataException("A 'Pin' greater or equal than 0 must be specified for a Gertboard Button port");

			int internalPin = this->mapAndLockPin(pinNumber, nodeName);

			// setup the port instance and add it; use internal pin number
			GertboardButton* port = new GertboardButton(openhat, nodeName.c_str(), internalPin);
			// set default group: Gertboard's node's group
			port->setGroup(group);
			port->logVerbosity = openhat->getConfigLogVerbosity(portConfig, this->logVerbosity);
			openhat->configureDigitalPort(portConfig, port);
			openhat->addPort(port);
		} else
		if (portType == "AnalogOutput") {
			// read output number
			int outputNumber = portConfig->getInt("Output", -1);
			// check whether the pin is valid; determine internal pin
			if ((outputNumber < 0) || (outputNumber > 1))
				throw Poco::DataException("An 'Output' of 0 or 1 must be specified for a Gertboard AnalogOutput port: " + openhat->to_string(outputNumber));

			// the analog output uses SPI; lock internal pins for SPI ports but ignore it if they are already locked
			// because another AnalogOutput or AnalogInput may also use SPI
			try {
				openhat->lockResource(RPI_GPIO_PREFIX + "7", nodeName);
				openhat->lockResource(RPI_GPIO_PREFIX + "9", nodeName);
				openhat->lockResource(RPI_GPIO_PREFIX + "10", nodeName);
				openhat->lockResource(RPI_GPIO_PREFIX + "11", nodeName);
			} catch (...) {}

			// lock analog output resource; this one may not be shared
			openhat->lockResource(std::string("AnalogOut") + openhat->to_string(outputNumber), nodeName);

			// setup the port instance and add it; use internal pin number
			AnalogGertboardOutput* port = new AnalogGertboardOutput(openhat, nodeName.c_str(), outputNumber);
			// set default group: Gertboard's node's group
			port->setGroup(group);
			port->logVerbosity = openhat->getConfigLogVerbosity(portConfig, this->logVerbosity);
			openhat->configureAnalogPort(portConfig, port);
			openhat->addPort(port);
		} else
		if (portType == "AnalogInput") {
			// read input number
			int inputNumber = portConfig->getInt("Input", -1);
			// check whether the pin is valid; determine internal pin
			if ((inputNumber < 0) || (inputNumber > 1))
				throw Poco::DataException("An 'Input' of 0 or 1 must be specified for a Gertboard AnalogInput port: " + openhat->to_string(inputNumber));

			// the analog input uses SPI; lock internal pins for SPI ports but ignore it if they are already locked
			// because another AnalogOutput or AnalogInput may also use SPI
			try {
				openhat->lockResource(RPI_GPIO_PREFIX + "8", nodeName);
				openhat->lockResource(RPI_GPIO_PREFIX + "9", nodeName);
				openhat->lockResource(RPI_GPIO_PREFIX + "10", nodeName);
				openhat->lockResource(RPI_GPIO_PREFIX + "11", nodeName);
			} catch (...) {}

			// lock analog input resource; this one may not be shared
			openhat->lockResource(std::string("AnalogIn") + openhat->to_string(inputNumber), nodeName);

			// setup the port instance and add it; use internal pin number
			AnalogGertboardInput* port = new AnalogGertboardInput(openhat, nodeName.c_str(), inputNumber);
			// set default group: Gertboard's node's group
			port->setGroup(group);
			port->logVerbosity = openhat->getConfigLogVerbosity(portConfig, this->logVerbosity);
			openhat->configureAnalogPort(portConfig, port);
			openhat->addPort(port);
		} else
		if (portType == "PWM") {
			// read inverse flag
			bool inverse = portConfig->getBool("Inverse", false);

			// the PWM output uses pin 18
			int internalPin = this->mapAndLockPin(18, nodeName);

			// setup the port instance and add it; use internal pin number
			GertboardPWM* port = new GertboardPWM(openhat, internalPin, nodeName.c_str(), inverse);
			// set default group: Gertboard's node's group
			port->setGroup(group);
			port->logVerbosity = openhat->getConfigLogVerbosity(portConfig, this->logVerbosity);
			openhat->configurePort(portConfig, port, 0);

			int value = portConfig->getInt("Value", -1);
			if (value > -1) {
				port->setPosition(value);
			}
			openhat->addPort(port);
		} else
		if (portType == "DigitalExpansionPort") {
			// serial device must be configured first
			if (this->serialDevice == "")
				throw Poco::DataException("To use the port expansion, please set the SerialDevice parameter in the Gertboard node");

			// the expansion port must be activated by sending it a "magic" string
			// do this only once
			if (!this->expanderInitialized) {
				this->openhat->logVerbose(node + ": Initializing Atmega Port Expander");

				// check whether it's already initialized
				// for example if OpenHAT has crashed and is being restarted
				this->sendExpansionPortCode(SIGNALCODE);
				try {
					if (this->receiveExpansionPortCode() == SIGNALCODE)
					this->openhat->logVerbose(node + ": Port Expander is already initialized");
					this->expanderInitialized = true;		
				} catch (...) {}

				if (!this->expanderInitialized) {
					for (size_t i = 0; i < sizeof(MAGIC); i++) {
						this->sendExpansionPortCode(MAGIC[i]);
					}
					// try to receive the confirmation code
					if (this->receiveExpansionPortCode() != SIGNALCODE) {
						throw Poco::DataException(node + ": Port Expander did not respond to initialization");
					}
					this->openhat->logVerbose(node + ": Port Expander initialization sequence successfully completed");
					this->expanderInitialized = true;
				}
			}

			// read pin number
			int pinNumber = portConfig->getInt("Pin", -1);
			// check whether the pin is valid; determine internal pin
			if ((pinNumber < 0) || (pinNumber > 15))
				throw Poco::DataException("A 'Pin' number between 0 and 15 must be specified for a Gertboard Digital Expansion port");

			// try to lock the pin as a resource
			this->openhat->lockResource(std::string("Gertboard Expansion Port ") + this->openhat->to_string(pinNumber), nodeName);

			// setup the port instance and add it; use internal pin number
			DigitalExpansionPort* port = new DigitalExpansionPort(openhat, this, nodeName.c_str(), pinNumber);
			// set default group: Gertboard's node's group
			port->setGroup(group);
			port->logVerbosity = openhat->getConfigLogVerbosity(portConfig, this->logVerbosity);

			// evaluate driver type (important: before regular configuration which may set output mode and state)
			std::string driverType = portConfig->getString("DriverType", "");
			if (driverType == "Standard") {
				port->driverType = DigitalExpansionPort::STANDARD;
			} else
			if (driverType == "LowSide") {
				port->driverType = DigitalExpansionPort::LOW_SIDE;
			} else
			if (driverType == "HighSide") {
				port->driverType = DigitalExpansionPort::HIGH_SIDE;
			} else
			if (driverType != "")
				throw Poco::DataException("Invalid value for DriverType: Expected 'Standard', 'LowSide' or 'HighSide': " + driverType);

			openhat->configureDigitalPort(portConfig, port);

			openhat->addPort(port);
		} else
			throw Poco::DataException("This plugin does not support the port type", portType);

		nli++;
	}

	this->openhat->logVerbose(node + ": GertboardPlugin setup completed successfully");
}

void GertboardPlugin::masterConnected() {
}

void GertboardPlugin::masterDisconnected() {
}

void GertboardPlugin::sendExpansionPortCode(uint8_t code){
	if (uart0_filestream != -1) {
		this->openhat->logDebug(this->nodeID + ": Sending expansion port control code: " + this->openhat->to_string((int)code));

		// flush the input buffer
		uint8_t rx_buffer[1];
		while (read(uart0_filestream, (void*)rx_buffer, 1) > 0);
		int count = write(uart0_filestream, &code, 1);
		if (count < 0)
			throw opdi::Port::PortError(this->nodeID + ": Serial communication error while sending: " + this->openhat->to_string(errno));
	} else
		throw opdi::Port::PortError(this->nodeID + ": Serial communication not initialized");
}

uint8_t GertboardPlugin::receiveExpansionPortCode(void) {
	if (uart0_filestream != -1) {
		uint8_t rx_buffer[1];
		uint64_t ticks = opdi_get_time_ms();

		int rx_length = 0;

		while ((rx_length <= 0) && (opdi_get_time_ms() - ticks < this->serialTimeoutMs)) {
			rx_length = read(uart0_filestream, (void*)rx_buffer, 1);
		}
		if (rx_length < 0)
			throw opdi::Port::PortError(this->nodeID + ": Serial communication error while receiving: " + this->openhat->to_string(errno));
		else if (rx_length == 0)
			throw opdi::Port::PortError(this->nodeID + ": Serial communication timeout");

		this->openhat->logDebug(this->nodeID + ": Received expansion port return code: " + this->openhat->to_string((int)rx_buffer[0]));
		return rx_buffer[0];
	} else
		throw opdi::Port::PortError(this->nodeID + ": Serial communication not initialized");
}

// plugin factory function
extern "C" IOpenHATPlugin* GetPluginInstance(int majorVersion, int minorVersion, int patchVersion) {
	// check whether the version is supported
	if ((majorVersion != openhat::OPENHAT_MAJOR_VERSION) || (minorVersion != openhat::OPENHAT_MINOR_VERSION))
		throw Poco::Exception("This plugin requires OpenHAT version "
			OPDI_QUOTE(openhat::OPENHAT_MAJOR_VERSION) "." OPDI_QUOTE(openhat::OPENHAT_MINOR_VERSION));

	// return a new instance of this plugin
	return new GertboardPlugin();
}
