
#ifdef _WINDOWS
#include "WindowsOpenHAT.h"
#endif

#ifdef linux
#include "LinuxOpenHAT.h"
#endif

namespace {

class WindowPort : public opdi::SelectPort {
friend class WindowPlugin;
protected:

#define POSITION_OFF	0
#define POSITION_CLOSED	1
#define POSITION_OPEN	2
#define POSITION_AUTO	3

	enum WindowMode {
		NOT_SET,
		H_BRIDGE,
		SERIAL_RELAY
	};

	enum WindowState {
		UNKNOWN,
		UNKNOWN_WAITING,
		CLOSED,
		OPEN,
		WAITING_AFTER_ENABLE,
		WAITING_BEFORE_DISABLE_OPEN,
		WAITING_BEFORE_DISABLE_CLOSED,
		WAITING_BEFORE_DISABLE_ERROR,
		WAITING_BEFORE_ENABLE_OPENING,
		WAITING_BEFORE_ENABLE_CLOSING,
		WAITING_AFTER_DISABLE,
		CLOSING,
		OPENING,
		ERR
	};

	enum ResetTo {
		RESET_NONE,
		RESET_TO_CLOSED,
		RESET_TO_OPEN
	};

	openhat::AbstractOpenHAT* openhat;

	// configuration
	std::string sensorClosedPortStr;
	uint8_t sensorClosedValue;
	std::string sensorOpenPortStr;
	uint8_t sensorOpenValue;
	std::string motorAStr;
	std::string motorBStr;
	std::string direction;
	uint8_t motorActive;
	uint64_t motorDelay;
	std::string enable;
	uint64_t enableDelay;
	uint8_t enableActive;
	uint64_t openingTime;
	uint64_t closingTime;
	std::string autoOpenStr;
	std::string autoCloseStr;
	std::string forceOpenStr;
	std::string forceCloseStr;
	std::string statusPortStr;
	std::string errorPortStr;
	std::string resetPortStr;
	ResetTo resetTo;
	int16_t positionAfterClose;
	int16_t positionAfterOpen;

	// processed configuration
	opdi::DigitalPort* sensorClosedPort;
	opdi::DigitalPort* sensorOpenPort;
	opdi::DigitalPort* motorAPort;
	opdi::DigitalPort* motorBPort;
	opdi::DigitalPort* directionPort;
	opdi::DigitalPort* enablePort;
	opdi::SelectPort* statusPort;

	opdi::DigitalPortList autoOpenPorts;
	opdi::DigitalPortList autoClosePorts;
	opdi::DigitalPortList forceOpenPorts;
	opdi::DigitalPortList forceClosePorts;
	opdi::DigitalPortList errorPorts;
	opdi::DigitalPortList resetPorts;

	WindowMode mode;
	// state
	WindowState targetState;
	WindowState currentState;
	uint64_t delayTimer;
	uint64_t openTimer;
	// set to true when the position has been just changed by the master
	bool positionNewlySet;
	bool isMotorEnabled;
	bool isMotorOn;

	void prepare() override;

	// gets the line status from the digital port
	uint8_t getPortLine(opdi::DigitalPort* port);

	// sets the line status of the digital port
	void setPortLine(opdi::DigitalPort* port, uint8_t line);

	bool isSensorClosed(void);

	bool isSensorOpen(void);

	void enableMotor(void);

	void disableMotor(void);

	void setMotorOpening(void);
	
	void setMotorClosing(void);

	void setMotorOff(void);

	std::string getMotorStateText(void);

	std::string getStateText(WindowState state);

	void setCurrentState(WindowState state);

	void setTargetState(WindowState state);

	void checkOpenHBridge(void);

	void checkCloseHBridge(void);

	virtual uint8_t doWork(uint8_t canSend) override;

public:
	WindowPort(openhat::AbstractOpenHAT* openhat, const char* id);

	virtual void setPosition(uint16_t position, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;
	
	virtual void getState(uint16_t* position) const override;
};

////////////////////////////////////////////////////////////////////////
// Plugin main class
////////////////////////////////////////////////////////////////////////

class WindowPlugin : public IOpenHATPlugin, public openhat::IConnectionListener {

protected:
	openhat::AbstractOpenHAT* openhat;

public:
	virtual void setupPlugin(openhat::AbstractOpenHAT* openhat, const std::string& node, openhat::ConfigurationView* config, const std::string& driverPath) override;

