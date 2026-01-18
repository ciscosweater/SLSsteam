#include "fakeappid.hpp"

#include "../config.hpp"

#include "../sdk/CSteamEngine.hpp"
#include "../sdk/CSteamMatchmakingServers.hpp"
#include "../sdk/CUser.hpp"
#include "../sdk/IClientUtils.hpp"

uint32_t FakeAppIds::launchedApp = 0;
std::unordered_map<uint32_t, uint32_t> FakeAppIds::fakeAppIdMap = std::unordered_map<uint32_t, uint32_t>();
std::unordered_map<uint32_t, uint32_t> FakeAppIds::fakeAppIdMapServer = std::unordered_map<uint32_t, uint32_t>();

uint32_t FakeAppIds::getFakeAppId(uint32_t appId)
{
	auto fakeAppIds = g_config.fakeAppIds.get();

	if (fakeAppIds.contains(appId))
	{
		return fakeAppIds[appId];
	}
	else if (fakeAppIds.contains(0) && !g_pSteamEngine->getUser(0)->checkAppOwnership(appId))
	{
		return fakeAppIds[0];
	}

	return 0;
}

uint32_t FakeAppIds::getRealAppIdForCurrentPipe(bool fallback)
{
	uint32_t hPipe = *g_pClientUtils->getPipeIndex();
	if (fakeAppIdMap.contains(hPipe))
	{
		return fakeAppIdMap[hPipe];
	}

	if (fallback)
	{
		return g_pClientUtils->getAppId();
	}

	return 0;
}

void FakeAppIds::setAppIdForCurrentPipe(uint32_t& appId)
{
	if (launchedApp != appId)
	{
		return;
	}

	launchedApp = 0;

	//Keep track of every AppId, for various reasons
	fakeAppIdMap[*g_pClientUtils->getPipeIndex()] = appId;

	//Do not change Steam Client itself (AppId 0)
	if (!appId)
	{
		return;
	}

	uint32_t newAppId = getFakeAppId(appId);
	if (newAppId)
	{
		g_pLog->once("Changing AppId of %u\n", appId);
		appId = newAppId;
	}
}

void FakeAppIds::pipeLoop(bool post)
{
	uint32_t appId = getRealAppIdForCurrentPipe();
	uint32_t fakeAppId = getFakeAppId(appId);

	if (!appId || !fakeAppId || appId == fakeAppId)
	{
		return;
	}

	if (post)
	{
		appId = fakeAppId;
	}

	g_pLog->debug("Setting AppId to %u in pipe %p\n", appId, *g_pClientUtils->getPipeIndex());
	g_pSteamEngine->setAppIdForCurrentPipe(appId);
}

void FakeAppIds::overwriteControllerAppIdIfNecessary(uint32_t& appId)
{
	//Don't use getRealAppid here! Returns 0
	const uint32_t fake = getFakeAppId(appId);

	if (!appId || !fake || appId == fake)
	{
		return;
	}

	//Exclude Steam Big Picture mode & Steam Controller Configs - Desktop. Causes issues with SteamInput
	if(appId == 769 || appId == 413080)
	{
		return;
	}

	g_pLog->debug("FakeAppIds::overwriteAppIdIfNecessary overwriting %u with %u\n", appId, fake);
	appId = fake;
}


void FakeAppIds::getServerDetails(uint32_t handle, gameserverdetails_t& details)
{
	if (!fakeAppIdMapServer.contains(handle))
	{
		return;
	}

	details.appId = fakeAppIdMapServer[handle];
	g_pLog->debug("Changing appId back to %u\n", fakeAppIdMapServer[handle]);
}

uint32_t FakeAppIds::requestInternetServerList(uint32_t appId)
{
	const uint32_t fake = getFakeAppId(appId);
	if (!fake)
	{
		return 0;
	}

	g_pLog->debug("Replacing %u with %u\n", appId, fake);
	return fake;
}
