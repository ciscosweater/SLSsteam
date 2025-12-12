#include "fakeappid.hpp"

#include "../config.hpp"

#include "../sdk/CSteamEngine.hpp"
#include "../sdk/CUser.hpp"
#include "../sdk/IClientUtils.hpp"


std::unordered_map<uint32_t, uint32_t> FakeAppIds::fakeAppIdMap = std::unordered_map<uint32_t, uint32_t>();

uint32_t FakeAppIds::getFakeAppId(uint32_t appId)
{
	if (g_config.fakeAppIds.contains(appId))
	{
		return g_config.fakeAppIds[appId];
	}
	else if (g_config.fakeAppIds.contains(0) && !g_pSteamEngine->getUser(0)->checkAppOwnership(appId))
	{
		return g_config.fakeAppIds[0];
	}

	return 0;
}

uint32_t FakeAppIds::getRealAppId()
{
	uint32_t hPipe = *g_pClientUtils->getPipeIndex();
	if (fakeAppIdMap.contains(hPipe))
	{
		return fakeAppIdMap[hPipe];
	}

	return g_pClientUtils->getAppId();
}

void FakeAppIds::setAppIdForCurrentPipe(uint32_t& appId)
{
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
	uint32_t appId = getRealAppId();
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
