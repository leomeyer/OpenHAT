#include "WindowsOpenHAT.h"

// OpenHAT main program entry point on Windows

#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>

#include "Windows.h"
#include "stdafx.h"

#include "opdi_constants.h"

// the main OPDI instance is declared here
openhat::AbstractOpenHAT* Opdi;

BOOL WINAPI SignalHandler(_In_ DWORD dwCtrlType) {
	switch (dwCtrlType) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
		std::cout << "Interrupted, exiting" << std::endl;
		Opdi->shutdown();
		return TRUE;
	case CTRL_SHUTDOWN_EVENT:
		return FALSE;
	}
	return FALSE;
}

int _tmain(int argc, _TCHAR* argv[], _TCHAR* envp[])
{
	// set console to display special characters correctly
	setlocale(LC_ALL, "");

	SetConsoleCtrlHandler(SignalHandler, true);

	// convert arguments to UTF8 strings
	std::vector<std::string> args;
	args.reserve(argc);
	for (int i = 0; i < argc; i++) {
		_TCHAR* targ = argv[i];
		std::wstring arg(targ);
		args.push_back(utf8_encode(arg));
	}

	// create a map of environment parameters (important: uppercase, prefix $)
	std::map<std::string, std::string> environment;
	size_t counter = 0;
	_TCHAR* env = envp[counter];
	while (env) {
		std::wstring envStr(env);
		wchar_t* envVar = wcstok(&envStr[0], L"=");
		if (envVar != nullptr) {
			wchar_t* envValue = wcstok(nullptr, L"");
			if (envValue != nullptr) {
				std::wstring envKey(envVar);
				// convert key to uppercase
				std::transform(envKey.begin(), envKey.end(), envKey.begin(), towupper);
				environment[std::string("$") + utf8_encode(envKey)] = utf8_encode(envValue);
				// std::wcout << envVar << L"=" << envValue << std::endl;
			}
		}
		counter++;
		env = envp[counter];
	}

	Opdi = new openhat::WindowsOpenHAT();
	Opdi->appName = "openhatd";

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

	Opdi->logNormal(Opdi->appName + " exited with code " + Opdi->to_string(exitcode));

	return exitcode;
}

