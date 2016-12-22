#include <sstream>

#include <Poco/File.h>
#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Object.h>

#include "AbstractOpenHAT.h"

#include <mongoose.h>

#ifdef _WINDOWS
#include <windows.h>
#endif

namespace {

////////////////////////////////////////////////////////////////////////
// Plugin main class
////////////////////////////////////////////////////////////////////////

class WebServerPlugin : public IOpenHATPlugin, public opdi::DigitalPort {

	class InvalidRequestException : public Poco::Exception
	{
	public:
		explicit InvalidRequestException(std::string message): Poco::Exception(message) {};
	};

	class MethodNotFoundException : public Poco::Exception
	{
	public:
		explicit MethodNotFoundException(std::string message): Poco::Exception(message) {};
	};

	openhat::AbstractOpenHAT* openhat;
	// web server management structures
	struct mg_mgr mgr;
	struct mg_connection* nc;
	struct mg_serve_http_opts s_http_server_opts;	// https://docs.cesanta.com/mongoose/dev/#/c-api/http.h/struct_mg_serve_http_opts/
	
	std::string httpPort;
	std::string documentRoot;
	std::string enableDirListing;
	std::string indexFiles;
	std::string perDirectoryAuthFile;
	std::string authDomain;
	std::string globalAuthFile;
	std::string ipACL;

	std::string jsonRpcUrl;

public:
	WebServerPlugin(): opdi::DigitalPort("WebServerPlugin", OPDI_PORTDIRCAP_OUTPUT, 0), mgr() {
		memset(&this->s_http_server_opts, 0, sizeof(mg_serve_http_opts));
		this->httpPort = "8080";
		this->documentRoot = "./webdocroot/";
		this->enableDirListing = "yes";
		this->indexFiles = "index.html";
		this->jsonRpcUrl = "/api/jsonrpc";
		this->nc = nullptr;

		this->setMode(OPDI_DIGITAL_MODE_OUTPUT, opdi::Port::ChangeSource::CHANGESOURCE_INT);
		this->setLine(1, opdi::Port::ChangeSource::CHANGESOURCE_INT);
	};

	virtual void setupPlugin(openhat::AbstractOpenHAT* openhat, const std::string& node, openhat::ConfigurationView* nodeConfig, const std::string& driverPath) override;

	void handleEvent(struct mg_connection* nc, int ev, void* p);

	virtual uint8_t doWork(uint8_t canSend) override;
	virtual void shutdown(void) override;

	void onAllPortsRefreshed(const void* pSender);
	void onPortRefreshed(const void* pSender, opdi::Port*& port);
	
	// JSON-RPC functions

	void sendJsonRpcError(struct mg_connection* nc, Poco::Dynamic::Var id, int code, const std::string& message);

	/** This method returns the state of the given port as a JSON object. */
	Poco::JSON::Object jsonGetPortState(opdi::Port* port);

	/** This method returns information about the given port as a JSON object. */
	Poco::JSON::Object jsonGetPortInfo(opdi::Port* port);

	/** This method returns the list of (non-hidden) ports as a JSON array. */
	Poco::JSON::Array jsonGetPortList();

	/** This method returns the list of groups as a JSON array. */
	Poco::JSON::Array jsonGetPortGroups();

	/** This method returns information about the device (name, ports, groups, ...) as a JSON object. */
	Poco::JSON::Object jsonRpcGetDeviceInfo(struct mg_connection* nc, struct http_message* hm, Poco::Dynamic::Var& params);

	/** This method expects the port ID in the portID parameter of the params object. */
	Poco::JSON::Object jsonRpcGetPortInfo(struct mg_connection* nc, struct http_message* hm, Poco::Dynamic::Var& params);

	/** This method expects the port ID in the portID parameter and the new line state in the line parameter of the params object.
	* It returns the port info object. */
	Poco::JSON::Object jsonRpcSetDigitalState(struct mg_connection* nc, struct http_message* hm, Poco::Dynamic::Var& params);

	/** This method expects the port ID in the portID parameter and the new value in the value parameter of the params object.
	* It returns the port info object. */
	Poco::JSON::Object jsonRpcSetAnalogValue(struct mg_connection* nc, struct http_message* hm, Poco::Dynamic::Var& params);

