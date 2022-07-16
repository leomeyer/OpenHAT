#pragma once

#include "Poco/Process.h"
#include "Poco/Thread.h"
#include "Poco/RunnableAdapter.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/PipeStream.h"
#include "Poco/StreamCopier.h"

#include <memory>

#include "AbstractOpenHAT.h"

// helper functions because make_unique is not a part of C++11
#include <type_traits>
#include <utility>

template <typename T, typename... Args>
std::unique_ptr<T> make_unique_helper(std::false_type, Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <typename T, typename... Args>
std::unique_ptr<T> make_unique_helper(std::true_type, Args&&... args) {
   static_assert(std::extent<T>::value == 0,
       "make_unique<T[N]>() is forbidden, please use make_unique<T[]>().");

   typedef typename std::remove_extent<T>::type U;
   return std::unique_ptr<T>(new U[sizeof...(Args)]{std::forward<Args>(args)...});
}

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
   return make_unique_helper<T>(std::is_array<T>(), std::forward<Args>(args)...);
}

namespace openhat {

///////////////////////////////////////////////////////////////////////////////
// Exec Port
///////////////////////////////////////////////////////////////////////////////

/** An ExecPort is a DigitalPort that starts an operating system process when its state is changed.
*   It has a certain WaitTime (in milliseconds) to avoid starting the process too often.
*   If set High and a ResetTime is specified (in milliseconds) the port will automatically reset to Low
*   after this time which may again trigger the process execution.
*   The process is started with the supplied parameters. The parameter string can contain
*   placeholders which are the names of ports; the syntax is $<port_id>. This expression will be
*   substituted with the value of the port when the process is being started. A DigitalPort
*   will be evaluated to 0 or 1. An AnalogPort will be evaluated as its relative value. A
*   DialPort will be evaluated as its absolute value, as will the position of a SelectPort.
*   If an error occurs during port evaluation the value will be represented as "<error>".
*   A port ID that cannot be found will not be substituted.
*   The special value $ALL_PORTS in the parameter string will be replaced by a space-separated
*   list of <port_id>=<value> specifiers.
*   If the parameter ForceKill is true a running process is killed if it is still running when the
*   same process is to be started again.
*/
class ExecPort : public opdi::DigitalPort {

protected:
	openhat::AbstractOpenHAT* openhat;
	std::string programName;
	std::string parameters;
	int64_t killTimeMs;
	std::string exitCodePortStr;
	opdi::DialPort* exitCodePort;

	bool startRequested;
	bool hasTerminated;
	uint64_t lastStartTime;
	Poco::ProcessHandle* processHandle;
	Poco::Process::PID processPID;
	Poco::Thread waitThread;
	Poco::RunnableAdapter<ExecPort> waiter;
	uint32_t exitCode;
	Poco::Pipe* inPipe;
	Poco::Pipe* outPipe;
	Poco::Pipe* errPipe;
	std::unique_ptr<Poco::PipeInputStream> outStr;
	std::unique_ptr<Poco::PipeInputStream> errStr;

	void waitForProcessEnd();

	virtual uint8_t doWork(uint8_t canSend);

public:
	ExecPort(AbstractOpenHAT* openhat, const char* id);

	virtual ~ExecPort();

	virtual void configure(ConfigurationView::Ptr config);

	virtual void prepare() override;

	/// This method ensures that the Line of an Exec port cannot be set to Low if a process is running.
	///
	virtual bool setLine(uint8_t line, ChangeSource changeSource = opdi::Port::ChangeSource::CHANGESOURCE_INT) override;
};

}		// namespace openhat
