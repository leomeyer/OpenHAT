#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <iostream>
#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/param.h>
#include <dlfcn.h>
#include <syslog.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/prctl.h>

#include "Poco/Exception.h"
#include "Poco/NumberParser.h"
#include "Poco/File.h"

#include "opdi_platformfuncs.h"
#include "opdi_configspecs.h"
#include "opdi_constants.h"
#include "opdi_port.h"
#include "opdi_message.h"
#include "opdi_protocol.h"
#include "opdi_slave_protocol.h"
#include "opdi_config.h"

#include "LinuxOpenHAT.h"

// global connection mode (TCP or COM)
#define MODE_TCP 1
#define MODE_SERIAL 2

static int connection_mode = 0;
static char first_com_byte = 0;

static openhat::LinuxOpenHAT* linuxOpenHAT;

void throw_system_error(const char* what, const char* info = nullptr) {
	std::stringstream fullWhatSS;
	fullWhatSS << what << ": " << (info != nullptr ? info : "") << (info != nullptr ? ": " : "") << strerror(errno);
	std::string fullWhat = fullWhatSS.str();
	throw Poco::ApplicationException(fullWhat.c_str());
}

namespace openhat {

/** For TCP connections, receives a byte from the socket specified in info and places the result in byte.
*   For serial connections, reads a byte from the file handle specified in info and places the result in byte.
*   Blocks until data is available or the timeout expires.
*   If an error occurs returns an error code != 0.
*   If the connection has been gracefully closed, returns STATUS_DISCONNECTED.
*/
static uint8_t io_receive(void* info, uint8_t* byte, uint16_t timeout, uint8_t canSend) {
	char c;
	int result;
	long ticks = opdi_get_time_ms();

	while (1) {
		// call work function
		uint8_t waitResult = Opdi->waiting(canSend);
		if (waitResult != OPDI_STATUS_OK)
			return waitResult;

		if (connection_mode == MODE_TCP) {

			int newsockfd = (long)info;

			// try to read data
			result = read(newsockfd, &c, 1);
			if (result < 0) {
				// timed out?
				if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
					// possible timeout
					// "real" timeout condition
					if (opdi_get_time_ms() - ticks >= timeout)
						return OPDI_TIMEOUT;
				} else
				// perhaps Ctrl+C
				if (errno == EINTR) {
					Opdi->shutdown(OPENHATD_INTERRUPTED);
				}
				else {
					printf("Error: %d\n", errno);
					// other error condition
					return OPDI_NETWORK_ERROR;
				}
			}
			else
			// connection closed?
			if (result == 0)
				// dirty disconnect
				return OPDI_NETWORK_ERROR;
			else
				// a byte has been received
//				printf("%i", c);
				break;
		}
		else
		if (connection_mode == MODE_SERIAL) {
			int fd = (long)info;
			char inputData;
			int bytesRead;

			// first byte of connection remembered?
			if (first_com_byte != 0) {
				c = first_com_byte;
				first_com_byte = 0;
				break;
			}

			if ((bytesRead = read(fd, &inputData, 1)) >= 0) {
				if (bytesRead == 1) {
					// a byte has been received
					c = inputData;
					break;
				}
				else {
					// ran into timeout
					// "real" timeout condition
					if (opdi_get_time_ms() - ticks >= timeout)
						return OPDI_TIMEOUT;
				}
			}
			else {
				// device error
				return OPDI_DEVICE_ERROR;
			}
		}
	}

	*byte = (uint8_t)c;

	return OPDI_STATUS_OK;
}

/** For TCP connections, sends count bytes to the socket specified in info.
*   For serial connections, writes count bytes to the file handle specified in info.
*   If an error occurs returns an error code != 0. */
static uint8_t io_send(void* info, uint8_t* bytes, uint16_t count) {
	char* c = (char*)bytes;

	if (connection_mode == MODE_TCP) {

		int newsockfd = (long)info;
sendloop:
		int result = send(newsockfd, c, count, MSG_DONTWAIT);
		if (result < 0) {
			// send buffer full?
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				// wait for some time
				usleep(10000);
				goto sendloop;
			} else
			// perhaps Ctrl+C
			if (errno == EINTR) {
				Opdi->shutdown(OPENHATD_INTERRUPTED);
			}
			else {
				linuxOpenHAT->logError(std::string("Socket send failed: " ) + strerror(errno));
				return OPDI_DEVICE_ERROR;
			}
		} else
		// incomplete send?
		if (result < count) {
			// advance pointer, decrease remaining count and send again
			bytes += result;
			count -= result;
			goto sendloop;
		}
		// send ok
	}
	else
	if (connection_mode == MODE_SERIAL) {
		int fd = (long)info;

		if (write(fd, c, count) != count) {
			linuxOpenHAT->logError(std::string("Serial write failed: " ) + strerror(errno));
			return OPDI_DEVICE_ERROR;
		}
	}

	return OPDI_STATUS_OK;
}