	/** This method expects the port ID in the portID parameter and the new position in the position parameter of the params object.
	* It returns the port info object. */
	Poco::JSON::Object jsonRpcSetDialPosition(struct mg_connection* nc, struct http_message* hm, Poco::Dynamic::Var& params);

	/** This method expects the port ID in the portID parameter and the new position in the position parameter of the params object.
	* It returns the port info object. */
	Poco::JSON::Object jsonRpcSetSelectPosition(struct mg_connection* nc, struct http_message* hm, Poco::Dynamic::Var& params);
};

}	// end anonymous namespace

static WebServerPlugin* instance;

// Mongoose event handler function
static void ev_handler(struct mg_connection* nc, int ev, void* p) {
	// delegate to class instance
	instance->handleEvent(nc, ev, p);
}

Poco::JSON::Object WebServerPlugin::jsonGetPortState(opdi::Port* port) {
	Poco::JSON::Object result;
	if (port->hasError())
		result.set("error", this->to_string(port->getError()));
	else
		try {
			// query port state
			if (0 == strcmp(port->getType(), OPDI_PORTTYPE_DIGITAL)) {
				opdi::DigitalPort* dport = (opdi::DigitalPort*)port;
				uint8_t mode;
				uint8_t line;
				dport->getState(&mode, &line);
				result.set("mode", mode);
				result.set("line", line);
			} else
			if (0 == strcmp(port->getType(), OPDI_PORTTYPE_ANALOG)) {
				uint8_t mode;
				uint8_t resolution;
				uint8_t reference;
				int32_t value;
				((opdi::AnalogPort*)port)->getState(&mode, &resolution, &reference, &value);
				result.set("mode", mode);
				result.set("resolution", resolution);
				result.set("reference", reference);
				result.set("value", value);
			} else
			if (0 == strcmp(port->getType(), OPDI_PORTTYPE_DIAL)) {
				opdi::DialPort* dport = (opdi::DialPort*)port;
				int64_t position;
				dport->getState(&position);
				result.set("position", position);
			} else
			if (0 == strcmp(port->getType(), OPDI_PORTTYPE_SELECT)) {
				opdi::SelectPort* sport = (opdi::SelectPort*)port;
				uint16_t position;
				sport->getState(&position);
				result.set("position", position);
			} else
				throw Poco::Exception("Port " + port->ID() + ": The port type is unknown");
		} catch (Poco::Exception&) {
			result.set("error", this->to_string(port->getError()));
		}

	return result;
}

Poco::JSON::Object WebServerPlugin::jsonGetPortInfo(opdi::Port* port) {
	Poco::JSON::Object result;
	result.set("id", port->ID());
	result.set("type", port->getType());
	result.set("label", port->getLabel());
	result.set("dirCaps", port->getDirCaps());
	result.set("flags", port->getFlags());
	result.set("readonly", port->isReadonly());
	if (0 == strcmp(port->getType(), OPDI_PORTTYPE_DIAL)) {
		opdi::DialPort* dport = (opdi::DialPort*)port;
		result.set("min", dport->getMin());
		result.set("max", dport->getMax());
		result.set("step", dport->getStep());
	} else
		if (0 == strcmp(port->getType(), OPDI_PORTTYPE_SELECT)) {
		opdi::SelectPort* sport = (opdi::SelectPort*)port;
		Poco::JSON::Array positions;
		for (uint16_t i = 0; i <= sport->getMaxPosition(); i++) {
			positions.add(sport->getPositionLabel(i));
		}
		result.set("positions", positions);
	}
	result.set("state", this->jsonGetPortState(port));
	result.set("extendedInfo", port->getExtendedInfo());
	result.set("extendedState", port->getExtendedState());
	return result;
}

Poco::JSON::Object WebServerPlugin::jsonRpcGetPortInfo(struct mg_connection* /*nc*/, struct http_message* /*hm*/, Poco::Dynamic::Var& params) {
	Poco::JSON::Object::Ptr object = params.extract<Poco::JSON::Object::Ptr>();
	Poco::Dynamic::Var portID = object->get("portID");
	std::string portIDStr = portID.convert<std::string>();

	if (portIDStr == "")
		throw Poco::InvalidArgumentException("Method getPortData: parameter portID is missing");

	opdi::Port* port = this->opdi->findPortByID(portIDStr.c_str());
	if (port == NULL)
		throw Poco::InvalidArgumentException(std::string("Method getPortData: port not found: ") + portIDStr);

	return this->jsonGetPortInfo(port);
}

