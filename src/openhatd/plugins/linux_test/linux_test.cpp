
// Test plugin for OpenHAT (Linux version)

#include "opdi_constants.h"

#include "LinuxOpenHAT.h"

namespace {

class DigitalTestPort : public opdi::DigitalPort {
public:
	DigitalTestPort();
	virtual void setLine(uint8_t line, ChangeSource changeSource = ChangeSource::CHANGESOURCE_INT) override;
};

}

DigitalTestPort::DigitalTestPort() : opdi::DigitalPort("LinuxTestPluginPort", OPDI_PORTDIRCAP_OUTPUT, 0) {}

void DigitalTestPort::setLine(uint8_t line, ChangeSource /*changeSource*/) {
	opdi::DigitalPort::setLine(line);

	openhat::AbstractOpenHAT* openhat = (openhat::AbstractOpenHAT*)this->opdi;
	if (line == 0) {
		openhat->logNormal("DigitalTestPort line set to Low");
	} else {
		openhat->logNormal("DigitalTestPort line set to High");
	}
}

class LinuxTestOpenHATPlugin : public IOpenHATPlugin, public openhat::IConnectionListener {

protected:
	openhat::AbstractOpenHAT* openhat;

public:
	virtual void setupPlugin(openhat::AbstractOpenHAT* openhat, const std::string& node, openhat::ConfigurationView* config);

	virtual void masterConnected(void) override;
	virtual void masterDisconnected(void) override;
};


void LinuxTestOpenHATPlugin::setupPlugin(openhat::AbstractOpenHAT* openhat, const std::string& node, openhat::ConfigurationView* config) {
	this->openhat = openhat;

	// add emulated test port
	DigitalTestPort* port = new DigitalTestPort();
	openhat->addPort(port);

	this->openhat->logVerbose("LinuxTestOpenHATPlugin setup completed successfully as node " + node);
}

void LinuxTestOpenHATPlugin::masterConnected() {
	this->openhat->logNormal("Test plugin: master connected");
}

void LinuxTestOpenHATPlugin::masterDisconnected() {
	this->openhat->logNormal("Test plugin: master disconnected");
}

extern "C" IOpenHATPlugin* GetPluginInstance(int majorVersion, int minorVersion, int patchVersion) {
	// check whether the version is supported
	if ((majorVersion != OPENHAT_MAJOR_VERSION) || (minorVersion != OPENHAT_MINOR_VERSION))
		throw Poco::Exception("This plugin requires OpenHAT version "
			OPDI_QUOTE(OPENHAT_MAJOR_VERSION) "." OPDI_QUOTE(OPENHAT_MINOR_VERSION));

	// return a new instance of this plugin
	return new LinuxTestOpenHATPlugin();
}
