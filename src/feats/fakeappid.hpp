#pragma once

#include <cstdint>
#include <unordered_map>


namespace FakeAppIds
{
	extern std::unordered_map<uint32_t, uint32_t> fakeAppIdMap;
	uint32_t getFakeAppId(uint32_t appId);
	uint32_t getRealAppId();

	void setAppIdForCurrentPipe(uint32_t& appId);
	void pipeLoop(bool post);

	void overwriteAppIdIfNecessary(uint32_t& appId);
}