Poco::JSON::Array WebServerPlugin::jsonGetPortList() {
	// return an array of port objects
	Poco::JSON::Array result;
	opdi::PortList pl = this->opdi->getPorts();
	auto it = pl.begin();
	auto ite = pl.end();
	while (it != ite) {
		if (!(*it)->isHidden())
			result.add(this->jsonGetPortInfo(*it));
		++it;
	}

	return result;
}

Poco::JSON::Array WebServerPlugin::jsonGetPortGroups() {
	// return an array of group objects
	Poco::JSON::Array result;
	opdi::PortGroupList gl = this->opdi->getPortGroups();
	auto it = gl.begin();
	auto ite = gl.end();
	while (it != ite) {
		Poco::JSON::Object group;
		group.set("id", std::string((*it)->getID()));
		group.set("label", std::string((*it)->getLabel()));
		group.set("parent", std::string((*it)->getParent()));
		result.add(group);
		++it;
	}

	return result;
}

Poco::JSON::Object WebServerPlugin::jsonRpcGetDeviceInfo(struct mg_connection* /*nc*/, struct http_message* /*hm*/, Poco::Dynamic::Var& /*params*/) {
	// return an object that represents the top group
	// sub-groups will be contained in its subgroups member
	Poco::JSON::Object result;

	result.set("name", this->opdi->getSlaveName());
	result.set("ports", this->jsonGetPortList());
	result.set("groups", this->jsonGetPortGroups());
	result.set("info", this->openhat->getDeviceInfo());

	return result;
}

Poco::JSON::Object WebServerPlugin::jsonRpcSetDigitalState(struct mg_connection* /*nc*/, struct http_message* /*hm*/, Poco::Dynamic::Var& params) {
	Poco::JSON::Object::Ptr object = params.extract<Poco::JSON::Object::Ptr>();
	Poco::Dynamic::Var portID = object->get("portID");
	if (portID.isEmpty())
		throw Poco::InvalidArgumentException("Method setDigitalState: parameter portID is missing");

	std::string portIDStr = portID.convert<std::string>();
	if (portIDStr == "")
		throw Poco::InvalidArgumentException("Method setDigitalState: parameter portID is missing");

	opdi::Port* port = this->opdi->findPortByID(portIDStr.c_str());
	if (port == NULL)
		throw Poco::InvalidArgumentException(std::string("Method setDigitalState: port not found: ") + portIDStr);

	if (0 != strcmp(port->getType(), OPDI_PORTTYPE_DIGITAL))
		throw Poco::InvalidArgumentException(std::string("Method setDigitalState: Specified port is not a digital port: ") + portIDStr);
	Poco::Dynamic::Var line = object->get("line");
	if (line.isEmpty())
		throw Poco::InvalidArgumentException("Method setDigitalState: parameter line is missing");
	uint8_t newLine = line.convert<uint8_t>();

	if ((newLine != OPDI_DIGITAL_LINE_LOW) && (newLine != OPDI_DIGITAL_LINE_HIGH))
		throw Poco::InvalidArgumentException(std::string("Method setDigitalState: Illegal value for line: ") + this->to_string((int)newLine));

	if (!port->isReadonly())
		((opdi::DigitalPort*)port)->setLine(newLine, opdi::Port::ChangeSource::CHANGESOURCE_USER);

	return this->jsonGetPortInfo(port);
}