LinuxOpenHAT::LinuxOpenHAT(void)
{
	this->framesPerSecond = 0;
}

LinuxOpenHAT::~LinuxOpenHAT(void)
{
}

void LinuxOpenHAT::print(const char* text) {
	// text is treated as UTF8.
	std::cout << text;
}

void LinuxOpenHAT::println(const char* text) {
	// text is treated as UTF8.
	std::cout << text << std::endl;
}

void LinuxOpenHAT::printe(const char* text) {
	// text is treated as UTF8.
	std::cerr << text;
}

void LinuxOpenHAT::printlne(const char* text) {
	// text is treated as UTF8.
	std::cerr << text << std::endl;
}

std::string LinuxOpenHAT::getCurrentUser(void) {
	struct passwd* passwd_data = getpwuid(getuid());
	if (passwd_data == nullptr)
		throw_system_error("Unable to resolve user ID");
	
	return std::string(passwd_data->pw_name) + " (" + this->to_string(getuid()) + ")";
}

void LinuxOpenHAT::switchToUser(const std::string& newUser) {
	// this implementation expects a numeric user ID or a user name
	int uid;
	
	if (!Poco::NumberParser::tryParse(newUser, uid)) {
		// try to map user name to UID
		struct passwd* passwd_data = getpwnam(newUser.c_str());
		if (passwd_data == nullptr)
			throw_system_error("Unable to resolve user name", newUser.c_str());
		uid = passwd_data->pw_uid;
	}
	
	// if there is a persistent file, it needs to be chown'ed to the new user
	if (!this->persistentConfigFile.empty()) {
		Poco::File file(this->persistentConfigFile);
		if (file.exists() && chown(this->persistentConfigFile.c_str(), uid, -1) == -1)
			throw_system_error("Unable to change persistent file owner to new user", newUser.c_str());
	}

	// change effective user ID
	if (setuid(uid) == -1)
		throw_system_error("Unable to switch process context to new user", newUser.c_str());
		
	// enable core dumps (disabled by setuid)
	prctl(PR_SET_DUMPABLE, 1);
	
	this->logNormal("Switched to user: " + this->getCurrentUser());	
}

/** This method handles an incoming TCP connection. It blocks until the connection is closed.
*/
int LinuxOpenHAT::HandleTCPConnection(int csock) {
	opdi_Message message;
	uint8_t result;

	connection_mode = MODE_TCP;

	struct timeval aTimeout;
	aTimeout.tv_sec = 0;
	aTimeout.tv_usec = 1000;		// one ms timeout

	// set timeouts on socket
	if (setsockopt (csock, SOL_SOCKET, SO_RCVTIMEO, (char*)&aTimeout, sizeof(aTimeout)) < 0) {
		this->log("setsockopt failed");
		return OPDI_DEVICE_ERROR;
	}
	if (setsockopt (csock, SOL_SOCKET, SO_SNDTIMEO, (char*)&aTimeout, sizeof(aTimeout)) < 0) {
		this->log("setsockopt failed");
		return OPDI_DEVICE_ERROR;
	}

	// set linger on socket
	struct linger sLinger;
	sLinger.l_onoff = 1;
	sLinger.l_linger = 1;
	if (setsockopt (csock, SOL_SOCKET, SO_LINGER, (char*)&sLinger, sizeof(sLinger)) < 0) {
		this->log("setsockopt failed");
		return OPDI_DEVICE_ERROR;
	}

	// set TCP_NODELAY
	int flag = 1;
	if (setsockopt(csock, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int)) < 0) {
		this->log("setsockopt failed");
		return OPDI_DEVICE_ERROR;
	}

	// info value is the socket handle
	result = opdi_message_setup(&io_receive, &io_send, (void*)(long)csock);
	if (result != 0)
		return result;

	result = opdi_get_message(&message, OPDI_CANNOT_SEND);
	if (result != 0)
		return result;

	last_activity = opdi_get_time_ms();

	// initiate handshake
	result = opdi_slave_start(&message, NULL, &protocol_callback);

	return result;
}

