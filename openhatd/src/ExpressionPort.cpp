#include "ExpressionPort.h"

#include "Poco/Timestamp.h"
#include "Poco/String.h"

using Poco::endsWith;

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

	this->requiredPortStr = config->getString("RequiredPorts", "");

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
	std::string validationMarker("._");	// special marker for ports that need to be validated

	// go through dependent entities (variables) of the expression
	for (std::size_t i = 0; i < this->symbol_list.size(); ++i)
	{
		symbol_t& symbol = this->symbol_list[i];

		if (symbol.second != parser_t::e_st_variable)
			continue;

		std::string portname = symbol.first;
		bool needsValidation = endsWith(portname, validationMarker);
		if (needsValidation)
			portname = portname.substr(0, portname.size() - 2);

		// find port (variable name is the port ID)
		opdi::Port* port = this->openhat->findPortByID(portname.c_str(), true);

		// port not found?
		if (port == nullptr) {
			throw PortError(this->ID() + ": Expression variable '" + symbol.first + "' did not resolve to an available port with ID '" + portname + "'");
		}

		// custom port?
		if (port->getType()[0] == OPDI_PORTTYPE_CUSTOM[0])
			throw PortError(this->ID() + ": Cannot use a custom port in an expression: " + portname);
		else
		// streaming port?
		if (port->getType()[0] == OPDI_PORTTYPE_STREAMING[0])
			throw PortError(this->ID() + ": Cannot use a streaming port in an expression: " + portname);
		else {
			// numeric port value
			// add reference to the port value (by symbol name)
			if (!this->symbol_table.add_variable(symbol.first, port->getValuePtr()))
				return false;
			if (needsValidation)
				this->validationPorts.push_back(port);
		}
	}

	return true;
}

void ExpressionPort::prepare() {
	opdi::DigitalPort::prepare();

	// find ports; throws errors if something required is missing
	this->findPorts(this->getID(), "OutputPorts", this->outputPortStr, this->outputPorts);
	this->findPorts(this->getID(), "RequiredPorts", this->requiredPortStr, this->requiredPorts);

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
			if (std::isnan(value)) {
				(*it)->setError(Error::VALUE_NOT_AVAILABLE);
			}
			else
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
				throw PortError("Unsupported port type");
		}
		catch (Poco::Exception &e) {
			this->logNormal("Error setting output port value of port " + (*it)->ID() + ": " + this->openhat->getExceptionMessage(e));
		}

		++it;
	}
}

double ExpressionPort::apply() {
	bool ok = true;
	// check required ports for errors
	for (auto it = this->requiredPorts.begin(); it != this->requiredPorts.end(); it++)
	{
		if ((*it)->getError() != Error::VALUE_OK) {
			ok = false;
			break;
		}
	}

	if (ok && this->validationPorts.size() > 0) {
		ok = false;
		// check validation ports for errors; at least one of them must be ok
		for (auto it = this->validationPorts.begin(); it != this->validationPorts.end(); it++)
		{
			if ((*it)->getError() == Error::VALUE_OK) {
				ok = true;
				break;
			}
		}
	}

	double value = std::numeric_limits<double>::quiet_NaN();

	if (ok) {
		value = expression.value();

		this->logExtreme("Expression result: " + to_string(value));
	}
	else {
		if (this->fallbackSpecified) {
			this->logExtreme("No port variables with valid values detected, using fallback value");

			value = this->fallbackValue;
		} else
			this->logExtreme("No port variables with valid values detected");
	}

	this->setOutputPorts(value);

	return value;
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
