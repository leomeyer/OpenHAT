
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

DigitalTestPort::DigitalTestPort() : opdi::DigitalPort("PluginPort", "Linux Test Plugin Port", OPDI_PORTDIRCAP_OUTPUT, 0) {}

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
	virtual void setupPlugin(openhat::AbstractOpenHAT* openhat, const std::string& node, Poco::Util::AbstractConfiguration* config);

	virtual void masterConnected(void) override;
	virtual void masterDisconnected(void) override;
};


void LinuxTestOpenHATPlugin::setupPlugin(openhat::AbstractOpenHAT* openhat, const std::string& node, Poco::Util::AbstractConfiguration* config) {
	this->openhat = openhat;

	Poco::Util::AbstractConfiguration* nodeConfig = config->createView(node);

	// get port type
	std::string portType = nodeConfig->getString("Type", "");

	if (portType == "DigitalPort") {
		// add emulated test port
		DigitalTestPort* port = new DigitalTestPort();
		openhat->configureDigitalPort(nodeConfig, port);
		openhat->addPort(port);
	} else
		throw Poco::DataException("This plugin supports only node type 'DigitalPort'", portType);

	this->openhat->logVerbose("LinuxTestOpenHATPlugin setup completed successfully as node " + node);
}

void LinuxTestOpenHATPlugin::masterConnected() {
	this->openhat->logNormal("Test plugin: master connected");
}

void LinuxTestOpenHATPlugin::masterDisconnected() {
	this->openhat->logNormal("Test plugin: master disconnected");
}

extern "C" IOpenHATPlugin* GetOpenHATPluginInstance(int majorVersion, int minorVersion, int patchVersion) {

	// check whether the version is supported
	if ((majorVersion > 0) || (minorVersion > 1))
		throw Poco::Exception("This version of the LinuxTestOPDIPlugin supports only OpenHAT versions up to 0.1");

	// return a new instance of this plugin
	return new LinuxTestOpenHATPlugin();
}