Poco::JSON::Object WebServerPlugin::jsonRpcSetAnalogValue(struct mg_connection* /*nc*/, struct http_message* /*hm*/, Poco::Dynamic::Var& params) {
	Poco::JSON::Object::Ptr object = params.extract<Poco::JSON::Object::Ptr>();
	Poco::Dynamic::Var portID = object->get("portID");
	if (portID.isEmpty())
		throw Poco::InvalidArgumentException("Method setAnalogValue: parameter portID is missing");

	std::string portIDStr = portID.convert<std::string>();
	if (portIDStr == "")
		throw Poco::InvalidArgumentException("Method setAnalogValue: parameter portID is missing");

	opdi::Port* port = this->opdi->findPortByID(portIDStr.c_str());
	if (port == NULL)
		throw Poco::InvalidArgumentException(std::string("Method setAnalogValue: port not found: ") + portIDStr);

	if (0 != strcmp(port->getType(), OPDI_PORTTYPE_ANALOG))
		throw Poco::InvalidArgumentException(std::string("Method setAnalogValue: Specified port is not an analog port: ") + portIDStr);

	Poco::Dynamic::Var value = object->get("value");
	if (value.isEmpty())
		throw Poco::InvalidArgumentException("Method setAnalogValue: parameter value is missing");
	int32_t newValue = value.convert<int32_t>();

	if (!port->isReadonly())
		((opdi::AnalogPort*)port)->setValue(newValue, opdi::Port::ChangeSource::CHANGESOURCE_USER);

	return this->jsonGetPortInfo(port);
}

Poco::JSON::Object WebServerPlugin::jsonRpcSetDialPosition(struct mg_connection* /*nc*/, struct http_message* /*hm*/, Poco::Dynamic::Var& params) {
	Poco::JSON::Object::Ptr object = params.extract<Poco::JSON::Object::Ptr>();
	Poco::Dynamic::Var portID = object->get("portID");
	if (portID.isEmpty())
		throw Poco::InvalidArgumentException("Method setDialPosition: parameter portID is missing");

	std::string portIDStr = portID.convert<std::string>();
	if (portIDStr == "")
		throw Poco::InvalidArgumentException("Method setDialPosition: parameter portID is missing");

	opdi::Port* port = this->opdi->findPortByID(portIDStr.c_str());
	if (port == NULL)
		throw Poco::InvalidArgumentException(std::string("Method setDialPosition: port not found: ") + portIDStr);

	if (0 != strcmp(port->getType(), OPDI_PORTTYPE_DIAL))
		throw Poco::InvalidArgumentException(std::string("Method setDialPosition: Specified port is not a dial port: ") + portIDStr);

	Poco::Dynamic::Var position = object->get("position");
	if (position.isEmpty())
		throw Poco::InvalidArgumentException("Method setDialPosition: parameter position is missing");
	int64_t newPosition = position.convert<int64_t>();

	if (!port->isReadonly())
		((opdi::DialPort*)port)->setPosition(newPosition, opdi::Port::ChangeSource::CHANGESOURCE_USER);

	return this->jsonGetPortInfo(port);
}

Poco::JSON::Object WebServerPlugin::jsonRpcSetSelectPosition(struct mg_connection* /*nc*/, struct http_message* /*hm*/, Poco::Dynamic::Var& params) {
	Poco::JSON::Object::Ptr object = params.extract<Poco::JSON::Object::Ptr>();
	Poco::Dynamic::Var portID = object->get("portID");
	if (portID.isEmpty())
		throw Poco::InvalidArgumentException("Method setSelectPosition: parameter portID is missing");

	std::string portIDStr = portID.convert<std::string>();
	if (portIDStr == "")
		throw Poco::InvalidArgumentException("Method setSelectPosition: parameter portID is missing");

	opdi::Port* port = this->opdi->findPortByID(portIDStr.c_str());
	if (port == NULL)
		throw Poco::InvalidArgumentException(std::string("Method setSelectPosition: port not found: ") + portIDStr);

	if (0 != strcmp(port->getType(), OPDI_PORTTYPE_SELECT))
		throw Poco::InvalidArgumentException(std::string("Method setSelectPosition: Specified port is not a select port: ") + portIDStr);

	Poco::Dynamic::Var position = object->get("position");
	if (position.isEmpty())
		throw Poco::InvalidArgumentException("Method setSelectPosition: parameter position is missing");
	uint16_t newPosition = position.convert<uint16_t>();

	if (!port->isReadonly())
		((opdi::SelectPort*)port)->setPosition(newPosition, opdi::Port::ChangeSource::CHANGESOURCE_USER);

	return this->jsonGetPortInfo(port);
}

