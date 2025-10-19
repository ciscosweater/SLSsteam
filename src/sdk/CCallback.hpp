#pragma once

#include <cstdint>


enum ECallbackType : uint32_t
{
	//TODO: Add RequestUserStats to make own hook redundant
	RequestEncryptedAppOwnershipTicket = 0x9a,
};
