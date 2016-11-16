
// Test plugin for OpenHAT (Windows version)

#include "opdi_constants.h"

#include "WindowsOpenHAT.h"

namespace {

class DigitalTestPort : public opdi::DigitalPort {
public:
	DigitalTestPort();
	virtual void setLine(uint8_t line, ChangeSource /*changeSource*/) override;
};

DigitalTestPort::DigitalTestPort() : opdi::DigitalPort("PluginPort", "Windows Test Plugin Port", OPDI_PORTDIRCAP_OUTPUT, 0) {}

void DigitalTestPort::setLine(uint8_t line, ChangeSource changeSource) {
	opdi::DigitalPort::setLine(line, changeSource);

	openhat::AbstractOpenHAT*openhat = (openhat::AbstractOpenHAT*)this->opdi;
	if (line == 0) {
		openhat->logNormal("DigitalTestPort line set to Low");
	} else {
		openhat->logNormal("DigitalTestPort line set to High");
	}
}

class WindowsTestPlugin : public IOpenHATPlugin, public openhat::IConnectionListener {

protected:
	openhat::AbstractOpenHAT* openhat;

public:
	virtual void setupPlugin(openhat::AbstractOpenHAT* abstractOpenHAT, const std::string& node, openhat::ConfigurationView* config) override;

	virtual void masterConnected(void) override;
	virtual void masterDisconnected(void) override;
};

}	// end anonymous namespace


void WindowsTestPlugin::setupPlugin(openhat::AbstractOpenHAT* abstractOpenHAT, const std::string& node, openhat::ConfigurationView* config) {
	this->openhat = abstractOpenHAT;

	Poco::AutoPtr<openhat::ConfigurationView> nodeConfig = this->openhat->createConfigView(config, node);

	// get port type
	std::string portType = nodeConfig->getString("Type", "");

	if (portType == "DigitalPort") {
		// add emulated test port
		DigitalTestPort* port = new DigitalTestPort();
		abstractOpenHAT->configureDigitalPort(nodeConfig, port);
		abstractOpenHAT->addPort(port);
	} else
		this->openhat->throwSettingsException("This plugin supports only node type 'DigitalPort'", portType);

	this->openhat->addConnectionListener(this);

	this->openhat->logVerbose("WindowsTestPlugin setup completed successfully as node " + node);
}

void WindowsTestPlugin::masterConnected() {
	this->openhat->logNormal("Test plugin: master connected");
}

void WindowsTestPlugin::masterDisconnected() {
	this->openhat->logNormal("Test plugin: master disconnected");
}

// plugin instance factory function
extern "C" __declspec(dllexport) IOpenHATPlugin* __cdecl GetPluginInstance(int majorVersion, int minorVersion, int patchVersion) {

	// check whether the version is supported
	if ((majorVersion != OPENHAT_MAJOR_VERSION) || (minorVersion != OPENHAT_MINOR_VERSION))
		throw Poco::Exception("This plugin requires OpenHAT version "
			OPDI_QUOTE(OPENHAT_MAJOR_VERSION) "." OPDI_QUOTE(OPENHAT_MINOR_VERSION));

	// return a new instance of this plugin
	return new WindowsTestPlugin();
}