#include "dlc.hpp"

#include "../sdk/CSteamEngine.hpp"
#include "../sdk/CUser.hpp"
#include "../sdk/IClientUtils.hpp"

#include "../config.hpp"


bool DLC::shouldUnlockDlc(uint32_t appId)
{
	if (!g_pClientUtils->getAppId())
	{
		return false;
	}

	if (g_config.shouldExcludeAppId(appId))
	{
		return false;
	}

	if (g_pSteamEngine->getUser(0)->checkAppOwnership(appId))
	{
		return false;
	}
	
	return true;
}

bool DLC::isDlcEnabled(uint32_t appId)
{
	return shouldUnlockDlc(appId);
}

bool DLC::isSubscribed(uint32_t appId)
{
	return shouldUnlockDlc(appId);
}

bool DLC::isAppDlcInstalled(uint32_t appId)
{
	return shouldUnlockDlc(appId);
}

bool DLC::userSubscribedInTicket(uint32_t appId)
{
	//Might want to compare the steamId param to the g_currentSteamId in the future
	//Although not doing that might also work for Dedicated servers?
	return shouldUnlockDlc(appId);
}

uint32_t DLC::getDlcCount(uint32_t appId)
{
	if (g_config.dlcData.contains(appId))
	{
		return g_config.dlcData[appId].dlcIds.size();
	}

	return 0;
}

bool DLC::getDlcDataByIndex(uint32_t appId, int index, uint32_t* dlcId, bool* available, char* dlcName, size_t& dlcNameLen)
{
	if (!dlcId || !available || !dlcName)
	{
		return false;
	}

	if (g_config.dlcData.contains(appId))
	{
		auto& data = g_config.dlcData[appId];
		auto dlc = std::next(data.dlcIds.begin(), index);

		*dlcId = dlc->first;
		*available = true;

		//No clue if we have to check for errors during printf since the devs hopefully didn't fuck
		//up the dlcNameLen. Who knows though
		snprintf(dlcName, dlcNameLen, "%s", dlc->second.c_str());

		return true;
	}
	else if (!g_config.shouldExcludeAppId(*dlcId))
	{
		*available = true;
	}

	return false;
}
