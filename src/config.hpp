#pragma once

#include "log.hpp"

#include "yaml-cpp/exceptions.h"
#include "yaml-cpp/node/node.h"
#include "yaml-cpp/yaml.h"

#include <cstdint>
#include <cstdio>
#include <pthread.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

class CConfig {
public:
	class CDlcData
	{
	public:
		uint32_t parentId;
		std::unordered_map<uint32_t, std::string> dlcIds;
		//No default constructor, otherwise dlcData will complain that no matching one was found
		//without implementing it ourself anyway
	};

	std::unordered_set<uint32_t> appIds;
	std::unordered_set<uint32_t> addedAppIds;
	std::unordered_map<uint32_t, CDlcData> dlcData;
	std::unordered_set<uint32_t> fakeOffline;
	std::unordered_map<uint32_t, uint32_t> fakeAppIds;

	//SteamId, AppIds tuple
	std::unordered_map<uint32_t, std::unordered_set<uint32_t>> denuvoGames;
	bool blockEncryptedAppTickets;
	bool denuvoSpoof;

	bool disableFamilyLock;
	bool useWhiteList;
	bool automaticFilter;
	bool playNotOwnedGames;
	bool safeMode;
	bool notifications;
	bool warnHashMissmatch;
	bool notifyInit;
	unsigned int logLevel;
	bool extendedLogging;

	std::string getDir();
	std::string getPath();
	bool createFile();
	bool init();

	bool loadSettings();

	template<typename T>
	T getSetting(YAML::Node& node, const char* name, T defVal)
	{
		if (!node[name])
		{
			g_pLog->notifyLong("Missing %s in configfile! Using default", name);
			return defVal;
		}

		try
		{
			 return node[name].as<T>();
		}
		catch (YAML::BadConversion& er)
		{
			g_pLog->notify("Failed to parse value of %s! Using default\n", name);
			return defVal;
		}
	};

	template<typename T>
	std::unordered_set<T> getList(YAML::Node& rootNode, const char* name)
	{
		auto list = std::unordered_set<T>();

		const auto node = rootNode[name];
		if (!node)
		{
			g_pLog->notify("Missing %s entry in config!", name);
			return list;
		}

		for(auto subNode : node)
		{
			try
			{
				T val = subNode.as<T>();
				list.emplace(val);

				//TODO: Find better way to log shit
				if (std::is_same_v<T, uint32_t>)
				{
					g_pLog->debug("Added %u to %s\n", val, name);
				}
			}
			catch(...)
			{
				g_pLog->notify("Failed to parse %s in %s!", subNode.as<std::string>().c_str(), name);
			}
		}

		return list;
	}

	bool isAddedAppId(uint32_t appId);
	bool addAdditionalAppId(uint32_t appId);

	bool shouldExcludeAppId(uint32_t appId);
	uint32_t getDenuvoGameOwner(uint32_t appId);
};

extern CConfig g_config;
