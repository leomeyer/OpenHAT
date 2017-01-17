#pragma once

#include <map>
#include <istream>
#include <unordered_set>

#include "Poco/Util/IniFileConfiguration.h"
#include "Poco/Util/LayeredConfiguration.h"

namespace openhat {

/// Wrapper around IniFileConfiguration that can translate the file contents given the
/// parameters supplied in the constructor.
class OpenHATConfigurationFile : public Poco::Util::IniFileConfiguration
{
protected:
	/** Override getRaw to allow values in quotes. Quotes are removed before returning the value.
	*   This allows whitespace to appear in front or at the end of values.
	*/
	virtual bool getRaw(const std::string & key, std::string & value) const;

public:
	OpenHATConfigurationFile(const std::string& path, std::map<std::string, std::string> parameters);
};

class AbstractOpenHAT;

/// Wrapper around a configuration view that tracks all settings that have been retrieved
/// from the configuration and can compare them against all supplied settings, to identify
/// unused or misspelled configuration parameters.
class ConfigurationView : public Poco::Util::LayeredConfiguration {

public:
	ConfigurationView(AbstractOpenHAT* openhat, Poco::AutoPtr<Poco::Util::AbstractConfiguration> config, const std::string& sourceFile, const std::string& section, bool checkUnused = true);

	~ConfigurationView();

	void addUsedKey(const std::string& key);

	void getUnusedKeys(std::vector<std::string>& unusedKeys);

	void setCheckUnused(bool check);

	const std::string sourceFile;
	const std::string section;
protected:
	AbstractOpenHAT* openhat;
	bool checkUnused;
	mutable Poco::AutoPtr<Poco::Util::AbstractConfiguration> innerConfig;
	mutable std::unordered_set<std::string> usedKeys;

	bool getRaw(const std::string& key, std::string& value) const;
};

};