// get sockaddr, IPv4 or IPv6:
static void* get_in_addr(struct sockaddr* sa)
{
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void WebServerPlugin::sendJsonRpcError(struct mg_connection* nc, Poco::Dynamic::Var id, int code, const std::string& message) {
	Poco::JSON::Object error;
	error.set("code", code);
	error.set("message", message);

	// send error response
	Poco::JSON::Object response;
	response.set("jsonrpc", "2.0");
	response.set("id", id);
	response.set("error", error);
	response.set("result", Poco::Dynamic::Var());	// will resolve to null on the client
					
	std::stringstream sOut;
	response.stringify(sOut);
	std::string strOut = sOut.str();

	this->logDebug("Sending JSON-RPC error: " + strOut);

	mg_printf(nc, "HTTP/1.0 200 OK\r\nContent-Length: %zu\r\n"
		"Content-Type: application/json\r\n\r\n%s",
		strOut.size(), strOut.c_str());
}

void WebServerPlugin::handleEvent(struct mg_connection* nc, int ev, void* p) {

	struct http_message* hm = (struct http_message*)p;
	
  	switch (ev) {
		case MG_EV_HTTP_REQUEST:
			// prepare log message
			char address[INET6_ADDRSTRLEN];
			inet_ntop(nc->sa.sa.sa_family, get_in_addr(&nc->sa.sa), address, sizeof address);
			this->logDebug(std::string("Request received from: ") + address + " for: " + std::string(hm->uri.p, hm->uri.len));

			// JSON-RPC url received?
			if (mg_vcmp(&hm->uri, jsonRpcUrl.c_str()) == 0) {
				std::string json(hm->body.p, hm->body.len);
				this->logDebug("Received JSON-RPC request: " + json);
				// parse JSON
				Poco::Dynamic::Var id;
				try {
					Poco::JSON::Parser parser;
					Poco::Dynamic::Var request = parser.parse(json);
					Poco::JSON::Object::Ptr object = request.extract<Poco::JSON::Object::Ptr>();
					Poco::Dynamic::Var method = object->get("method");
					std::string methodStr = method.convert<std::string>();
					Poco::Dynamic::Var params = object->get("params");
					id = object->get("id");
					Poco::Dynamic::Var jsonrpc = object->get("jsonrpc");
					std::string jsonrpcStr = jsonrpc.convert<std::string>();

					// validate request
					if (jsonrpcStr != "2.0")
						throw InvalidRequestException("Invalid version number, expected 2.0");
					if (methodStr == "")
						throw InvalidRequestException("Method name missing");
					
					Poco::JSON::Object result;
					if (methodStr == "getDeviceInfo") {
						result.set("deviceInfo", this->jsonRpcGetDeviceInfo(nc, hm, params));
					} else
					if (methodStr == "getPortInfo") {
						result.set("port", this->jsonRpcGetPortInfo(nc, hm, params));
					} else
					if (methodStr == "setDigitalState") {
						result.set("port", this->jsonRpcSetDigitalState(nc, hm, params));
					} else
					if (methodStr == "setAnalogValue") {
						result.set("port", this->jsonRpcSetAnalogValue(nc, hm, params));
					} else
					if (methodStr == "setDialPosition") {
						result.set("port", this->jsonRpcSetDialPosition(nc, hm, params));
					} else
					if (methodStr == "setSelectPosition") {
						result.set("port", this->jsonRpcSetSelectPosition(nc, hm, params));
					} else
						throw MethodNotFoundException(std::string("Unknown JSON-RPC method: ") + methodStr);

					// create JSON-RPC response object
					
					Poco::JSON::Object response;
					response.set("jsonrpc", "2.0");
					response.set("id", id);
					response.set("error", Poco::Dynamic::Var());	// will resolve to null on the client
					response.set("result", result);
					
					std::stringstream sOut;
					response.stringify(sOut);
					std::string strOut = sOut.str();

					this->logDebug("Sending JSON-RPC response: " + strOut);

					// send result
					mg_printf(nc, "HTTP/1.0 200 OK\r\nContent-Length: %zu\r\n"
						"Content-Type: application/json\r\n\r\n%s",
						strOut.size(), strOut.c_str());

				// Error handling:
				// http://www.jsonrpc.org/specification, section 5.1
				} catch (Poco::JSON::JSONException& e) {
					// Parse error
					std::string err("Error processing JSON: ");
					err.append(e.what());
					err.append(": ");
					err.append(e.message());
					this->logVerbose("" + err);
					this->sendJsonRpcError(nc, id, -32700, err);	// Parse error
				} catch (InvalidRequestException& e) {
					// Invalid Request
					std::string err("Invalid JSON request: ");
					err.append(e.message());
					this->logVerbose("" + err);
					this->sendJsonRpcError(nc, id, -32600, err);	// Invalid Request
				} catch (MethodNotFoundException& e) {
					// Method not found
					std::string err("Method not found: ");
					err.append(e.message());
					this->logVerbose("" + err);
					this->sendJsonRpcError(nc, id, -32601, err);	// Method not found
				} catch (Poco::InvalidArgumentException& e) {
					// Invalid params
					std::string err("Invalid parameters: ");
					err.append(e.message());
					this->logVerbose("" + err);
					this->sendJsonRpcError(nc, id, -32602, err);	// Invalid params
				} catch (Poco::Exception& e) {
					// Internal error
					std::string err("Error processing request: ");
					err.append(e.what());
					err.append(": ");
					err.append(e.message());
					this->logVerbose("" + err);
					this->sendJsonRpcError(nc, id, -32603, err);	// Internal error
				} catch (std::exception& e) {
					// Internal error
					std::string err("Error processing request: ");
					err.append(e.what());
					this->logVerbose("" + err);
					this->sendJsonRpcError(nc, id, -32603, err);	// Internal error
				} catch (...) {
					// Internal error
					std::string err("Error processing request (unknown error)");
					this->logVerbose("" + err);
					this->sendJsonRpcError(nc, id, -32603, err);	// Internal error
				}
				// send data
				nc->flags |= MG_F_SEND_AND_CLOSE;
				break;
			} else
				// serve static content
				mg_serve_http(nc, hm, this->s_http_server_opts);
			break;
		default:
			break;
	  }
}

void WebServerPlugin::onAllPortsRefreshed(const void* /*pSender*/) {
	struct mg_connection *c;
	const char* message = "RefreshAll";
	int msgLen = strlen(message);

	for (c = mg_next(&this->mgr, NULL); c != NULL; c = mg_next(&this->mgr, c)) {
		if (c->flags & MG_F_IS_WEBSOCKET)
			mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, message, msgLen);
	}
}

