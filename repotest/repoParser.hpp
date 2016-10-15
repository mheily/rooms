#pragma once

#include <string>
#include <string>
#include "json.hpp"

// A room in the repository
class RepoRoom {
public:
	RepoRoom(const std::string& name, const nlohmann::json& json) {
		this->name = name;
		if (json.find("parent") != json.end()) {
			parent = json["parent"];
		} else {
			parent = "";
		}
		summary = json["summary"];
	}

        const std::string getName() const
	{
		return name;
	}

        const std::string getSummary() const
	{
		return summary;
	}

        const std::string getParent() const
	{
		return parent;
	}


private:
	std::string name;
	std::string parent;
	std::string summary;
};

// TODO: should be RepoManager instead
class RepoParser {
public:
	RepoParser();
	void parse(const std::string& path);
	RepoRoom* getRoom(const std::string& name) {
		return rooms[name];
	}	
	void installRoom(const std::string& name);
	void printSearchResult(const std::string& pattern);

private:
	// the base_uri when downloading rooms
	std::string baseUri;

	std::map<std::string, RepoRoom*> rooms;
	std::string repoJsonPath; // path to JSON repository metadata
	void parseJSON(nlohmann::json& result);
};
