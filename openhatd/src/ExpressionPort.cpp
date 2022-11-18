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
	this->setLine(1);
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

bool ExpressionPort::setLine(uint8_t line, ChangeSource changeSource) {
	bool changed = opdi::DigitalPort::setLine(line, changeSource);

	if (!changed)
		return false;

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
	return true;
}

bool ExpressionPort::prepareSymbols(symbol_table_t& symTab, bool /*duringSetup*/) {
	symTab.add_function("timestamp", this->timestampFunc);

	// Adding constants leads to a memory leak; furthermore, constants are not detected as known symbols.
	// As there are only three constants (pi, epsilon, infinity) we can well do without this function.
	//	this->symbol_table.add_constants();

	return true;
}

bool ExpressionPort::prepareVariables(bool duringSetup) {
	// go through dependent entities (variables) of the expression
	for (std::size_t i = 0; i < this->symbol_list.size(); ++i)
	{
		symbol_t& symbol = this->symbol_list[i];

		if (symbol.second != parser_t::e_st_variable)
			continue;

		// find port (variable name is the port ID)
		opdi::Port* port = this->openhat->findPortByID(symbol.first.c_str(), true);

		// port not found?
		if (port == nullptr) {
			throw PortError(this->ID() + ": Expression variable did not resolve to an available port ID: " + symbol.first);
		}

		// custom port?
		if (port->getType()[0] == OPDI_PORTTYPE_CUSTOM[0])
			throw PortError(this->ID() + ": Cannot use a custom port in an expression: " + symbol.first);
		else
		// streaming port?
		if (port->getType()[0] == OPDI_PORTTYPE_STREAMING[0])
			throw PortError(this->ID() + ": Cannot use a streaming port in an expression: " + symbol.first);
		else {
			// numeric port value
			// add reference to the port value (by port ID)
			if (!this->symbol_table.add_variable(port->ID(), port->getValuePtr()))
				return false;
		}
	}

	return true;
}

void ExpressionPort::prepare() {
	opdi::DigitalPort::prepare();

	// find ports; throws errors if something required is missing
	this->findPorts(this->getID(), "OutputPorts", this->outputPortStr, this->outputPorts);

	symbol_table_t localSymtab;
	expression_t localExpr;
	localExpr.register_symbol_table(localSymtab);
	this->prepareSymbols(localSymtab, true);

	parser_t parser;
	parser.enable_unknown_symbol_resolver();
	// collect only variables as symbol names
	parser.dec().collect_variables() = true;

	// compile to detect variables
	if (!parser.compile(this->expressionStr, localExpr))
		throw Poco::Exception(this->ID() + ": Error in expression: " + parser.error());

	// store symbol list (input variables)
	parser.dec().symbols(this->symbol_list);

	// prepare actual symbol table
	this->expression.register_symbol_table(symbol_table);
	this->prepareSymbols(this->symbol_table, true);
	if (!this->prepareVariables(true)) {
		throw Poco::Exception(this->ID() + ": Unable to resolve variables");
	}

	parser.disable_unknown_symbol_resolver();
	parser.dec().collect_variables() = false;
	if (!parser.compile(this->expressionStr, this->expression))
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

double ExpressionPort::apply() {
/*
	// clear symbol table and values
	this->symbol_table.clear();

	this->prepareSymbols(false);

	// prepareVariables will return false in case of errors
	if (this->prepareVariables(false)) {
*/
		double value = expression.value();

		this->logExtreme("Expression result: " + to_string(value));

		this->setOutputPorts(value);

		return value;
/*
	}
	else {
		// the variables could not be prepared, due to some error

		// fallback value specified?
		if (this->fallbackSpecified) {
			double value = this->fallbackValue;

			this->logExtreme("An error occurred, applying fallback value of: " + to_string(value));

			this->setOutputPorts(value);

			return value;
		}
	}

	return NAN;
*/
}

uint8_t ExpressionPort::doWork(uint8_t canSend)  {
	opdi::DigitalPort::doWork(canSend);

	if (this->getLine() == 1) {
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