void WebServerPlugin::onPortRefreshed(const void* /*pSender*/, opdi::Port*& port) {
	struct mg_connection *c;
	char buf[255];

	snprintf(buf, sizeof(buf), "Refresh %s", port->ID().c_str());
	int bufLen = strlen(buf);
	for (c = mg_next(&this->mgr, NULL); c != NULL; c = mg_next(&this->mgr, c)) {
		if (c->flags & MG_F_IS_WEBSOCKET)
			mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, buf, bufLen);
	}
}

void WebServerPlugin::setupPlugin(openhat::AbstractOpenHAT* openhat, const std::string& node, openhat::ConfigurationView* config, const std::string& driverPath) {
	this->opdi = this->openhat = openhat;
	this->setID(node.c_str());

	Poco::AutoPtr<openhat::ConfigurationView> nodeConfig = this->openhat->createConfigView(config, node);
	// avoid check for unused plugin keys
	nodeConfig->addUsedKey("Type");
	nodeConfig->addUsedKey("Driver");

	openhat->configureDigitalPort(nodeConfig, this, false);
	this->logVerbosity = openhat->getConfigLogVerbosity(nodeConfig, opdi::LogVerbosity::UNKNOWN);

	// get web server configuration
	this->httpPort = nodeConfig->getString("Port", this->httpPort);
	// determine document root; default is the current plugin directory
	this->documentRoot = nodeConfig->getString("DocumentRoot", this->documentRoot);
	std::string docRootRelativeTo = nodeConfig->getString("DocumentRootRelativeTo", "Plugin");
	this->documentRoot = openhat->resolveRelativePath(nodeConfig, node, this->documentRoot, docRootRelativeTo, driverPath, "DocumentRootRelativeTo");
	this->logDebug("Resolved document root to: " + this->documentRoot);
	if (!Poco::File(this->documentRoot).isDirectory())
		this->openhat->throwSettingsException("Resolved document root folder does not exist or is not a folder: " + documentRoot);
		
	// expose JSON-RPC API via special URL (can be disabled by setting the URL to "")
	this->jsonRpcUrl = nodeConfig->getString("JsonRpcUrl", this->jsonRpcUrl);
		
	this->s_http_server_opts.document_root = this->documentRoot.c_str();
	this->enableDirListing = nodeConfig->getBool("EnableDirListing", false) ? "yes" : "";
	this->s_http_server_opts.enable_directory_listing = this->enableDirListing.c_str();
	this->indexFiles = nodeConfig->getString("IndexFiles", this->indexFiles);
	this->s_http_server_opts.index_files = this->indexFiles.c_str();
	this->perDirectoryAuthFile = nodeConfig->getString("PerDirectoryAuthFile", this->perDirectoryAuthFile);
	if (this->perDirectoryAuthFile != "")
		this->s_http_server_opts.per_directory_auth_file = this->perDirectoryAuthFile.c_str();
	this->authDomain = nodeConfig->getString("AuthDomain", this->indexFiles);
	if (this->authDomain != "")
		this->s_http_server_opts.auth_domain = this->authDomain.c_str();
	this->globalAuthFile = nodeConfig->getString("GlobalAuthFile", this->globalAuthFile);
	if (this->globalAuthFile != "")
		this->s_http_server_opts.global_auth_file = this->globalAuthFile.c_str();
	this->ipACL = nodeConfig->getString("IP_ACL", this->ipACL);
	if (this->ipACL != "")
		this->s_http_server_opts.ip_acl = this->ipACL.c_str();

	this->logVerbose("Setting up web server on port " + this->httpPort);

	const char* errorString[256];
	struct mg_bind_opts opts;
	opts.user_data = nullptr;
	opts.flags = 0;
	opts.error_string = errorString;
		
	// setup web server
	mg_mgr_init(&this->mgr, nullptr);
	this->nc = mg_bind_opt(&this->mgr, this->httpPort.c_str(), ev_handler, opts);
	
	if (this->nc == nullptr)
#ifdef linux
		throw Poco::ApplicationException(this->ID() + ": Unable to setup web server: " + *errorString + ": " + strerror(errno));
#elif _WINDOWS
		throw Poco::ApplicationException(this->ID() + ": Unable to setup web server: " + *errorString);
#else
#error Platform not defined
#endif

	// set HTTP server parameters
	mg_set_protocol_http_websocket(this->nc);

	this->logVerbose("WebServerPlugin setup completed successfully on port " + this->httpPort);
	
	// register port (necessary for doWork to be called regularly)
	this->opdi->addPort(this);

	// register port refresh events (for websocket broadcasts)
	this->openhat->allPortsRefreshed += Poco::delegate(this, &WebServerPlugin::onAllPortsRefreshed);
	this->openhat->portRefreshed += Poco::delegate(this, &WebServerPlugin::onPortRefreshed);
}

