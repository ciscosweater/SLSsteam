#include "IClientUser.hpp"

#include "../hooks.hpp"
#include "../patterns.hpp"


bool IClientUser::isLoggedOn()
{
	return Hooks::IClientUser_BLoggedOn.tramp.fn(this);
}

uint32_t IClientUser::updateOwnershipInfo(uint32_t appId, bool staleOnly)
{
	//Call hook to make ticket cache cooperate
	return Hooks::IClientUser_BUpdateAppOwnershipInfo.hookFn.fn(this, appId, staleOnly);
}

IClientUser* g_pClientUser;
