#include "apps.hpp"

#include "../sdk/CAppOwnershipInfo.hpp"
#include "../sdk/CSteamEngine.hpp"
#include "../sdk/CUser.hpp"
#include "../sdk/IClientApps.hpp"

#include "../config.hpp"
#include "../globals.hpp"


bool Apps::applistRequested;
std::map<uint32_t, int> Apps::appIdOwnerOverride;

bool Apps::unlockApp(uint32_t appId, CAppOwnershipInfo* info, uint32_t ownerId)
{
	//Changing the purchased field is enough, but just for nicety in the Steamclient UI we change the owner too
	info->ownerSteamId = ownerId;
	info->familyShared = ownerId != g_currentSteamId;

	info->purchased = true;
	//Unnessecary but whatever
	info->permanent = !info->familyShared;

	//Found in backtrace
	info->releaseState = 4;
	info->field10_0x25 = 0;
	//Seems to do nothing in particular, some dlc have this as 1 so I uncomented this for now. Might be free stuff?
	//pOwnershipInfo->field27_0x36 = 1;

	//g_pLog->debug("Unlocked %u for %u\n", appId, ownerId);
	g_pLog->debug("Unlocked %u\n", appId);
	return true;
}

bool Apps::unlockApp(uint32_t appId, CAppOwnershipInfo* info)
{
	return unlockApp(appId, info, g_currentSteamId);
}

bool Apps::checkAppOwnership(uint32_t appId, CAppOwnershipInfo* pInfo)
{
	//Wait Until GetSubscribedApps gets called once to let Steam request and populate legit data first.
	//Afterwards modifying should hopefully not affect false positives anymore
	if (!applistRequested || !pInfo || !g_currentSteamId)
	{
		return false;
	}

	const uint32_t denuvoOwner = g_config.getDenuvoGameOwner(appId);

	//Do not modify Denuvo enabled Games
	if (denuvoOwner && denuvoOwner != g_currentSteamId)
	{
		//Would love to log the SteamId, but for users anonymity I won't
		g_pLog->once("Skipping %u because it's a Denuvo game from someone else\n", appId);
		return false;
	}

	//TODO: Backtrace those 4 calls and only patch the really necessary ones since this might be prone to breakage
	//Edit: Not worth it.
	if (g_config.disableFamilyLock && appIdOwnerOverride.contains(appId) && appIdOwnerOverride.at(appId) < 4)
	{
		unlockApp(appId, pInfo, 1);
		appIdOwnerOverride[appId]++;
	}
	else if (!g_config.shouldExcludeAppId(appId) && (g_config.isAddedAppId(appId) || (g_config.playNotOwnedGames && !pInfo->purchased)))
	{
		unlockApp(appId, pInfo);
	}

	//Returning false after we modify data shouldn't cause any problems because it should just get discarded
	if (!g_pClientApps)
		return false;

	auto type = g_pClientApps->getAppType(appId);
	if (type == APPTYPE_DLC) //Don't touch DLC here, otherwise downloads might break. Hopefully this won't decrease compatibility
	{
		return false;
	}

	if (g_config.automaticFilter)
	{
		switch(type)
		{
			case APPTYPE_APPLICATION:
			case APPTYPE_GAME:
				break;

			default:
				return false;
		}
	}

	return true;
}

void Apps::getSubscribedApps(uint32_t* appList, size_t size, uint32_t& count)
{
	//Valve calls this function twice, once with size of 0 then again
	if (!size || !appList)
	{
		count = count + g_config.addedAppIds.size();
		return;
	}

	//TODO: Maybe Add check if AppId already in list before blindly appending
	for(auto& appId : g_config.addedAppIds)
	{
		appList[count++] = appId;
	}

	applistRequested = true;
}

void Apps::launchApp(uint32_t appId)
{
	appIdOwnerOverride[appId] = 0;
}

bool Apps::shouldDisableCloud(uint32_t appId)
{
	return !g_pSteamEngine->getUser(0)->checkAppOwnership(appId);
}

bool Apps::shouldDisableCDKey(uint32_t appId)
{
	return !g_pSteamEngine->getUser(0)->checkAppOwnership(appId);
}

bool Apps::shouldDisableUpdates(uint32_t appId)
{
	//Using AdditionalApps here aswell so users can manually block updates
	return g_config.isAddedAppId(appId) || !g_pSteamEngine->getUser(0)->checkAppOwnership(appId);
}
