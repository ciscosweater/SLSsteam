#include "ticket.hpp"

#include "../config.hpp"
#include "../globals.hpp"

#include "../sdk/CAppTicket.hpp"
#include "../sdk/CProtoBufMsgBase.hpp"
#include "../sdk/CSteamEngine.hpp"
#include "../sdk/CUser.hpp"
#include "../sdk/IClientUtils.hpp"
#include "../sdk/EResult.hpp"

#include "base64/base64.hpp"
#include "yaml-cpp/binary.h"
#include "yaml-cpp/emitter.h"
#include "yaml-cpp/emittermanip.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>

uint32_t Ticket::oneTimeSteamIdSpoof = 0;
uint32_t Ticket::tempSteamIdSpoof = 0;
std::map<uint32_t, CAppTicket> Ticket::ticketMap = std::map<uint32_t, CAppTicket>();
std::map<uint32_t, Ticket::SavedEncryptedTicket> Ticket::encryptedTicketMap = std::map<uint32_t, SavedEncryptedTicket>();

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
	if (ticketMap.contains(appId))
	{
		return ticketMap[appId];
	}

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
	
	ticketMap[appId] = ticket;

	return ticket;
}

bool Ticket::saveTicketToCache(uint32_t appId, void* ticketData, uint32_t ticketSize)
{
	CAppTicket ticket {};
	g_pLog->debug("Saving ticket for %u...\n", appId);

	memcpy(&ticket, ticketData, ticketSize);

	const auto path = getTicketPath(appId);
	std::ofstream ofs(path.c_str(), std::ios::out);

	ofs.write(reinterpret_cast<char*>(&ticket), sizeof(ticket));

	g_pLog->once("Saved ticket for %u\n", appId);

	ticketMap[appId] = ticket;
	
	return true;
}

void Ticket::recvAppOwnershipTicketResponse(CMsgAppOwnershipTicketResponse* resp)
{
	if (resp->result != ERESULT_OK)
	{
		return;
	}

	//Very weird way to store data, thanks GCC
	//TODO: Maybe dig in further to improve my classes
	uint32_t* pSize = (reinterpret_cast<uint32_t*>(*resp->ppTicket) - 3);
	saveTicketToCache(resp->appId, *resp->ppTicket, *pSize);
}

void Ticket::launchApp(uint32_t appId)
{
	auto ticket = getCachedTicket(appId);
	if (!ticket.steamId)
	{
		return;
	}

	g_pSteamEngine->getUser(0)->updateAppOwnershipTicket(appId, reinterpret_cast<void*>(&ticket), ticket.getSize());
	g_pLog->once("Force loaded AppOwnershipTicket for %i\n", appId);
}

void Ticket::getTicketOwnershipExtendedData(uint32_t appId)
{
	const CAppTicket cached = Ticket::getCachedTicket(appId);
	const uint32_t steamId = cached.steamId;
	if (!steamId)
	{
		return;
	}

	oneTimeSteamIdSpoof = steamId;
}

std::string Ticket::getEncryptedTicketPath(uint32_t appId)
{
	std::stringstream ss;
	ss << getTicketDir().c_str() << "/encryptedTicket_" << appId << ".yaml";

	return ss.str();
}

Ticket::SavedEncryptedTicket Ticket::getCachedEncryptedTicket(uint32_t appId)
{
	if (encryptedTicketMap.contains(appId))
	{
		return encryptedTicketMap[appId];
	}

	SavedEncryptedTicket ticket {};

	const auto path = getEncryptedTicketPath(appId);
	if (!std::filesystem::exists(path.c_str()))
	{
		return ticket;
	}

	std::ifstream ifs(path, std::ios::in);

	g_pLog->debug("Reading encrypted ticket for %u\n", appId);

	auto node = YAML::LoadFile(path);
	ticket.steamId = node["steamId"].as<uint32_t>();
	ticket.ticket = std::string
	(
		//Can not get yaml-cpp to properly decode
		//TODO: Investigate
		//reinterpret_cast<const char*>
		//(
		//	&YAML::DecodeBase64(node["encryptedTicket"].as<std::string>()).at(0)
		//)
		base64::from_base64(node["encryptedTicket"].as<std::string>())
	);
	//g_pLog->debug("Ticket: %u, %u, %u\n", ticket.getSteamId(), ticket.getAppId(), ticket.getSize());
	g_pLog->debug("Ticket: %u, %s\n", ticket.steamId, ticket.ticket.c_str());

	encryptedTicketMap[appId] = ticket;

	return ticket;
}

bool Ticket::saveEncryptedTicketToCache(CMsgClientRequestEncryptedAppTicketResponse* resp)
{
	const uint32_t appId = resp->app_id();

	g_pLog->debug("Saving encrypted ticket for %u...\n", appId);

	auto bytes = resp->SerializeAsString();

	YAML::Emitter node;
	node << YAML::BeginMap;
	node << YAML::Key << "steamId";
	node << YAML::Value << g_currentSteamId;
	node << YAML::Key << "encryptedTicket";
	//node << YAML::Value << YAML::EncodeBase64(reinterpret_cast<const unsigned char*>(bytes.c_str()), bytes.size());
	node << YAML::Value << base64::to_base64(bytes);
	node << YAML::EndMap;

	const auto path = getEncryptedTicketPath(appId);
	std::ofstream ofs(path.c_str(), std::ios::out);

	ofs.write(node.c_str(), node.size());

	g_pLog->once("Saved encrypted ticket for %u\n", appId);

	//TODO: Skip copy
	SavedEncryptedTicket ticket {};
	ticket.steamId = g_currentSteamId;
	ticket.ticket = bytes;
	encryptedTicketMap[appId] = ticket;
	
	return true;
}

void Ticket::recvEncryptedAppTicket(CMsgClientRequestEncryptedAppTicketResponse* msg)
{
	if (msg->eresult() == ERESULT_OK)
	{
		saveEncryptedTicketToCache(msg);
		return;
	}

	SavedEncryptedTicket ticket = getCachedEncryptedTicket(msg->app_id());
	if(!ticket.steamId)
	{
		return;
	}

	msg->ParseFromString(ticket.ticket);
	g_pLog->debug("Using encryptedTicket_%u from disk\n", msg->app_id());
}

void Ticket::recvMsg(CProtoBufMsgBase* msg)
{
	switch(msg->type)
	{
		case EMSG_ENCRYPTED_APPTICKET_RESPONSE:
			recvEncryptedAppTicket(reinterpret_cast<CMsgClientRequestEncryptedAppTicketResponse*>(msg->body));
			break;
	}
}
