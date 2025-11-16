#include "config.hpp"
#include "feats/ticket.hpp"
#include "globals.hpp"
#include "hooks.hpp"
#include "log.hpp"
#include "patterns.hpp"
#include "utils.hpp"

#include "libmem/libmem.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <link.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

static const char* EXPECTED_STEAMCLIENT_HASH = "ec96ee3c700e759c118bfe308e1fc707ec3a8e4f6bf2bbb117df19b5b730b62d";

static bool cleanEnvVar(const char* varName)
{
	char* var = getenv(varName);
	if (var == NULL)
		return false;

	auto splits = Utils::strsplit(var, ":");
	auto newEnv = std::string();

	for(unsigned int i = 0; i < splits.size(); i++)
	{
		auto split = splits.at(i);
		if (split.ends_with("SLSsteam.so"))
		{
			g_pLog->debug("Removed SLSsteam.so from $%s\n", varName);
			continue;
		}

		if(newEnv.size() > 0)
		{
			newEnv.append(":");
		}
		newEnv.append(split);
	}

	setenv(varName, newEnv.c_str(), true);
	//g_pLog->debug("Set %s to %s\n", varName, newEnv.c_str());

	return true;
}

static bool verifySteamClientHash()
{

	auto path = std::filesystem::path(g_modSteamClient.path);
	auto dir = path.parent_path();

	g_pLog->info
	(
		"steamclient.so loaded from %s/%s at %p to %p\n",
		dir.filename().c_str(),
		path.filename().c_str(),
		g_modSteamClient.base,
		g_modSteamClient.end
	);

	try
	{
		std::string sha256 = Utils::getFileSHA256(path.c_str());
		g_pLog->info("steamclient.so hash is %s\n", sha256.c_str());

		//TODO: Research if there's a better way to compare const char* to std::string
		return strcmp(sha256.c_str(), EXPECTED_STEAMCLIENT_HASH) == 0;
	}
	catch(std::runtime_error& err)
	{
		g_pLog->debug("Unable to read steamclient.so hash!\n");
		return false;
	}
}

//Looking at /proc/self/maps it seems like this isn't needed for processes that aren't steam
//__attribute__((noreturn))
static void unload()
{
	Hooks::remove();

	//This is absolutely unnessecary for applications loading SLSsteam where it cancels from setup()
	//Would be nice to run have for failed load() attempts though 
	//lm_module_t mod;
	//if (LM_FindModule("SLSsteam.so", &mod))
	//{
	//	//TODO: Investigate crash ?
	//	//Possibly: Might be because we're unmapping what ever thread we're running in
	//	//munmap(reinterpret_cast<void*>(mod.base), mod.size);
	//}
	//exit(0);
}

//TODO: Remove when unload() works properly since it should not be needed anymore after that
static bool setupSuccess = false;

static void setup()
{
	lm_process_t proc {};
	if (!LM_GetProcess(&proc))
	{
		unload();
		return;
	}

	//Do not do anything in other processes
	if (strcmp(proc.name, "steam") != 0)
	{
		unload();
		return;
	}

	g_pLog = std::unique_ptr<CLog>(CLog::createDefaultLog());
	if (!g_pLog)
	{
		unload();
		return;
	}

	g_pLog->debug("SLSsteam loading in %s\n", proc.name);

	cleanEnvVar("LD_AUDIT");
	//TODO: Investigate weird logging. Not like it's necessary anymore
	//cleanEnvVar("LD_PRELOAD");

	if(!g_config.init())
	{
		unload();
		return;
	}

	//Since we can't statically link everything and some distros seem to respect LD_LIBRARY_PATH
	//more or less than mine does we just force append those
	//Hopefully this won't mess anything else up
	auto ldLibPath = std::string(getenv("LD_LIBRARY_PATH"));
	ldLibPath.append("/usr/lib:/usr/lib32");
	setenv("LD_LIBRARY_PATH", ldLibPath.c_str(), true);

	setupSuccess = true;
}

static void load()
{
	if (!setupSuccess)
	{
		return;
	}

	//This should never happen, but better be safe than sorry in case I refactor someday
	if (!LM_FindModule("steamclient.so", &g_modSteamClient))
	{
		unload();
		return;
	}

	if (!verifySteamClientHash())
	{
		if (g_config.safeMode)
		{
			g_pLog->warn("Unknown steamclient.so hash! Aborting...");
			unload();
			return;
		}
		else if (g_config.warnHashMissmatch)
		{
			g_pLog->warn("steamclient.so hash missmatch! Please update :)");
		}
	}

	if (!Patterns::init())
	{
		g_pLog->warn("Failed to find all patterns! Aborting...");
		return;
	}

	if (!Hooks::setup())
	{
		unload();
		return;
	}

	if (g_config.notifyInit)
	{
		const auto now = std::chrono::time_point{std::chrono::system_clock::now()};
		const auto ymd = std::chrono::year_month_day{std::chrono::floor<std::chrono::days>(now)};

		//Funsy easter egg :)
		if (static_cast<unsigned int>(ymd.month()) == 2 && static_cast<unsigned int>(ymd.day()) == 22)
		{
			g_pLog->notify("Happy birthday SLSsteam!");
		}
		else
		{
			g_pLog->notify("Loaded successfully");
		}
	}
}

unsigned int la_version(unsigned int)
{
	return LAV_CURRENT;
}

unsigned int la_objopen(struct link_map *map, __attribute__((unused)) Lmid_t lmid, __attribute__((unused)) uintptr_t *cookie)
{
	if (std::string(map->l_name).ends_with("/steamclient.so"))
	{
		load();
	}

	return 0;
}

void la_preinit(__attribute__((unused)) uintptr_t *cookie)
{
	setup();
}
