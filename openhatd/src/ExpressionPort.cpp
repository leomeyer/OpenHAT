#include "ExpressionPort.h"

#include "Poco/Timestamp.h"

#ifdef OPENHAT_USE_EXPRTK

namespace openhat {

///////////////////////////////////////////////////////////////////////////////
// Expression Port
///////////////////////////////////////////////////////////////////////////////

ExpressionPort::ExpressionPort(AbstractOpenHAT* openhat, const char* id) : opdi::DigitalPort(id, OPDI_PORTDIRCAP_OUTPUT, 0) {
	this->opdi = this->openhat = openhat;
	this->numIterations = 0;
	this->fallbackSpecified = false;
	this->fallbackValue = 0;
	this->deactivationSpecified = false;
	this->deactivationValue = 0;

	opdi::DigitalPort::setMode(OPDI_DIGITAL_MODE_OUTPUT);

	// default: enabled
	this->line = 1;
}

ExpressionPort::~ExpressionPort() {
}

void ExpressionPort::configure(ConfigurationView::Ptr config) {
	this->openhat->configurePort(config, this, 0);
	this->logVerbosity = this->openhat->getConfigLogVerbosity(config, opdi::LogVerbosity::UNKNOWN);

	this->expressionStr = config->getString("Expression", "");
	if (this->expressionStr == "")
		this->openhat->throwSettingException(this->ID() + ": You have to specify an Expression");

	this->outputPortStr = config->getString("OutputPorts", "");
	if (this->outputPortStr == "")
		this->openhat->throwSettingException(this->ID() + ": You have to specify at least one output port in the OutputPorts setting");

	this->numIterations = config->getInt64("Iterations", 0);

	if (config->hasProperty("FallbackValue")) {
		this->fallbackValue = config->getDouble("FallbackValue");
		this->fallbackSpecified = true;
	}

	if (config->hasProperty("DeactivationValue")) {
		this->deactivationValue = config->getDouble("DeactivationValue");
		this->deactivationSpecified = true;
	}
}

void ExpressionPort::setLine(uint8_t line, ChangeSource changeSource) {
	opdi::DigitalPort::setLine(line, changeSource);

	// if the line has been set to High, start the iterations
	if (line == 1) {
		this->logDebug("Expression activated, number of iterations: " + this->to_string(this->numIterations));
		this->iterations = this->numIterations;
	}
	// set to 0; check whether to set a deactivation value
	else {
		this->logDebug("Expression deactivated");
		if (this->deactivationSpecified) {
			this->logDebug("Applying deactivation value: " + this->to_string(this->deactivationValue));
			this->setOutputPorts(this->deactivationValue);
		}
	}
}

bool ExpressionPort::prepareSymbols(bool /*duringSetup*/) {
	this->symbol_table.add_function("timestamp", this->timestampFunc);

	// Adding constants leads to a memory leak; furthermore, constants are not detected as known symbols.
	// As there are only three constants (pi, epsilon, infinity) we can well do without this function.
	//	this->symbol_table.add_constants();

	return true;
}

bool ExpressionPort::prepareVariables(bool duringSetup) {
	this->portValues.clear();
	// assume most port values are numeric
	this->portValues.reserve(this->symbol_list.size());
	this->portStrings.clear();

	// go through dependent entities (variables) of the expression
	for (std::size_t i = 0; i < this->symbol_list.size(); ++i)
	{
		symbol_t& symbol = this->symbol_list[i];

		if (symbol.second != parser_t::e_st_variable)
			continue;
		
		if (symbol.first == "currentframe") {
			this->currentFrame = this->openhat->getCurrentFrame();
			if (!this->symbol_table.add_variable("currentframe", this->currentFrame))
				return false;			
			continue;
		}

		// find port (variable name is the port ID)
		opdi::Port* port = this->openhat->findPortByID(symbol.first.c_str(), true);

		// port not found?
		if (port == nullptr) {
			throw PortError(this->ID() + ": Expression variable did not resolve to an available port ID: " + symbol.first);
		}

		// custom port?
		if (port->getType()[0] == OPDI_PORTTYPE_CUSTOM[0]) {
			// custom port: set text variable
			try {
				std::string value = ((opdi::CustomPort*)port)->getValue();
				this->logExtreme("Resolved value of port " + port->ID() + " to: '" + value + "'");
				this->portStrings.push_back(value);
			} catch (Poco::Exception& e) {
				// error handling during setup is different; to avoid too many warnings (in the doWork method)
				// we make a difference here
				if (duringSetup) {
					// emit a warning
					this->logWarning("Failed to resolve value of port " + port->ID() + ": " + this->openhat->getExceptionMessage(e));
					this->portStrings.push_back("");
				} else {
					// warn in extreme logging mode only
					this->logExtreme("Failed to resolve value of port " + port->ID() + ": " + this->openhat->getExceptionMessage(e));
					// the expression cannot be evaluated if there is an error
					return false;
				}
			}

			// add reference to the port value (by port ID)
			if (!this->symbol_table.add_stringvar(port->ID(), this->portStrings[this->portStrings.size() - 1]))
				return false;
		} else {
			// numeric port value
			try {
				double value = openhat->getPortValue(port);
				this->logExtreme("Resolved value of port " + port->ID() + " to: " + to_string(value));
				this->portValues.push_back(value);
			} catch (Poco::Exception& e) {
				// error handling during setup is different; to avoid too many warnings (in the doWork method)
				// we make a difference here
				if (duringSetup) {
					// emit a warning
					this->logWarning("Failed to resolve value of port " + port->ID() + ": " + this->openhat->getExceptionMessage(e));
					this->portValues.push_back(0.0f);
				} else {
					// warn in extreme logging mode only
					this->logExtreme("Failed to resolve value of port " + port->ID() + ": " + this->openhat->getExceptionMessage(e));
					// the expression cannot be evaluated if there is an error
					return false;
				}
			}

			// add reference to the port value (by port ID)
			if (!this->symbol_table.add_variable(port->ID(), this->portValues[this->portValues.size() - 1]))
				return false;
		}
	}

	return true;
}

void ExpressionPort::prepare() {
	opdi::DigitalPort::prepare();

	// find ports; throws errors if something required is missing
	this->findPorts(this->getID(), "OutputPorts", this->outputPortStr, this->outputPorts);

	// clear symbol table and values
	this->symbol_table.clear();

	expression.register_symbol_table(symbol_table);
	this->prepareSymbols(true);

	parser_t parser;
	parser.enable_unknown_symbol_resolver();
	// collect only variables as symbol names
	parser.dec().collect_variables() = true;

	// compile to detect variables
	if (!parser.compile(this->expressionStr, expression))
		throw Poco::Exception(this->ID() + ": Error in expression: " + parser.error());

	// store symbol list (input variables)
	parser.dec().symbols(this->symbol_list);

	this->symbol_table.clear();
	this->prepareSymbols(true);
	if (!this->prepareVariables(true)) {
		throw Poco::Exception(this->ID() + ": Unable to resolve variables");
	}
	parser.disable_unknown_symbol_resolver();

	if (!parser.compile(this->expressionStr, expression))
		throw Poco::Exception(this->ID() + ": Error in expression: " + parser.error());

	// initialize the number of iterations
	// if the port is Low this has no effect; if it is High it will disable after the evaluation
	this->iterations = this->numIterations;
}

void ExpressionPort::setOutputPorts(double value) {
	// go through list of output ports
	auto it = this->outputPorts.begin();
	auto ite = this->outputPorts.end();
	while (it != ite) {
		try {
			if ((*it)->getType()[0] == OPDI_PORTTYPE_DIGITAL[0]) {
				if (value == 0)
					((opdi::DigitalPort*)(*it))->setLine(0);
				else
					((opdi::DigitalPort*)(*it))->setLine(1);
			}
			else
			if ((*it)->getType()[0] == OPDI_PORTTYPE_ANALOG[0]) {
				// analog port: relative value (0..1)
				((opdi::AnalogPort*)(*it))->setRelativeValue(value);
			}
			else
			if ((*it)->getType()[0] == OPDI_PORTTYPE_DIAL[0]) {
				// dial port: absolute value
				((opdi::DialPort*)(*it))->setPosition((int64_t)value);
			}
			else
			if ((*it)->getType()[0] == OPDI_PORTTYPE_SELECT[0]) {
				// select port: current position number
				((opdi::SelectPort*)(*it))->setPosition((uint16_t)value);
			}
			else
			if ((*it)->getType()[0] == OPDI_PORTTYPE_CUSTOM[0]) {
				// custom port: set text value
				((opdi::CustomPort*)(*it))->setValue(this->openhat->to_string(value));
			}
			else
				throw PortError("");
		}
		catch (Poco::Exception &e) {
			this->logNormal("Error setting output port value of port " + (*it)->ID() + ": " + this->openhat->getExceptionMessage(e));
		}

		++it;
	}
}

void ExpressionPort::apply() {
	// clear symbol table and values
	this->symbol_table.clear();

	this->prepareSymbols(false);

	// prepareVariables will return false in case of errors
	if (this->prepareVariables(false)) {

		double value = expression.value();

		this->logExtreme("Expression result: " + to_string(value));

		this->setOutputPorts(value);
	}
	else {
		// the variables could not be prepared, due to some error

		// fallback value specified?
		if (this->fallbackSpecified) {
			double value = this->fallbackValue;

			this->logExtreme("An error occurred, applying fallback value of: " + to_string(value));

			this->setOutputPorts(value);
		}
	}
}

uint8_t ExpressionPort::doWork(uint8_t canSend)  {
	opdi::DigitalPort::doWork(canSend);

	if (this->line == 1) {
		this->apply();
		
		// maximum number of iterations specified and reached?
		if ((this->numIterations > 0) && (--this->iterations <= 0)) {
			// disable this expression
			this->setLine(0);
		}
	}

	return OPDI_STATUS_OK;
}

}		// namespace openhat

#endif