int LinuxOpenHAT::setupTCP(const std::string& /*interface_*/, int port) {

	// store instance reference
	linuxOpenHAT = this;

	// adapted from: http://www.linuxhowtos.org/C_C++/socket.htm

	int sockfd, newsockfd;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;

	// create socket (non-blocking)
	sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | TCP_NODELAY, 0);
	if (sockfd < 0) {
		throw_system_error("Error opening socket");
	}

	// set TCP_NODELAY
	int flag = 1;
	if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(int)) < 0) {
		this->log("setsockopt failed");
		return OPDI_DEVICE_ERROR;
	}
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*) &flag, sizeof(int)) < 0) {
		this->log("setsockopt failed");
		return OPDI_DEVICE_ERROR;
	}

	// prepare address
	bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	// bind to specified port
	if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
		throw_system_error("Error binding to socket");
	}

	// listen for an incoming connection
	listen(sockfd, 1);

	int sleepRemainderBase = 1000;
	int sleepRemainderAdjustCount = 0;

	while (true) {
		this->logNormal(std::string("Listening for a connection on TCP port ") + this->to_string(port));

		while (true) {

			clilen = sizeof(cli_addr);
			newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
			if (newsockfd < 0) {
				if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
					// measure processing time
					struct timeval tv;
					gettimeofday(&tv, NULL);
					int64_t proctime = 1000000LL * tv.tv_sec + tv.tv_usec;

					// not yet connected; process housekeeping about once a millisecond
					uint8_t waitResult = this->waiting(false);
					if (waitResult != OPDI_STATUS_OK)
						return waitResult;

					gettimeofday(&tv, NULL);
					int64_t elapsed = 1000000LL * tv.tv_sec + tv.tv_usec - proctime;

					// sleep for remainder of the millisecond
					//  usleep has some overhead that might be different on different systems
					// calculate sleep remainder base to approximate the specified target fps
					if (sleepRemainderAdjustCount > 100) {
						sleepRemainderAdjustCount = 0;
						if (this->framesPerSecond > 0) {
							if (this->framesPerSecond > this->targetFramesPerSecond)
								sleepRemainderBase++;
							else if (this->framesPerSecond < this->targetFramesPerSecond)
								sleepRemainderBase--;
						}
					} else
						sleepRemainderAdjustCount++;

					if (elapsed < sleepRemainderBase) {
						this->logExtreme(std::string("Sleeping for ") + this->to_string(sleepRemainderBase - elapsed) + " microseconds");
						usleep(sleepRemainderBase - elapsed);
					}
				} else
					this->logNormal(std::string("Error accepting connection: ") + this->to_string(errno));
			} else {

				this->logNormal((std::string("Connection attempt from ") + std::string(inet_ntoa(cli_addr.sin_addr))).c_str());

				int err = HandleTCPConnection(newsockfd);

//				shutdown(newsockfd, SHUT_WR);

				// TODO maybe there's a better way to ensure that data is sent?
//				usleep(500);

				close(newsockfd);

				if ((err != OPDI_STATUS_OK) && (err != OPDI_DISCONNECTED))
					return err;
			
				// shutdown requested?
				if (this->shutdownRequested)
					return this->shutdownExitCode;

				break;
			}
		}
	}

	return OPDI_STATUS_OK;
}

IOpenHATPlugin* LinuxOpenHAT::getPlugin(const std::string& driver) {
	// append platform specific extension if necessary
	std::string lDriver(driver);
	if (lDriver.find(".so") != lDriver.length() - 3)
		lDriver.append(".so");

	this->logDebug("Trying to load plugin shared library: " + lDriver);

	this->warnIfPluginMoreRecent(lDriver);

	void* hndl = dlopen(lDriver.c_str(), RTLD_NOW);
	if (hndl == NULL){
		throw Poco::FileException("Could not load the plugin library", dlerror());
	}

	dlerror();
	// getPluginInstance can't be declared as void* because this emits a -pedantic warning:
	// "ISO C++ forbids casting between pointer-to-function and pointer-to-object"
	// This trick is described here: https://github.com/christopherpoole/cppplugin/wiki/Plugins-in-CPP%3A-Dynamically-Linking-Shared-Objects
	IOpenHATPlugin* (*getPluginInstance)(int, int, int);
	*(void**)(&getPluginInstance) = dlsym(hndl, "GetPluginInstance");

	char* lasterror = dlerror();
	if (lasterror != NULL) {
		dlclose(hndl);
		throw Poco::ApplicationException("Invalid plugin library; could not locate function 'GetPluginInstance' in " + lDriver, lasterror);
	}

	// call the library function to get the plugin instance
	return ((IOpenHATPlugin* (*)(int, int, int))(getPluginInstance))(this->majorVersion, this->minorVersion, this->patchVersion);
}

} 		// namespace openhat
