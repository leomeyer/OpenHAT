#include "Configuration.h"
#include "AbstractOpenHAT.h"

#include <sstream>

#include "Poco/FileStream.h"

namespace openhat {

// OpenHATConfigurationFile

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
	// sort parameters by length in descending order
	// to ensure that longer parameters are replaced first
	std::vector<std::string> paramKeys;
	for (auto const& param : parameters)
		paramKeys.push_back(param.first);
	std::sort(paramKeys.begin(), paramKeys.end(), [](const std::string &s1, const std::string &s2) { return s1.size() > s2.size(); });

	for (auto iterator = paramKeys.begin(), iteratorEnd = paramKeys.end(); iterator != iteratorEnd; ++iterator) {
		std::string key = *iterator;
		std::string value = parameters[*iterator];

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

// ConfigurationView

ConfigurationView::ConfigurationView(AbstractOpenHAT* openhat, Poco::AutoPtr<Poco::Util::AbstractConfiguration> config, const std::string& sourceFile, const std::string& section, bool checkUnused)
	: sourceFile(sourceFile), section(section) {
	this->openhat = openhat;
	this->checkUnused = checkUnused;
	this->innerConfig = config;
	this->add(this->innerConfig);
}

ConfigurationView::~ConfigurationView() {
	if (!checkUnused)
		return;

	std::vector<std::string> unusedKeys;
	this->getUnusedKeys(unusedKeys);

	if (unusedKeys.size() > 0)
		this->openhat->unusedConfigKeysDetected(this->sourceFile, this->section, unusedKeys);
}

void ConfigurationView::addUsedKey(const std::string & key) {
	this->usedKeys.insert(key);
}

bool ConfigurationView::getRaw(const std::string& key, std::string& value) const
{
	Poco::Util::LayeredConfiguration::getRaw(key, value);
	if (this->innerConfig->has(key)) {
		if (!section.empty())
			this->openhat->logConfigKeyAccess((this->sourceFile.empty() ? std::string() : this->sourceFile + ": " ) + "Retrieved setting " + section + "." + key + ", value is: '" + value + "'");
		this->usedKeys.insert(key);
		return true;
	} else
		if (!section.empty())
			this->openhat->logConfigKeyAccess((this->sourceFile.empty() ? std::string() : this->sourceFile + ": ") + "Setting " + section + "." + key + " not specified");
	return false;
}

void ConfigurationView::getUnusedKeys(std::vector<std::string>& unusedKeys) {
	unusedKeys.clear();
	std::vector<std::string> keys;
	this->innerConfig->keys(keys);
	// add all keys except used ones
	auto ite = keys.cend();
	for (auto it = keys.cbegin(); it != ite; ++it)
		if (this->usedKeys.find(*it) == this->usedKeys.end())
			unusedKeys.push_back(*it);
}

void ConfigurationView::setCheckUnused(bool check) {
	this->checkUnused = check;
}

}
