#include "CUser.hpp"

#include "CAppOwnershipInfo.hpp"

#include "../hooks.hpp"
#include "../patterns.hpp"


bool CUser::checkAppOwnership(uint32_t appId, CAppOwnershipInfo* pInfo)
{
	return Hooks::CUser_CheckAppOwnership.tramp.fn(this, appId, pInfo);
}

bool CUser::checkAppOwnership(uint32_t appId)
{
	CAppOwnershipInfo info {};
	return checkAppOwnership(appId, &info) && info.purchased;
}

void CUser::updateAppOwnershipTicket(uint32_t appId, void* pTicket, uint32_t len)
{
	const static auto fn = reinterpret_cast<void(*)(void*, uint32_t, void*, uint32_t)>(Patterns::CUser::UpdateAppOwnershipTicket.address);
	fn(this, appId, pTicket, len);
}
