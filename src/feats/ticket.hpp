#pragma once

#include "../sdk/ECallback.hpp"

#include <cstdint>
#include <map>
#include <string>

class CAppTicket;
class CEncryptedAppTicket;
class CMsgAppOwnershipTicketResponse;

class CMsgClientRequestEncryptedAppTicketResponse;
class CProtoBufMsgBase;

namespace Ticket
{
	class SavedEncryptedTicket
	{
public:
		uint32_t steamId;
		std::string ticket;
	};

	extern uint32_t oneTimeSteamIdSpoof;
	extern uint32_t tempSteamIdSpoof;
	extern std::map<uint32_t, CAppTicket> ticketMap;
	extern std::map<uint32_t, SavedEncryptedTicket> encryptedTicketMap;

	std::string getTicketDir();

	//TODO: Fill with error checks
	std::string getTicketPath(uint32_t appId);
	CAppTicket getCachedTicket(uint32_t appId);
	bool saveTicketToCache(uint32_t appId, void* ticketData, uint32_t ticketSize);

	void recvAppOwnershipTicketResponse(CMsgAppOwnershipTicketResponse* resp);
	void launchApp(uint32_t appId);
	void getTicketOwnershipExtendedData(uint32_t appId);

	std::string getEncryptedTicketPath(uint32_t appId);
	SavedEncryptedTicket getCachedEncryptedTicket(uint32_t appId);
	bool saveEncryptedTicketToCache(CMsgClientRequestEncryptedAppTicketResponse* resp);
	void recvEncryptedAppTicket(CMsgClientRequestEncryptedAppTicketResponse* msg);
	void recvMsg(CProtoBufMsgBase* msg);
}
