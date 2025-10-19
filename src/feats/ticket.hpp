#pragma once

#include "../sdk/CAppTicket.hpp"
#include "../sdk/CCallback.hpp"

#include <cstdint>
#include <map>
#include <string>


namespace Ticket
{
	extern uint32_t steamIdSpoof;
	extern std::map<uint32_t, CEncryptedAppTicket> encryptedTicketMap;

	std::string getTicketDir();

	//TODO: Fill with error checks
	std::string getTicketPath(uint32_t appId);
	CAppTicket getCachedTicket(uint32_t appId);
	bool saveTicketToCache(uint32_t appId, void* ticketData, uint32_t ticketSize, uint32_t* a4);

	uint32_t getTicketOwnershipExtendedData(uint32_t appId, void* ticket, uint32_t ticketSize, uint32_t* a4);//, uint32_t* a5, uint32_t* a6, uint32_t* a7);


	std::string getEncryptedTicketPath(uint32_t appId);
	CEncryptedAppTicket getCachedEncryptedTicket(uint32_t appId);
	bool saveEncryptedTicketToCache(uint32_t appId, uint32_t steamId, void* ticketData, uint32_t ticketSize);

	bool getEncryptedAppTicket(void* ticketData, uint32_t* bytesWritten);
	bool getAPICallResult(ECallbackType type, void* pCallback);
}
