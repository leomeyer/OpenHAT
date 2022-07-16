#pragma once

// need to guard against security check warnings
#define _SCL_SECURE_NO_WARNINGS	1

#include <cstdio>

#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/Timestamp.h"

#include "AbstractOpenHAT.h"

// expression evaluation library
// avoid excessive g++ 7 (and newer) compile warnings
#ifdef __GNUC__
#if __GNUC__ >= 7
#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="
#endif
#endif

#include <exprtk.hpp>

#ifdef __GNUC__
#if __GNUC__ >= 7
#pragma GCC diagnostic pop
#endif
#endif

namespace openhat {

///////////////////////////////////////////////////////////////////////////////
// Expression Port
///////////////////////////////////////////////////////////////////////////////

/** An ExpressionPort is a DigitalPort that sets the value of other ports
*   depending on the result of a calculation expression.
*   Expression syntax documentation: http://partow.net/programming/exprtk/index.html
*   The expression is evaluated in each doWork iteration if the ExpressionPort is enabled,
*   i. e. its digital state is High (default).
*   The expression can refer to port IDs (input variables). Although these port IDs are case-
*   insensitive (a restriction of the underlying library), it is recommended to use the correct case.
*   The rules for the different port types are:
*    - A digital port is evaluated as 1 or 0 (High or Low).
*    - An analog port's relative value is evaluated in the range 0..1.
*    - A dial port's value is evaluated to its 64-bit value.
*    - A select port's value is its current item position.
*   The expression is evaluated using the ExprTk library. It queries the current state
*   of all ports to make their values available to the expression formula.
*   The resulting value can be assigned to a number of output ports.
*   If a port state cannot be queried (e. g. due to an exception) the expression is not evaluated,
*   however, you can specify a fallback value that is assigned to the output ports.
*   The rules for the different port types are:
*    - A digital port is set to Low if the value is 0 and to High otherwise.
*    - An analog port's relative value is set to the range 0..1.
*      If the value is < 0, it is assumed as 0. If the value is > 1, it is assumed as 1.
*    - A dial port's value is set by casting the value to a 64-bit signed integer.
*    - A select port's position is set by casting the value to a 16-bit unsigned int.
*   An expression port continuously evaluates its expression and sets the value of the output ports
*   accordingly. Sometimes it is useful to limit the number of evaluations such that the value is
*   calculated e. g. only once. The ExpressionPort supports an iterations counter which will be decreased
*   each time the value is evaluated. If the iterations counter reaches 0 the ExpressionPort is
*   disabled, i. e. set to 0. The counter starts running again when the ExpressionPort is again set
*   to High.
*	The expression port can set the output ports to an optional deactivation value when it becomes
*   deactivated.
*   The ExpressionPort supports the following custom functions:
*    - timestamp(): Returns the number of seconds since 1/1/1970 00:00 UTC.
*/
#ifdef OPENHAT_USE_EXPRTK

/// ExprTk function extensions
struct timestamp_func : public exprtk::ifunction<double>
{
	timestamp_func()
	: exprtk::ifunction<double>(0)
	{}

	double operator()()
	{
		// return epoch time since midnight, 1 January 1970, in seconds
		return (double)Poco::Timestamp().epochTime();
	}
};

class ExpressionPort : public opdi::DigitalPort {
protected:
	openhat::AbstractOpenHAT* openhat;

	std::vector<double> portValues;	// holds the numeric values of the ports for the expression evaluation
	std::vector<std::string> portStrings;	// holds the string values of the ports for the expression evaluation

	int64_t numIterations;
	double fallbackValue;
	bool fallbackSpecified;
	double deactivationValue;
	bool deactivationSpecified;
        double currentFrame;

	timestamp_func timestampFunc;

	typedef exprtk::symbol_table<double> symbol_table_t;
	typedef exprtk::expression<double> expression_t;
	typedef exprtk::parser<double> parser_t;
	typedef parser_t::dependent_entity_collector::symbol_t symbol_t;
	std::deque<symbol_t> symbol_list;

	symbol_table_t symbol_table;
	expression_t expression;
	int64_t iterations;

	virtual bool prepareSymbols(bool duringSetup);

	virtual bool prepareVariables(bool duringSetup);

	virtual uint8_t doWork(uint8_t canSend);

	void setOutputPorts(double value);

public:
	std::string expressionStr;
	std::string outputPortStr;
	opdi::PortList outputPorts;

	ExpressionPort(AbstractOpenHAT* openhat, const char* id);

	virtual ~ExpressionPort();

	virtual void configure(ConfigurationView::Ptr config);

	virtual bool setLine(uint8_t line, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;

	virtual void prepare() override;
        
    virtual double apply(void);
};

#endif // def OPENHAT_USE_EXPRTK

}		// namespace openhat
