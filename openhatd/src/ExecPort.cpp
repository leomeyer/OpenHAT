#include "ExecPort.h"

namespace openhat {

///////////////////////////////////////////////////////////////////////////////
// Exec Port
///////////////////////////////////////////////////////////////////////////////

ExecPort::ExecPort(AbstractOpenHAT* openhat, const char* id) : opdi::DigitalPort(id, OPDI_PORTDIRCAP_OUTPUT, 0), waiter(*this, &ExecPort::waitForProcessEnd) {
	this->opdi = this->openhat = openhat;

	opdi::DigitalPort::setMode(OPDI_DIGITAL_MODE_OUTPUT);

	this->exitCodePort = nullptr;

	// default: Low
	this->line = 0;
	this->processPID = 0;
	this->killTimeMs = 0;		// kill time disabled
	this->startRequested = false;
	this->hasTerminated = false;
	this->lastStartTime = 0;
}

ExecPort::~ExecPort() {
}

void ExecPort::configure(ConfigurationView* config) {
	this->openhat->configurePort(config, this, 0);
	this->logVerbosity = this->openhat->getConfigLogVerbosity(config, opdi::LogVerbosity::UNKNOWN);

	this->programName = this->openhat->resolveRelativePath(config, this->ID(), config->getString("Program", ""), "Config");
	if (this->programName == "")
		this->openhat->throwSettingException(this->ID() + ": You have to specify a Program parameter");

	this->parameters = config->getString("Parameters", "");

	this->exitCodePortStr = config->getString("ExitCodePort", "");

	this->killTimeMs = config->getInt64("KillTime", this->killTimeMs);
	if (this->killTimeMs < 0)
		this->openhat->throwSettingException(this->ID() + ": Please specify a positive value for KillTime: ", this->to_string(this->killTimeMs));
}

void ExecPort::prepare() {
	this->logDebug("Preparing port");
	opdi::DigitalPort::prepare();

	this->exitCodePort = this->findDialPort(this->ID(), "ExitCodePort", this->exitCodePortStr, false);
}

void ExecPort::setLine(uint8_t line, ChangeSource changeSource) {
	// the line cannot be set to 0 while the process is still running
	if ((line == 0) && (this->processPID != 0) && Poco::Process::isRunning(*this->processHandle))
		return;

	// switch from Low to High?
	if ((this->line == 0) && (line == 1))
		// set flag for doWork
		this->startRequested = true;

	opdi::DigitalPort::setLine(line, changeSource);
}

void ExecPort::waitForProcessEnd() {
	while (Poco::Process::isRunning(*this->processHandle)) {
		// read data from the stream
		std::string data;
		Poco::StreamCopier::copyToString(*errStr, data);
		if (!data.empty()) {
			std::stringstream ss(data);
			std::string item;
			while (std::getline(ss, item, '\n')) {
				if (!item.empty())
					this->logNormal("stderr: " + item);
			}
		}
		data = {};
		// read data from the stream
		Poco::StreamCopier::copyToString(*outStr, data);
		if (!data.empty()) {
			std::stringstream ss(data);
			std::string item;
			while (std::getline(ss, item, '\n')) { 
				if (!item.empty())
					this->logVerbose("stdout: " + item);
			}
		}
		Poco::Thread::sleep(1);
	}
	this->exitCode = this->processHandle->wait();
	this->hasTerminated = true;
	outStr->close();
	errStr->close();
	free(this->inPipe);
	free(this->outPipe);
	free(this->errPipe);
}

uint8_t ExecPort::doWork(uint8_t canSend)  {
	opdi::DigitalPort::doWork(canSend);

	// process terminated?
	if (this->hasTerminated) {
		this->hasTerminated = false;
		this->logVerbose("Process with PID " + this->to_string(this->processPID) + " has terminated with exit code " + this->to_string(this->exitCode));
		this->processPID = 0;
		// update exitCodePort if specified
		if (this->exitCodePort != nullptr)
			this->exitCodePort->setPosition(this->exitCode);
		this->setLine(0);
	}

	// process still running?
	if (this->processPID != 0) {
		if (Poco::Process::isRunning(this->processPID)) {
			uint64_t timeDiff = opdi_get_time_ms() - this->lastStartTime;
			// kill time up?
			if ((this->killTimeMs > 0) && (timeDiff > this->killTimeMs)) {
				this->logVerbose("Kill time exceeded: Trying to kill previously started process with PID " + this->to_string(this->processPID));

				// kill process
				Poco::Process::kill(this->processPID);
			}
		}
	} else {
		// process is not running
		if (this->startRequested) {
			this->startRequested = false;

			// build parameter string
			std::string params(this->parameters);

			typedef std::map<std::string, std::string> PortValues;
			PortValues portValues;
			std::string allPorts;
			// go through all ports
			opdi::PortList pl = this->openhat->getPorts();
			auto pli = pl.begin();
			auto plie = pl.end();
			while (pli != plie) {
				std::string val = this->openhat->getPortStateStr(*pli);
				if (val.empty())
					val = "<error>";
				// store value
				portValues[std::string("$") + (*pli)->ID()] = val;
				allPorts += (*pli)->ID() + "=" + val + " ";
				++pli;
			}
			portValues["$ALL_PORTS"] = allPorts;

			// replace parameters in content
			for (auto iterator = portValues.begin(), iteratorEnd = portValues.end(); iterator != iteratorEnd; ++iterator) {
				std::string key = iterator->first;
				std::string value = iterator->second;

				size_t start = 0;
				while ((start = params.find(key, start)) != std::string::npos) {
					params.replace(start, key.length(), value);
				}
			}

			this->logDebug("Preparing start of program '" + this->programName + "'");
			this->logDebug("Parameters: " + params);

			// split parameters
			std::vector<std::string> argList;
			std::stringstream ss(params);
			std::string item;
			while (std::getline(ss, item, ' ')) {
				if (!item.empty())
					argList.push_back(item);
			}

			// execute program
			try {
				// start process, register pipes for IO redirection
				this->inPipe = new Poco::Pipe;
				this->outPipe = new Poco::Pipe;
				this->errPipe = new Poco::Pipe;
				this->processHandle = new Poco::ProcessHandle(Poco::Process::launch(this->programName, argList, this->inPipe, this->outPipe, this->errPipe));
				this->processPID = this->processHandle->id();
				this->outStr = std::make_unique<Poco::PipeInputStream>(*outPipe);
				this->errStr = std::make_unique<Poco::PipeInputStream>(*errPipe);

				this->logVerbose("Started program '" + this->programName + "' with PID " + this->to_string(this->processPID));

				// create a thread that waits for the process to terminate
				// otherwise, on Linux, child processes will remain defunct
				this->waitThread.start(waiter);

				this->lastStartTime = opdi_get_time_ms();
			}
			catch (Poco::Exception &e) {
				this->logNormal("ERROR: Unable to start program '" + this->programName + "': " + this->openhat->getExceptionMessage(e));
			}
		}		// switched to High
	}
	return OPDI_STATUS_OK;
}

}		// namespace openhat

