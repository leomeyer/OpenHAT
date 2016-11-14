#include "Configuration.h"

#include <sstream>

#include "Poco/FileStream.h"

bool OpenHATConfigurationFile::getRaw(const std::string & key, std::string & value) const {
	bool result = Poco::Util::IniFileConfiguration::getRaw(key, value);
	if (result) {
		// detect quotes around the value
		if ((value.size() > 1) && (value[0] == '"') && (value[value.size() - 1] == '"')) {
			value = value.substr(1, value.size() - 2);
		}
	}
	return result;
}

OpenHATConfigurationFile::OpenHATConfigurationFile(const std::string& path, std::map<std::string, std::string> parameters) {

	// load the file content
	Poco::FileInputStream istr(path, std::ios::in);

	if (!istr.good())
		throw Poco::OpenFileException(path);
	
	std::string content;
	std::string line;
	while (std::getline(istr, line)) {
		content += line;
		content.push_back('\n');
	}

	// replace parameters in content
	for (auto iterator = parameters.begin(), iteratorEnd = parameters.end(); iterator != iteratorEnd; ++iterator) {
		std::string key = iterator->first;
		std::string value = iterator->second;

		size_t start = 0;
		while ((start = content.find(key, start)) != std::string::npos) {
			content.replace(start, key.length(), value);
		}
	}

	// load the configuration from the new content
	std::stringstream newContent;
	newContent.str(content);
	this->load(newContent);
}
