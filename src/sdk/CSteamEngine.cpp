#include "CSteamEngine.hpp"


IClientUser* CSteamEngine::getSteamUser()
{
	return getInterface<IClientUser*>(0x1894);
}
