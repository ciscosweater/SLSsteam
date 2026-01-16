#pragma once

#include <cstdint>
#include <unordered_map>


namespace FakeAppIds
{
	extern uint32_t launchedApp;
	extern std::unordered_map<uint32_t, uint32_t> fakeAppIdMap;
	uint32_t getFakeAppId(uint32_t appId);
	uint32_t getRealAppIdForCurrentPipe(bool fallback = true);

	void setAppIdForCurrentPipe(uint32_t& appId);
	void pipeLoop(bool post);

	void overwriteControllerAppIdIfNecessary(uint32_t& appId);
}
