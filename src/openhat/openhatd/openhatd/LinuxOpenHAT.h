#pragma once

#include "AbstractOpenHAT.h"

namespace openhat {

class LinuxOpenHAT : public openhat::AbstractOpenHAT
{
public:
	LinuxOpenHAT(void);

	virtual ~LinuxOpenHAT(void);

	virtual void print(const char* text);

	virtual void println(const char* text);

	virtual void printe(const char* text);

	virtual void printlne(const char* text);
	
	virtual std::string getCurrentUser(void);

	virtual void switchToUser(std::string newUser);
	
	int HandleTCPConnection(int csock);

	int setupTCP(std::string interfaces, int port);

	IOpenHATPlugin* getPlugin(std::string driver);
};

}
