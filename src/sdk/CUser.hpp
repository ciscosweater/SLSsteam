#pragma once

class CAppOwnershipInfo;

#include <cstdint>

class CUser
{
public:
	bool checkAppOwnership(uint32_t appId, CAppOwnershipInfo* pInfo);
	bool checkAppOwnership(uint32_t appId);

	void updateAppOwnershipTicket(uint32_t appId, void* pTicket, uint32_t len);
};
