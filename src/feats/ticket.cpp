#include "ticket.hpp"

#include "../config.hpp"
#include "../globals.hpp"

#include "../sdk/IClientUtils.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>

uint32_t Ticket::oneTimeSteamIdSpoof = 0;
uint32_t Ticket::tempSteamIdSpoof = 0;
std::map<uint32_t, CEncryptedAppTicket> Ticket::encryptedTicketMap = std::map<uint32_t, CEncryptedAppTicket>();

std::string Ticket::getTicketDir()
{
	std::stringstream ss;
	ss << g_config.getDir().c_str() << "/cache";

	const auto dir = ss.str();
	if (!std::filesystem::exists(dir.c_str()))
	{
		std::filesystem::create_directory(dir.c_str());
	}

	return ss.str();
}

std::string Ticket::getTicketPath(uint32_t appId)
{
	std::stringstream ss;
	ss << getTicketDir().c_str() << "/ticket_" << appId;

	return ss.str();
}

CAppTicket Ticket::getCachedTicket(uint32_t appId)
{
	CAppTicket ticket {};

	const auto path = getTicketPath(appId);
	if (!std::filesystem::exists(path.c_str()))
	{
		return ticket;
	}

	std::ifstream ifs(path, std::ios::in);

	g_pLog->debug("Reading ticket for %u\n", appId);

	ifs.read(reinterpret_cast<char*>(&ticket), sizeof ticket);
	//g_pLog->debug("Ticket: %u, %u, %u\n", ticket.getSteamId(), ticket.getAppId(), ticket.getSize());

	return ticket;
}

bool Ticket::saveTicketToCache(uint32_t appId, void* ticketData, uint32_t ticketSize, uint32_t* a4)
{
	CAppTicket ticket {};
	g_pLog->debug("Saving ticket for %u...\n", appId);

	//steamId is in ticket too, but whatever
	memcpy(ticket.bytes, ticketData, ticketSize);
	memcpy(ticket.extraData, a4, sizeof(ticket.extraData));

	const auto path = getTicketPath(appId);
	std::ofstream ofs(path.c_str(), std::ios::out);

	ofs.write(reinterpret_cast<char*>(&ticket), sizeof(ticket));

	g_pLog->once("Saved ticket for %u\n", appId);
	
	return true;
}

uint32_t Ticket::getTicketOwnershipExtendedData(uint32_t appId, void* ticket, uint32_t ticketSize, uint32_t* a4)
{
	if (ticketSize)
	{
		saveTicketToCache(appId, ticket, ticketSize, a4);
		return 0;
	}

	const CAppTicket cached = Ticket::getCachedTicket(appId);
	const uint32_t size = cached.getSize();
	if (!size)
	{
		return 0;
	}

	oneTimeSteamIdSpoof = cached.getSteamId();

	memcpy(ticket, cached.bytes, size);
	memcpy(a4, cached.extraData, sizeof(cached.extraData));

	return size;
}

std::string Ticket::getEncryptedTicketPath(uint32_t appId)
{
	std::stringstream ss;
	ss << getTicketDir().c_str() << "/encryptedTicket_" << appId;

	return ss.str();
}

CEncryptedAppTicket Ticket::getCachedEncryptedTicket(uint32_t appId)
{
	if (encryptedTicketMap.contains(appId))
	{
		return encryptedTicketMap[appId];
	}

	CEncryptedAppTicket ticket {};

	const auto path = getEncryptedTicketPath(appId);
	if (!std::filesystem::exists(path.c_str()))
	{
		return ticket;
	}

	std::ifstream ifs(path, std::ios::in);

	g_pLog->debug("Reading encrypted ticket for %u\n", appId);

	ifs.read(reinterpret_cast<char*>(&ticket), sizeof ticket);
	//g_pLog->debug("Ticket: %u, %u, %u\n", ticket.getSteamId(), ticket.getAppId(), ticket.getSize());

	encryptedTicketMap[appId] = ticket;

	return ticket;
}

bool Ticket::saveEncryptedTicketToCache(uint32_t appId, uint32_t steamId, void* ticketData, uint32_t ticketSize)
{
	CEncryptedAppTicket ticket {};
	g_pLog->debug("Saving encrypted ticket for %u...\n", appId);

	ticket.steamId = steamId;
	ticket.size = ticketSize;
	memcpy(ticket.bytes, ticketData, ticketSize);

	const auto path = getEncryptedTicketPath(appId);
	std::ofstream ofs(path.c_str(), std::ios::out);

	ofs.write(reinterpret_cast<char*>(&ticket), sizeof(ticket));

	g_pLog->once("Saved encrypted ticket for %u\n", appId);

	encryptedTicketMap[appId] = ticket;
	
	return true;
}

bool Ticket::getEncryptedAppTicket(void* ticketData, uint32_t* bytesWritten)
{
	if (!ticketData || !bytesWritten)
	{
		//This shouldn't happen, but some games seem to do it for some reason (possible a pitfall idk)
		return false;
	}

	bool cached = false;

	if (*bytesWritten)
	{
		saveEncryptedTicketToCache(g_pClientUtils->getAppId(), g_currentSteamId, ticketData, *bytesWritten);
		cached = true;
	}

	if (g_config.blockEncryptedAppTickets)
	{
		memset(ticketData, 0, 0x1000);
		*bytesWritten = 0;
	}

	if (cached)
	{
		return false;
	}

	CEncryptedAppTicket ticket = Ticket::getCachedEncryptedTicket(g_pClientUtils->getAppId());
	//TODO: Add isValid helper function to ticket or similiar
	if (!ticket.size || !ticket.steamId)
	{
		return false;
	}

	tempSteamIdSpoof = ticket.steamId;
	memcpy(ticketData, ticket.bytes, ticket.size);
	*bytesWritten = ticket.size;

	return true;
}

bool Ticket::getAPICallResult(ECallbackType type, void* pCallback)
{
	if (type != ECallbackType::RequestEncryptedAppOwnershipTicket)
	{
		return false;
	}

	uint32_t* pResult = reinterpret_cast<uint32_t*>(pCallback);

	if (*pResult == 1)
	{
		return false;
	}

	CEncryptedAppTicket ticket = getCachedEncryptedTicket(g_pClientUtils->getAppId());
	if (!ticket.size || !ticket.steamId)
	{
		return false;
	}

	*pResult = 1; //Success
	g_pLog->debug("Spoofed RequestEncryptedAppOwnershipTicket callback!\n");

	return true;
}