uint8_t WebServerPlugin::doWork(uint8_t canSend) {
	opdi::DigitalPort::doWork(canSend);

	if (this->line == 1)
		// call Mongoose work function
		mg_mgr_poll(&this->mgr, 1);
	
	return OPDI_STATUS_OK;
}


void WebServerPlugin::shutdown() {
	//this->logVerbose("WebServerPlugin shutting down");
	mg_mgr_free(&this->mgr);
}

// plugin instance factory function

#ifdef _WINDOWS

extern "C" __declspec(dllexport) IOpenHATPlugin* __cdecl GetPluginInstance(int majorVersion, int minorVersion, int patchVersion)

#elif linux

extern "C" IOpenHATPlugin* GetPluginInstance(int majorVersion, int minorVersion, int /*patchVersion*/)

#else
#error "Unable to compile plugin instance factory function: Compiler not supported"
#endif
{
	// check whether the version is supported
	if ((majorVersion != openhat::OPENHAT_MAJOR_VERSION) || (minorVersion != openhat::OPENHAT_MINOR_VERSION))
		throw Poco::Exception("This plugin requires OpenHAT version "
			OPDI_QUOTE(openhat::OPENHAT_MAJOR_VERSION) "." OPDI_QUOTE(openhat::OPENHAT_MINOR_VERSION));

	// return a new instance of this plugin
	instance = new WebServerPlugin();
	
	return instance;
}
