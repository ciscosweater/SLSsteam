#include "apps.hpp"

#include "../sdk/CUser.hpp"
#include "../sdk/IClientApps.hpp"

#include "../config.hpp"
#include "../globals.hpp"

bool Apps::applistRequested;
std::map<uint32_t, int> Apps::appIdOwnerOverride;

bool Apps::checkAppOwnership(uint32_t appId, CAppOwnershipInfo* pInfo)
{
	//Wait Until GetSubscribedApps gets called once to let Steam request and populate legit data first.
	//Afterwards modifying should hopefully not affect false positives anymore
	if (!applistRequested || !pInfo || !g_currentSteamId)
	{
		return false;
	}

	uint32_t ownerOverride = g_currentSteamId;
	const uint32_t denuvoOwner = g_config.getDenuvoGameOwner(appId);

	//Do not modify Denuvo enabled Games
	if (denuvoOwner && denuvoOwner != g_currentSteamId)
	{
		if (g_config.denuvoSpoof)
		{
			ownerOverride = denuvoOwner;
		}
		else
		{
			//Would love to log the SteamId, but for users anonymity I won't
			g_pLog->once("Skipping %u because it's a Denuvo game from someone else\n", appId);
			return false;
		}
	}

	//Doing that might be not worth it since this will most likely be easier to mantain
	//TODO: Backtrace those 4 calls and only patch the really necessary ones since this might be prone to breakage
	if (g_config.disableFamilyLock && appIdOwnerOverride.count(appId) && appIdOwnerOverride.at(appId) < 4)
	{
		ownerOverride = 1;
		appIdOwnerOverride[appId]++;
	}

	if (!g_config.shouldExcludeAppId(appId) && (g_config.isAddedAppId(appId) || (g_config.playNotOwnedGames && !pInfo->purchased)))
	{
		//Changing the purchased field is enough, but just for nicety in the Steamclient UI we change the owner too
		pInfo->ownerSteamId = ownerOverride;
		pInfo->familyShared = ownerOverride != g_currentSteamId;

		pInfo->purchased = true;
		//Unnessecary but whatever
		pInfo->permanent = !pInfo->familyShared;

		//Found in backtrace
		pInfo->releaseState = 4;
		pInfo->field10_0x25 = 0;
		//Seems to do nothing in particular, some dlc have this as 1 so I uncomented this for now. Might be free stuff?
		//pOwnershipInfo->field27_0x36 = 1;

		//This stopped working after some steam update!
		//Now GetSubscribedApps will only get called once and then the amount of Apps returned
		//will be stored somewhere instead of calling it over and over again.
		//g_config.addAdditionalAppId(appId);
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
	return !g_pUser->checkAppOwnership(appId);
}

bool Apps::shouldDisableCDKey(uint32_t appId)
{
	return !g_pUser->checkAppOwnership(appId);
}

bool Apps::shouldDisableUpdates(uint32_t appId)
{
	//Using AdditionalApps here aswell so users can manually block updates
	return g_config.isAddedAppId(appId) || !g_pUser->checkAppOwnership(appId);
}