	virtual void masterConnected(void) override;
	virtual void masterDisconnected(void) override;
};

}	// end anonymous namespace

WindowPort::WindowPort(openhat::AbstractOpenHAT* openhat, const char* id) : opdi::SelectPort(id) {
	this->opdi = this->openhat = openhat;

	this->targetState = UNKNOWN;
	this->currentState = UNKNOWN;
	this->mode = NOT_SET;
	this->positionNewlySet = false;
	this->isMotorEnabled = false;
	this->isMotorOn = false;
	this->refreshMode =RefreshMode::REFRESH_NOT_SET;
	this->sensorClosedPort = nullptr;
	this->sensorOpenPort = nullptr;
	this->enablePort = nullptr;
	this->directionPort = nullptr;
	this->statusPort = nullptr;
	this->resetTo = RESET_NONE;
	this->positionAfterClose = -1;
	this->positionAfterOpen = -1;
	this->sensorClosedValue = 1;
	this->sensorOpenValue = 1;
	this->motorActive = false;
	this->motorDelay = 0;
	this->enableDelay = false;
	this->enableActive = false;
	this->openingTime = 0;
	this->closingTime = 0;
	this->motorAPort = nullptr;
	this->motorBPort = nullptr;
	this->delayTimer = 0;
	this->openTimer = 0;
}

void WindowPort::setPosition(uint16_t position, ChangeSource changeSource) {
	// recovery from error state is always possible
	if ((this->currentState == ERR) || this->position != position) {
		// prohibit disabling the automatic mode by setting the position to the current state
		if ((this->positionAfterClose >= 0) && (position == POSITION_CLOSED ) && (this->currentState == CLOSED))
			position = POSITION_AUTO;
		if ((this->positionAfterOpen >= 0) && (position == POSITION_OPEN ) && (this->currentState == OPEN))
			position = POSITION_AUTO;

		opdi::SelectPort::setPosition(position, changeSource);
		this->positionNewlySet = true;

		std::string info = std::string("Setting position to ") + to_string(position) + " ";
		switch (position) {
		case POSITION_OFF:
			info += "(OFF)";
			break;
		case POSITION_CLOSED:
			info += "(CLOSED)";
			break;
		case POSITION_OPEN:
			info += "(OPEN)";
			break;
		default:
			info += "(AUTOMATIC)";
			break;
		}
		this->logVerbose(info + "; current state is: " + this->getStateText(this->currentState) + this->getMotorStateText());
	}
}

void WindowPort::getState(uint16_t* position) const {
	if (this->currentState == ERR && !this->positionNewlySet)
		throw PortError(this->ID() + ": Sensor or motor failure or misconfiguration");
	opdi::SelectPort::getState(position);
}

void WindowPort::prepare() {
	this->logDebug("Preparing WindowPort");
	opdi::Port::prepare();

	// find ports; throws errors if something required is missing
	if (this->sensorClosedPortStr != "")
		this->sensorClosedPort = this->findDigitalPort(this->ID(), "SensorClosed", this->sensorClosedPortStr, true);
	if (this->sensorOpenPortStr != "")
		this->sensorOpenPort = this->findDigitalPort(this->ID(), "SensorOpen", this->sensorOpenPortStr, true);
	if (this->mode == H_BRIDGE) {
		this->motorAPort = this->findDigitalPort(this->ID(), "MotorA", this->motorAStr, true);
		this->motorBPort = this->findDigitalPort(this->ID(), "MotorB", this->motorBStr, true);
		if (this->enable != "")
			this->enablePort = this->findDigitalPort(this->ID(), "Enable", this->enable, true);
		// no enable port? assume always enabled
		if (this->enablePort == nullptr)
			this->isMotorEnabled = true;

	} else if (this->mode == SERIAL_RELAY) {
		this->directionPort = this->findDigitalPort(this->ID(), "Direction", this->direction, true);
		this->enablePort = this->findDigitalPort(this->ID(), "Enable", this->enable, true);
	} else
		this->openhat->throwSettingsException(this->ID() + ": Unknown window mode; only H-Bridge and SerialRelay are supported");

	if (this->statusPortStr != "")
		this->statusPort = this->findSelectPort(this->ID(), "StatusPort", this->statusPortStr, true);

	this->findDigitalPorts(this->ID(), "AutoOpen", this->autoOpenStr, this->autoOpenPorts);
	this->findDigitalPorts(this->ID(), "AutoClose", this->autoCloseStr, this->autoClosePorts);
	this->findDigitalPorts(this->ID(), "ForceOpen", this->forceOpenStr, this->forceOpenPorts);
	this->findDigitalPorts(this->ID(), "ForceClose", this->forceCloseStr, this->forceClosePorts);
	this->findDigitalPorts(this->ID(), "ErrorPorts", this->errorPortStr, this->errorPorts);
	this->findDigitalPorts(this->ID(), "ResetPorts", this->resetPortStr, this->resetPorts);
	
	// a window port normally refreshes itself automatically unless specified otherwise
	if (this->refreshMode == RefreshMode::REFRESH_NOT_SET)
		this->refreshMode = RefreshMode::REFRESH_AUTO;
}

uint8_t WindowPort::getPortLine(opdi::DigitalPort* port) {
	uint8_t mode;
	uint8_t line;
	port->getState(&mode, &line);
	return line;
}

void WindowPort::setPortLine(opdi::DigitalPort* port, uint8_t newLine) {
	uint8_t mode;
	uint8_t line;
	port->getState(&mode, &line);
	port->setLine(newLine);
}

bool WindowPort::isSensorClosed(void) {
	// sensor not present?
	if (this->sensorClosedPort == nullptr)
		return false;
	// query the sensor
	return (this->getPortLine(this->sensorClosedPort) == this->sensorClosedValue);
}

bool WindowPort::isSensorOpen(void) {
	// sensor not present?
	if (this->sensorOpenPort == nullptr)
		return false;
	// query the sensor
	return (this->getPortLine(this->sensorOpenPort) == this->sensorOpenValue);
}

void WindowPort::enableMotor(void) {
	if (this->enablePort == nullptr)
		return;
	this->logDebug("Enabling motor");
	this->setPortLine(this->enablePort, (this->enableActive == 1 ? 1 : 0));
	this->isMotorEnabled = true;
}

void WindowPort::disableMotor(void) {
	if (this->enablePort == nullptr)
		return;
	this->logDebug("Disabling motor");
	this->setPortLine(this->enablePort, (this->enableActive == 1 ? 0 : 1));
	// relay mode?
	if (this->mode == SERIAL_RELAY) {
		// disable direction (setting the relay to the OFF position in order not to waste energy)
		this->setPortLine(this->directionPort, (this->motorActive == 1 ? 0 : 1));
	}	
	this->isMotorEnabled = false;
}

void WindowPort::setMotorOpening(void) {
	this->logDebug("Setting motor to 'opening'");
	if (this->mode == H_BRIDGE) {
		this->setPortLine(this->motorAPort, (this->motorActive == 1 ? 1 : 0));
		this->setPortLine(this->motorBPort, (this->motorActive == 1 ? 0 : 1));
	} else if (this->mode == SERIAL_RELAY) {
		this->setPortLine(this->directionPort, (this->motorActive == 1 ? 1 : 0));
	}
	this->isMotorOn = true;
}

void WindowPort::setMotorClosing(void) {
	this->logDebug("Setting motor to 'closing'");
	if (this->mode == H_BRIDGE) {
		this->setPortLine(this->motorAPort, (this->motorActive == 1 ? 0 : 1));
		this->setPortLine(this->motorBPort, (this->motorActive == 1 ? 1 : 0));
	} else if (this->mode == SERIAL_RELAY) {
		this->setPortLine(this->directionPort, (this->motorActive == 1 ? 0 : 1));
	}
	this->isMotorOn = true;
}

void WindowPort::setMotorOff(void) {
	this->logDebug("Stopping motor");
	if (this->mode == H_BRIDGE) {
		this->setPortLine(this->motorAPort, (this->motorActive == 1 ? 0 : 1));
		this->setPortLine(this->motorBPort, (this->motorActive == 1 ? 0 : 1));
	} else if (this->mode == SERIAL_RELAY)
		throw Poco::ApplicationException(this->ID() + ": Cannot set motor off in Serial Relay Mode");
	this->isMotorOn = false;
}

std::string WindowPort::getMotorStateText(void) {
	if (this->mode == H_BRIDGE) {
		if (this->isMotorOn)
			return " (Motor is on)";
		else
			return std::string(" (Motor is off and ") + (this->isMotorEnabled ? "enabled" : "disabled") + ")";
	} else if (this->mode == SERIAL_RELAY) {
		return std::string(" (Motor is ") + (this->isMotorEnabled ? "enabled" : "disabled") + ")";
	} else
		return std::string(" (Mode is unknown)");
}

std::string WindowPort::getStateText(WindowState state) {
	switch (state) {
		case UNKNOWN:                        return std::string("UNKNOWN");
		case UNKNOWN_WAITING:				 return std::string("UNKNOWN_WAITING");
		case CLOSED:						 return std::string("CLOSED");
		case OPEN:							 return std::string("OPEN");
		case WAITING_AFTER_ENABLE:			 return std::string("WAITING_AFTER_ENABLE");
		case WAITING_BEFORE_DISABLE_OPEN:	 return std::string("WAITING_BEFORE_DISABLE_OPEN");
		case WAITING_BEFORE_DISABLE_CLOSED:	 return std::string("WAITING_BEFORE_DISABLE_CLOSED");
		case WAITING_BEFORE_DISABLE_ERROR:	 return std::string("WAITING_BEFORE_DISABLE_ERROR");
		case WAITING_BEFORE_ENABLE_OPENING:	 return std::string("WAITING_BEFORE_ENABLE_OPENING");
		case WAITING_BEFORE_ENABLE_CLOSING:	 return std::string("WAITING_BEFORE_ENABLE_CLOSING");
		case WAITING_AFTER_DISABLE:			 return std::string("WAITING_AFTER_DISABLE");
		case CLOSING:						 return std::string("CLOSING");
		case OPENING:						 return std::string("OPENING");
		case ERR:							 return std::string("ERR");
	};
	return std::string("unknown state ") + to_string(state);
}

void WindowPort::setCurrentState(WindowState state) {
	if (this->currentState != state) {
		this->logVerbose("Changing current state to: " + this->getStateText(state) + this->getMotorStateText());

		// if set to ERR or ERR is cleared, notify the ErrorPorts
		bool notifyErrorPorts = (this->currentState == ERR) || (state == ERR);
		this->currentState = state;

		if (notifyErrorPorts) {
			// go through error ports
			auto pi = this->errorPorts.begin();
			auto pie = this->errorPorts.end();
			while (pi != pie) {
				this->logDebug("Notifying error port: " + (*pi)->ID() + ": " + (state == ERR ? "Entering" : "Leaving") + " error state");
				this->setPortLine((*pi), (state == ERR ? 1 : 0));
				++pi;
			}
		}

		// undefined states reset the target
		if ((state == UNKNOWN) || (state == ERR))
			this->setTargetState(UNKNOWN);
		
		if ((state == UNKNOWN) ||
			(state == CLOSED) || 
			(state == OPEN) || 
			(state == ERR)) {

			if ((this->targetState != UNKNOWN) && (state == CLOSED) && (this->positionAfterClose >= 0) && (this->position != POSITION_OFF))
				this->setPosition(this->positionAfterClose);
			if ((this->targetState != UNKNOWN) && (state == OPEN) && (this->positionAfterOpen >= 0) && (this->position != POSITION_OFF))
				this->setPosition(this->positionAfterOpen);

			this->refreshRequired = (this->refreshMode ==RefreshMode::REFRESH_AUTO);
		}
		
		// update status port?
		if ((this->statusPort != nullptr) &&
			((state == UNKNOWN) ||
			(state == CLOSED) || 
			(state == OPEN) || 
			(state == CLOSING) || 
			(state == OPENING) || 
			(state == ERR))) {

			// translate to select port's position
			uint16_t selPortPos = 0;
			switch (state) {
			case UNKNOWN: selPortPos = 0; break;
			case CLOSED: selPortPos = 1; break;
			case OPEN: selPortPos = 2; break;
			case CLOSING: selPortPos = 3; break;
			case OPENING: selPortPos = 4; break;
			case ERR: selPortPos = 5; break;
			default: break;
			}

			this->logDebug("Notifying status port: " + this->statusPort->ID() + ": new position = " + to_string((int)selPortPos));

			this->statusPort->setPosition(selPortPos);
		}
	}
}

void WindowPort::setTargetState(WindowState state) {
	if (this->targetState != state) {
		this->logVerbose("Changing target state to: " + this->getStateText(state));
		this->targetState = state;
	}
}

void WindowPort::checkOpenHBridge(void) {
	if (this->targetState == OPEN) {					
		if (this->enableDelay > 0) {					
			this->delayTimer = opdi_get_time_ms();		
			this->enableMotor();						
			this->setCurrentState(WAITING_AFTER_ENABLE);
		} else {										
			this->openTimer = opdi_get_time_ms();		
			this->setMotorOpening();					
			this->setCurrentState(OPENING);				
		}												
	}
}

void WindowPort::checkCloseHBridge(void) {
	if (this->targetState == CLOSED) {					
		if (this->enableDelay > 0) {					
			this->delayTimer = opdi_get_time_ms();		
			this->enableMotor();						
			this->setCurrentState(WAITING_AFTER_ENABLE);
		} else {										
			this->openTimer = opdi_get_time_ms();		
			this->setMotorClosing();					
			this->setCurrentState(CLOSING);				
		}												
	}
}

uint8_t WindowPort::doWork(uint8_t canSend)  {
	opdi::SelectPort::doWork(canSend);

	// state machine implementations

	if (this->mode == H_BRIDGE) {
		// state machine for H-Bridge mode

		// unknown state (first call or off or transition not yet implemented)? initialize
		if (this->currentState == UNKNOWN) {
			// disable the motor
			this->setMotorOff();
			this->disableMotor();
		
			// check sensors
			if (this->isSensorClosed() && this->isSensorOpen()) {
				this->logNormal("Closed sensor signal and open sensor signal detected at the same time");
				this->setCurrentState(ERR);
			}
			else
			if (this->isSensorClosed()) {
				this->logVerbose("Closed sensor signal detected");
				this->setCurrentState(CLOSED);
			}
			else
			if (this->isSensorOpen()) {
				this->logVerbose("Open sensor signal detected");
				this->setCurrentState(OPEN);
			}
			else
				// we don't know
				this->setCurrentState(UNKNOWN_WAITING);

			// set target state according to selected position (== mode)
			if (this->position == POSITION_OPEN) {
				this->setTargetState(OPEN);
			} else
			if (this->position == POSITION_CLOSED) {
				this->setTargetState(CLOSED);
			}
		} else
		// unknown; waiting for command
		if ((this->currentState == UNKNOWN_WAITING)) {
			// check sensors
			if (this->isSensorClosed() && this->isSensorOpen()) {
				this->logNormal("Closed sensor signal and open sensor signal detected at the same time");
				this->setCurrentState(ERR);
			}
			else
			if (this->isSensorClosed()) {
				this->logVerbose("Closed sensor signal detected");
				this->setCurrentState(CLOSED);
			}
			else
			if (this->isSensorOpen()) {
				this->logVerbose("Open sensor signal detected");
				this->setCurrentState(OPEN);
			} else {
				// do not change current state

				// opening required?
				this->checkOpenHBridge();

				// closing required?
				this->checkCloseHBridge();
			}
		} else
		// open?
		if (this->currentState == OPEN) {
			// closing required?
			this->checkCloseHBridge();
			// close sensor active? (error condition)
			if (this->isSensorClosed()) {
				this->logNormal("Warning: Closed sensor signal received while assuming state OPEN");
				// set error directly
				this->setCurrentState(ERR);
			}
		} else
		// closed?
		if (this->currentState == CLOSED) {
			// opening required?
			this->checkOpenHBridge();
			// open sensor active? (error condition)
			if (this->isSensorOpen()) {
				this->logNormal("Warning: Open sensor signal received while assuming state CLOSED");
				// set error directly
				this->setCurrentState(ERR);
			}
		} else
		// waiting after enable?
		if (this->currentState == WAITING_AFTER_ENABLE) {
			// waiting time up?
			if (opdi_get_time_ms() - this->delayTimer > this->enableDelay) {
				// enable delay is up
			
				// target may have changed; check again
				if (this->targetState == OPEN) {
					// start opening immediately
					this->openTimer = opdi_get_time_ms();
					this->setMotorOpening();
					this->setCurrentState(OPENING);
				} else
				if (this->targetState == CLOSED) {
					// start closing immediately
					this->openTimer = opdi_get_time_ms();
					this->setMotorClosing();
					this->setCurrentState(CLOSING);
				} else {
					// target state is unknown; wait
					this->setCurrentState(UNKNOWN_WAITING);
				}
			}
		} else
		// waiting before disable after opening?
		if (this->currentState == WAITING_BEFORE_DISABLE_OPEN) {
			if (this->targetState == CLOSED) {
				// need to close
				this->openTimer = opdi_get_time_ms();	// reset timer; TODO set to delta value?
				this->setMotorClosing();
				this->setCurrentState(CLOSING);
			} else {
				// motor delay time up?
				if (this->isMotorOn && (opdi_get_time_ms() - this->delayTimer > this->motorDelay)) {
					// stop motor immediately
					this->setMotorOff();
				}
				// waiting time up?
				if (opdi_get_time_ms() - this->delayTimer > this->enableDelay) {
					// disable
					this->disableMotor();

					this->setCurrentState(OPEN);
				}
			}
		} else
		// waiting before disable after closing?
		if (this->currentState == WAITING_BEFORE_DISABLE_CLOSED) {
			if (this->targetState == OPEN) {
				// need to open
				this->openTimer = opdi_get_time_ms();	// reset timer; TODO set to delta value?
				this->setMotorOpening();
				this->setCurrentState(OPENING);
			} else {
				// motor delay time up?
				if (this->isMotorOn && (opdi_get_time_ms() - this->delayTimer > this->motorDelay)) {
					// stop motor immediately
					this->setMotorOff();
				}
				// waiting time up?
				if (opdi_get_time_ms() - this->delayTimer > this->enableDelay) {
					// disable
					this->disableMotor();
			
					this->setCurrentState(CLOSED);
				}
			}
		} else
		// waiting before disable after error while closing?
		if (this->currentState == WAITING_BEFORE_DISABLE_ERROR) {
			// waiting time up?
			if (opdi_get_time_ms() - this->delayTimer > this->enableDelay) {
				// disable
				this->disableMotor();
			
				this->setCurrentState(ERR);
			}
		} else
		// opening?
		if (this->currentState == OPENING) {
			if (this->targetState == CLOSED) {
				// need to close
				this->openTimer = opdi_get_time_ms();	// reset timer; TODO set to delta value?
				this->setMotorClosing();
				this->setCurrentState(CLOSING);
			}
			else
			// sensor open detected?
			if (this->isSensorOpen()) {
				this->logVerbose("Open sensor signal detected");

				// enable delay specified?
				if (this->enableDelay > 0) {
					// need to disable first
					this->delayTimer = opdi_get_time_ms();
					this->setCurrentState(WAITING_BEFORE_DISABLE_OPEN);
				}
				else {
					// the window is open now
					this->setCurrentState(OPEN);
				}
			}
			else
			// opening time up?
			if (opdi_get_time_ms() - this->openTimer > this->openingTime) {
				// stop motor
				this->setMotorOff();

				// without detecting sensor? error condition
				if (this->sensorOpenPort != nullptr) {
					this->logNormal("Warning: Open sensor signal not detected while opening");

					// enable delay specified?
					if (this->enableDelay > 0) {
						// need to disable first
						this->delayTimer = opdi_get_time_ms();
						this->setCurrentState(WAITING_BEFORE_DISABLE_ERROR);
					}
					else {
						// go to error condition directly
						this->setCurrentState(ERR);
					}
				} else {
					// assume open
					// enable delay specified?
					if (this->enableDelay > 0) {
						// need to disable first
						this->delayTimer = opdi_get_time_ms();
						this->setCurrentState(WAITING_BEFORE_DISABLE_OPEN);
					} else {
						// assume that the window is open now
						this->setCurrentState(OPEN);
					}
				}
			}
		} else
		// closing?
		if (this->currentState == CLOSING) {
			if (this->targetState == OPEN) {
				// need to open
				this->openTimer = opdi_get_time_ms();	// reset timer; TODO set to delta value?
				this->setMotorOpening();
				this->setCurrentState(OPENING);
			} else
			// sensor close detected?
			if (this->isSensorClosed()) {
				this->logVerbose("Closed sensor signal detected");

				// enable delay specified?
				if (this->enableDelay > 0) {
					// need to disable first
					this->delayTimer = opdi_get_time_ms();
					this->setCurrentState(WAITING_BEFORE_DISABLE_CLOSED);
				} else {
					// the window is closed now
					this->setCurrentState(CLOSED);
				}
			} else
			// closing time up?
			if (opdi_get_time_ms() - this->openTimer > this->closingTime) {

				// stop motor immediately
				this->setMotorOff();

				// without detecting sensor? error condition
				if (this->sensorClosedPort != nullptr) {
					this->logNormal("Warning: Closed sensor signal not detected while closing");

					// enable delay specified?
					if (this->enableDelay > 0) {
						// need to disable first
						this->delayTimer = opdi_get_time_ms();
						this->setCurrentState(WAITING_BEFORE_DISABLE_ERROR);
					}
					else {
						// go to error condition directly
						this->setCurrentState(ERR);
					}
				} else {
					// assume closed
					// enable delay specified?
					if (this->enableDelay > 0) {
						// need to disable first
						this->delayTimer = opdi_get_time_ms();
						this->setCurrentState(WAITING_BEFORE_DISABLE_CLOSED);
					} else {
						// go to closed condition directly
						this->setCurrentState(CLOSED);
					}
				}
			}
		}
	}	// end of H-Bridge Mode state machine

	else if (this->mode == SERIAL_RELAY) {
		// state machine for serial relay mode

		// unknown state (first call or off or transition not yet implemented)? initialize
		if (this->currentState == UNKNOWN) {
			// disable the motor
			this->delayTimer = opdi_get_time_ms();
			this->disableMotor();
			this->setCurrentState(WAITING_AFTER_DISABLE);

			// set target state according to selected position
			if (this->position == POSITION_OPEN) {
				this->setTargetState(OPEN);
			} else
			if (this->position == POSITION_CLOSED) {
				this->setTargetState(CLOSED);
			}
		} else
		// waiting after disable?
		if (this->currentState == WAITING_AFTER_DISABLE) {
			// waiting time up?
			if (opdi_get_time_ms() - this->delayTimer > this->enableDelay) {
				// disable delay is up
			
				// target state is unknown; wait
				this->setCurrentState(UNKNOWN_WAITING);
			}
		} else
		// unknown; waiting for command
		if ((this->currentState == UNKNOWN_WAITING)) {
			// check sensors
			if (this->isSensorClosed() && this->isSensorOpen()) {
				this->logNormal("Closed sensor signal and open sensor signal detected at the same time");
				this->setCurrentState(ERR);
			}
			else
			if (this->isSensorClosed()) {
				this->logVerbose("Closed sensor signal detected");
				this->setCurrentState(CLOSED);
			}
			else
			if (this->isSensorOpen()) {
				this->logVerbose("Open sensor signal detected");
				this->setCurrentState(OPEN);
			}
			else {
				// do not change current state

				// opening required?
				if (this->targetState == OPEN) {
					this->delayTimer = opdi_get_time_ms();
					this->setCurrentState(WAITING_BEFORE_ENABLE_OPENING);
				}

				// closing required?
				if (this->targetState == CLOSED) {
					this->delayTimer = opdi_get_time_ms();
					this->setCurrentState(WAITING_BEFORE_ENABLE_CLOSING);
				}
			}
		} else
		// open?
		if (this->currentState == OPEN) {
			// closing required?
			if (this->targetState == CLOSED) {
				// disable the motor
				this->delayTimer = opdi_get_time_ms();
				this->disableMotor();
				this->setCurrentState(WAITING_BEFORE_ENABLE_CLOSING);
			}
			// close sensor active? (error condition)
			if (this->isSensorClosed()) {
				this->logNormal("Warning: Closed sensor signal received while assuming state OPEN");
				// set error directly
				this->setCurrentState(ERR);
			}
		} else
		// closed?
		if (this->currentState == CLOSED) {
			// opening required?
			if (this->targetState == OPEN) {
				// disable the motor
				this->delayTimer = opdi_get_time_ms();
				this->disableMotor();
				this->setCurrentState(WAITING_BEFORE_ENABLE_OPENING);
			}
			// open sensor active? (error condition)
			if (this->isSensorOpen()) {
				this->logNormal("Warning: Open sensor signal received while assuming state CLOSED");
				// set error directly
				this->setCurrentState(ERR);
			}
		} else
		// waiting before enable before opening?
		if (this->currentState == WAITING_BEFORE_ENABLE_OPENING) {
			if (this->targetState == CLOSED) {
				// need to close
				this->delayTimer = opdi_get_time_ms();
				this->setCurrentState(WAITING_BEFORE_ENABLE_CLOSING);
			} else
			// waiting time up?
			if (opdi_get_time_ms() - this->delayTimer > this->enableDelay) {
				this->openTimer = opdi_get_time_ms();
				// set direction
				this->setMotorOpening();
				this->enableMotor();
			
				this->setCurrentState(OPENING);
			}
		} else
		// waiting before enable before closing?
		if (this->currentState == WAITING_BEFORE_ENABLE_CLOSING) {
			if (this->targetState == OPEN) {
				// need to open
				this->delayTimer = opdi_get_time_ms();
				this->setCurrentState(WAITING_BEFORE_ENABLE_OPENING);
			} else
			// waiting time up?
			if (opdi_get_time_ms() - this->delayTimer > this->enableDelay) {
				this->openTimer = opdi_get_time_ms();
				// set direction
				this->setMotorClosing();
				this->enableMotor();
			
				this->setCurrentState(CLOSING);
			}
		} else
		// opening?
		if (this->currentState == OPENING) {
			if (this->targetState == CLOSED) {
				// need to close
				this->delayTimer = opdi_get_time_ms();
				this->disableMotor();
				this->setCurrentState(WAITING_BEFORE_ENABLE_CLOSING);
			} else
			// sensor open detected?
			if (this->isSensorOpen()) {
				this->logVerbose("Open sensor signal detected");

				// enable delay specified?
				if (this->enableDelay > 0) {
					// need to disable first
					this->delayTimer = opdi_get_time_ms();
					this->setCurrentState(WAITING_BEFORE_DISABLE_OPEN);
				}
				else {
					// the window is open now
					this->setCurrentState(OPEN);
				}
			} else
			// opening time up?
			if (opdi_get_time_ms() - this->openTimer > this->openingTime) {
				// stop motor
				this->disableMotor();
				// without detecting sensor? error condition
				if (this->sensorOpenPort != nullptr) {
					this->logNormal("Warning: Open sensor signal not detected while opening");
					// go to error condition directly
					this->setCurrentState(ERR);
				} else {
					// sensor not present - assume open
					this->setCurrentState(OPEN);
				}
			}
		} else
		// closing?
		if (this->currentState == CLOSING) {
			if (this->targetState == OPEN) {
				// need to open
				this->delayTimer = opdi_get_time_ms();
				this->disableMotor();
				this->setCurrentState(WAITING_BEFORE_ENABLE_OPENING);
			} else
			// sensor close detected?
			if (this->isSensorClosed()) {
				this->logVerbose("Closed sensor signal detected");
				// stop motor immediately
				this->disableMotor();
				this->setCurrentState(CLOSED);
			}
			else
			// closing time up?
			if (opdi_get_time_ms() - this->openTimer > this->closingTime) {
				// stop motor immediately
				this->disableMotor();

				// without detecting sensor? error condition
				if (this->sensorClosedPort != nullptr) {
					this->logNormal("Warning: Closed sensor signal not detected while closing");

					this->setCurrentState(ERR);
				} else {
					// sensor not present - assume closed
					this->setCurrentState(CLOSED);
				}
			}
		}
	}

	opdi::DigitalPortList::const_iterator pi;
	bool forceOpen = false;
	bool forceClose = false;

	// if the window has detected an error, do not automatically open or close
	if (this->currentState != ERR) {
		// if one of the ForceOpen ports is High, the window must be opened
		auto pi = this->forceOpenPorts.begin();
		auto pie = this->forceOpenPorts.end();
		while (pi != pie) {
			if (this->getPortLine(*pi) == 1) {
				this->logExtreme("ForceOpen detected from port: " + (*pi)->ID());
				forceOpen = true;
				break;
			}
			++pi;
		}
		// if one of the ForceClose ports is High, the window must be closed
		pi = this->forceClosePorts.begin();
		pie = this->forceClosePorts.end();
		while (pi != pie) {
			if (this->getPortLine(*pi) == 1) {
				this->logExtreme("ForceClose detected from port: " + (*pi)->ID());
				forceClose = true;
				break;
			}
			++pi;
		}
	} else {
		// if one of the Reset ports is High, the window should be reset to the defined state
		pi = this->resetPorts.begin();
		auto pie = this->resetPorts.end();
		while (pi != pie) {
			if (this->getPortLine(*pi) == 1) {
				this->logExtreme("Reset detected from port: " + (*pi)->ID());
				this->setCurrentState(UNKNOWN);
				if (this->resetTo == RESET_TO_CLOSED)
					forceClose = true;
				else if (this->resetTo == RESET_TO_OPEN)
					forceOpen = true;
				else
					this->setPosition(POSITION_OFF);
				break;
			}
			++pi;
		}
	}

	if (forceOpen) {
		this->setTargetState(OPEN);
	} else if (forceClose) {
		this->setTargetState(CLOSED);
	} else {
		WindowState target = UNKNOWN;
		// if the window has detected an error, do not automatically open or close
		// if the position is set to automatic, evaluate auto ports
		if ((this->currentState != ERR) && (this->position >= POSITION_AUTO)) {
			// if one of the AutoClose ports is High, the window should be closed (takes precedence)
			pi = this->autoClosePorts.begin();
			auto pie = this->autoClosePorts.end();
			while (pi != pie) {
				if (this->getPortLine(*pi) == 1) {
					// avoid repeating messages
					if (this->targetState != CLOSED) {
						this->logExtreme("AutoClose detected from port: " + (*pi)->ID());
					}
					target = CLOSED;
					break;
				}
				++pi;
			}
			if (target == UNKNOWN) {
				// if one of the AutoOpen ports is High, the window should be opened
				pi = this->autoOpenPorts.begin();
				pie = this->autoOpenPorts.end();
				while (pi != pie) {
					if (this->getPortLine(*pi) == 1) {
						// avoid repeating messages
						if (this->targetState != OPEN) {
							this->logExtreme("AutoOpen detected from port: " + (*pi)->ID());
						}
						target = OPEN;
						break;
					}
					++pi;
				}
			}
		} else
		// check for position changes only - not if the previous setting is still active
		// this helps to recover from an error state - the user must manually change the position
		if (this->positionNewlySet && (this->position == POSITION_OFF)) {
			// setting the window "UNKNOWN" disables the motor and re-initializes
			this->setCurrentState(UNKNOWN);
		} else
		if (this->positionNewlySet && (this->position == POSITION_OPEN)) {
			target = OPEN;
		} else
		if (this->positionNewlySet && (this->position == POSITION_CLOSED)) {
			target = CLOSED;
		}
		this->positionNewlySet = false;

		if (target != UNKNOWN) {
			this->setTargetState(target);
			// resetting from error state
			if (this->currentState == ERR)
				this->setCurrentState(UNKNOWN_WAITING);
		}
	}

	return OPDI_STATUS_OK;
}


void WindowPlugin::setupPlugin(openhat::AbstractOpenHAT* openhat, const std::string& node, openhat::ConfigurationView* config, const std::string& /*driverPath*/) {
	this->openhat = openhat;

	Poco::AutoPtr<openhat::ConfigurationView> nodeConfig = this->openhat->createConfigView(config, node);
	// avoid check for unused plugin keys
	nodeConfig->addUsedKey("Type");
	nodeConfig->addUsedKey("Driver");

	// create window port
	WindowPort* port = new WindowPort(this->openhat, node.c_str());
	this->openhat->configureSelectPort(nodeConfig, config, port);

	port->logVerbosity = this->openhat->getConfigLogVerbosity(nodeConfig, opdi::LogVerbosity::UNKNOWN);

	if (nodeConfig->getInt("EnableDelay", 0) < 0)
		this->openhat->throwSettingsException("EnableDelay may not be negative: " + this->openhat->to_string(port->enableDelay));
	port->enableDelay = nodeConfig->getInt("EnableDelay", 0);

	// read control mode
	std::string controlMode = this->openhat->getConfigString(nodeConfig, node, "ControlMode", "", true);
	if (controlMode == "H-Bridge") {
		this->openhat->logVerbose(node + ": Configuring WindowPort in H-Bridge Mode");
		port->mode = WindowPort::H_BRIDGE;
		// motorA and motorB are required
		port->motorAStr = this->openhat->getConfigString(nodeConfig, node, "MotorA", "", true);
		port->motorBStr = this->openhat->getConfigString(nodeConfig, node, "MotorB", "", true);
		if (nodeConfig->getInt("MotorDelay", 0) < 0)
			this->openhat->throwSettingsException("MotorDelay may not be negative: " + this->openhat->to_string(port->motorDelay));
		port->motorDelay = nodeConfig->getInt("MotorDelay", 0);
		port->enable = nodeConfig->getString("Enable", "");
		if (port->enableDelay < port->motorDelay)
			this->openhat->throwSettingsException("If using MotorDelay, EnableDelay must be greater or equal: " + this->openhat->to_string(port->enableDelay));
	} else if (controlMode == "SerialRelay") {
		this->openhat->logVerbose(node + ": Configuring WindowPort in Serial Relay Mode");
		port->mode = WindowPort::SERIAL_RELAY;
		// direction and enable ports are required
		port->direction = this->openhat->getConfigString(nodeConfig, node, "Direction", "", true);
		port->enable = this->openhat->getConfigString(nodeConfig, node, "Enable", "", true);
	} else
		this->openhat->throwSettingsException("ControlMode setting not supported; expected 'H-Bridge' or 'SerialRelay'", controlMode);

	// legacy check
	if (!this->openhat->getConfigString(nodeConfig, node, "Sensor", "", false).empty())
		this->openhat->throwSettingsException("Setting 'Sensor' is deprecated, please use 'SensorClosed' instead");

	// read additional configuration parameters
	port->sensorClosedPortStr = this->openhat->getConfigString(nodeConfig, node, "SensorClosed", "", false);
	port->sensorClosedValue = (uint8_t)nodeConfig->getInt("SensorClosedValue", 1);
	if (port->sensorClosedValue > 1)
		this->openhat->throwSettingsException("SensorClosedValue must be either 0 or 1: " + this->openhat->to_string((int)port->sensorClosedValue));
	port->sensorOpenPortStr = this->openhat->getConfigString(nodeConfig, node, "SensorOpen", "", false);
	port->sensorOpenValue = (uint8_t)nodeConfig->getInt("SensorOpenValue", 1);
	if (port->sensorClosedValue > 1)
		this->openhat->throwSettingsException("SensorOpenValue must be either 0 or 1: " + this->openhat->to_string((int)port->sensorOpenValue));
	port->motorActive = (uint8_t)nodeConfig->getInt("MotorActive", 1);
	if (port->motorActive > 1)
		this->openhat->throwSettingsException("MotorActive must be either 0 or 1: " + this->openhat->to_string((int)port->motorActive));
	port->enableActive = (uint8_t)nodeConfig->getInt("EnableActive", 1);
	if (port->enableActive > 1)
		this->openhat->throwSettingsException("EnableActive must be either 0 or 1: " + this->openhat->to_string((int)port->enableActive));
	port->openingTime = nodeConfig->getInt("OpeningTime", 0);
	if (port->openingTime <= 0)
		this->openhat->throwSettingsException("OpeningTime must be specified and greater than 0: " + this->openhat->to_string(port->openingTime));
	port->closingTime = nodeConfig->getInt64("ClosingTime", port->openingTime);
	if (port->closingTime <= 0)
		this->openhat->throwSettingsException("ClosingTime must be greater than 0: " + this->openhat->to_string(port->closingTime));
	port->autoOpenStr = this->openhat->getConfigString(nodeConfig, node, "AutoOpen", "", false);
	port->autoCloseStr = this->openhat->getConfigString(nodeConfig, node, "AutoClose", "", false);
	port->forceOpenStr = this->openhat->getConfigString(nodeConfig, node, "ForceOpen", "", false);
	port->forceCloseStr = this->openhat->getConfigString(nodeConfig, node, "ForceClose", "", false);
	if ((port->forceOpenStr != "") && (port->forceCloseStr != ""))
		this->openhat->throwSettingsException("You cannot use ForceOpen and ForceClose at the same time");
	port->statusPortStr = this->openhat->getConfigString(nodeConfig, node, "StatusPort", "", false);
	port->errorPortStr = this->openhat->getConfigString(nodeConfig, node, "ErrorPorts", "", false);
	port->resetPortStr = this->openhat->getConfigString(nodeConfig, node, "ResetPorts", "", false);
	std::string resetTo = this->openhat->getConfigString(nodeConfig, node, "ResetTo", "Off", false);
	if (resetTo == "Closed") {
		port->resetTo = WindowPort::RESET_TO_CLOSED;
	} else if (resetTo == "Open") {
		port->resetTo = WindowPort::RESET_TO_OPEN;
	} else if (resetTo != "Off")
		this->openhat->throwSettingsException("Invalid value for the ResetTo setting; expected 'Off', 'Closed' or 'Open'", resetTo);
	port->positionAfterClose = nodeConfig->getInt("PositionAfterClose", -1);
	port->positionAfterOpen = nodeConfig->getInt("PositionAfterOpen", -1);

	this->openhat->addPort(port);

	this->openhat->addConnectionListener(this);

	this->openhat->logVerbose(node + ": WindowPlugin setup completed successfully");
}

void WindowPlugin::masterConnected() {
}

void WindowPlugin::masterDisconnected() {
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
	return new WindowPlugin();
}
