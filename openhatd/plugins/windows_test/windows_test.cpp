
// Test plugin for OpenHAT (Windows version)

#include "WindowsOpenHAT.h"

namespace {

class DigitalTestPort : public opdi::DigitalPort {
public:
	DigitalTestPort();
	virtual bool setLine(uint8_t line, ChangeSource /*changeSource*/) override;
};

DigitalTestPort::DigitalTestPort() : opdi::DigitalPort("WindowsTestPluginPort", OPDI_PORTDIRCAP_OUTPUT, 0) {}

bool DigitalTestPort::setLine(uint8_t line, ChangeSource changeSource) {
	bool changed = opdi::DigitalPort::setLine(line, changeSource);

	openhat::AbstractOpenHAT*openhat = (openhat::AbstractOpenHAT*)this->opdi;
	if (line == 0) {
		openhat->logNormal("DigitalTestPort line set to Low");
	} else {
		openhat->logNormal("DigitalTestPort line set to High");
	}

	return changed;
}

class WindowsTestPlugin : public IOpenHATPlugin, public openhat::IConnectionListener {

protected:
	openhat::AbstractOpenHAT* openhat;

public:
	virtual void setupPlugin(openhat::AbstractOpenHAT* openHAT, const std::string& nodeName, openhat::ConfigurationView::Ptr nodeConfig, openhat::ConfigurationView::Ptr parentConfig, const std::string& driverPath) override;

	virtual void masterConnected(void) override;
	virtual void masterDisconnected(void) override;
};

}	// end anonymous namespace


void WindowsTestPlugin::setupPlugin(openhat::AbstractOpenHAT* openHAT, const std::string& nodeName, openhat::ConfigurationView::Ptr nodeConfig, openhat::ConfigurationView::Ptr parentConfig, const std::string& driverPath) {
	this->openhat = openHAT;

	// get port type
	std::string portType = nodeConfig->getString("Type", "");

	if (portType == "DigitalPort") {
		// add test port
		DigitalTestPort* port = new DigitalTestPort();
		this->openhat->configureDigitalPort(nodeConfig, port);
		this->openhat->addPort(port);
	} else
		this->openhat->throwSettingException("This plugin supports only node type 'DigitalPort'", portType);

	this->openhat->addConnectionListener(this);

	this->openhat->logVerbose("WindowsTestPlugin setup completed successfully as node " + nodeName);
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
	if ((majorVersion != openhat::OPENHAT_MAJOR_VERSION) || (minorVersion != openhat::OPENHAT_MINOR_VERSION))
		throw Poco::Exception("This plugin requires OpenHAT version "
			OPDI_QUOTE(openhat::OPENHAT_MAJOR_VERSION) "." OPDI_QUOTE(openhat::OPENHAT_MINOR_VERSION));

	// return a new instance of this plugin
	return new WindowsTestPlugin();
}