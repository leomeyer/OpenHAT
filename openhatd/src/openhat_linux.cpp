#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <iostream>
#include <cstdio>
#include <string>
#include <vector>

#include "opdi_constants.h"

#include "LinuxOpenHAT.h"

// the main OPDI instance is declared here
openhat::AbstractOpenHAT* Opdi;

void signal_handler(int) {
	// tell the OPDI system to shut down
	if (Opdi != NULL) {
    	std::cout << "Interrupted, exiting" << std::endl;
		Opdi->shutdown(0);
    }
}

void signal_handler_term(int) {
	// tell the OPDI system to shut down
	if (Opdi != NULL) {
    	std::cout << "Terminated, exiting" << std::endl;
		Opdi->shutdown(0);
    }
}

void signal_handler_abrt(int signum) {
	const int BACKTRACE_LEVELS = 10;
	void* array[BACKTRACE_LEVELS];
	size_t size;

	// retrieve backtrace information
	size = backtrace(array, BACKTRACE_LEVELS);

	std::cout << "Received ABRT signal, generating stack trace" << std::endl;
	backtrace_symbols_fd(array, size, STDERR_FILENO);

	// generate core file
	signal(signum, SIG_DFL);
	kill(getpid(), signum);
}

int main(int argc, char* argv[], char* envp[])
{
	Opdi = new openhat::LinuxOpenHAT();
	Opdi->appName = "openhatd";
	
	// install Ctrl+C intercept handler
	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = signal_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

	sigIntHandler.sa_handler = signal_handler_term;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGTERM, &sigIntHandler, NULL);

	sigIntHandler.sa_handler = signal_handler_abrt;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGABRT, &sigIntHandler, NULL);

	// convert arguments to vector list
	std::vector<std::string> args;
	args.reserve(argc);
	for (int i = 0; i < argc; i++) {
		args.push_back(argv[i]);
	}

	// create a map of environment parameters (important: uppercase, prefix $)
	std::map<std::string, std::string> environment;
	size_t counter = 0;
	char* env = envp[counter];
	while (env) {
		std::string envStr(env);
		char* envVar = strtok(&envStr[0], "=");
		if (envVar != NULL) {
			char* envValue = strtok(NULL, "");
			if (envValue != NULL) {
				std::string envKey(envVar);
				// convert key to uppercase
				std::transform(envKey.begin(), envKey.end(), envKey.begin(), toupper);
				environment[std::string("$") + envKey] = std::string(envValue);
			}
		}
		counter++;
		env = envp[counter];
	}

	int exitcode;
	try
	{
		exitcode = Opdi->startup(args, environment);
	}
	catch (Poco::Exception& e) {
		Opdi->logError(e.displayText());
		// signal error
		exitcode = OPDI_DEVICE_ERROR;
	}
	catch (std::exception& e) {
		Opdi->logError(e.what());
		exitcode = OPDI_DEVICE_ERROR;
	}
	catch (...) {
		Opdi->logError("An unknown error occurred. Exiting.");
		exitcode = OPDI_DEVICE_ERROR;
	}

	// regular shutdown?
	if (exitcode == OPDI_SHUTDOWN)
		exitcode = Opdi->shutdownExitCode;

	Opdi->logNormal(Opdi->appName + " exited with code " + Opdi->to_string(exitcode) + ": " + Opdi->getResultCodeText(exitcode));

	return exitcode;
}
