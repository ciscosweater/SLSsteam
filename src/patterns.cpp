#include "patterns.hpp"

#include "globals.hpp"
#include "memhlp.hpp"

#include "libmem/libmem.h"

#include <algorithm>
#include <memory>


Pattern_t::Pattern_t(const char* name, const char* pattern, MemHlp::SigFollowMode followMode)
	:
	Pattern_t(name, pattern, followMode, std::vector<uint8_t>())
{
}

Pattern_t::Pattern_t(const char* name, const char* pattern, MemHlp::SigFollowMode followMode, std::vector<uint8_t> prologue)
	:
	name(name),
	pattern(pattern),
	followMode(followMode),
	prologue(prologue)
{
	Patterns::patterns.emplace_back(this);
}

bool Pattern_t::find()
{
	address = MemHlp::searchSignature(name.c_str(), pattern.c_str(), g_modSteamClient, followMode, &prologue[0], prologue.size());
	return address != LM_ADDRESS_BAD;
}

bool Patterns::init()
{
	for(auto& pattern : patterns)
	{
		if (!pattern->find())
		{
			return false;
		}
	}
	return true;
}

using SigFollowMode = MemHlp::SigFollowMode;

namespace Patterns
{
	Pattern_t FamilyGroupRunningApp
	{
		"FamilyGroupRunningApp",
		"E8 ? ? ? ? 83 C4 10 83 EC 08 C7 46 ? 01 00 00 00 C6 46 ? 01 56 57 E8 ? ? ? ? 83 C4 1C B8 01 00 00 00 5B 5E 5F 5D C3 ? ? ? ? ? ? ? 83 EC 04",
		SigFollowMode::Relative
	};
	Pattern_t StopPlayingBorrowedApp
	{
		"StopPlayingBorrowedApp",
		"8B 40 ? 83 EC 0C 89 F3 8B 95",
		SigFollowMode::PrologueUpwards,
		std::vector<uint8_t> { 0x56, 0x57, 0xe5, 0x89, 0x55 }
	};

	Pattern_t LogSteamPipeCall
	{
		"LogSteamPipeCall",
		"E8 ? ? ? ? 83 C4 10 85 FF 74 ? 8B 07 83 EC 04 FF B5 ? ? ? ? FF B5 ? ? ? ? 57 FF 10 83 C4 10 8D 45 ? 83 EC 04 89 F3 6A 04 50 FF 75",
		SigFollowMode::Relative
	};

	namespace CProtoBufMsgBase
	{
		Pattern_t New
		{
			"CProtoBufMsgBase::New",
			"E8 ? ? ? ? 58 8B 45 ? 8B 8D",
			SigFollowMode::Relative
		};
		Pattern_t Send
		{
			"CProtoBufMsgBase::Send",
			"E8 ? ? ? ? 59 5A 50 56 E8 ? ? ? ? 83 C4 0C",
			SigFollowMode::Relative
		};
	};

	namespace CSteamEngine
	{
		Pattern_t Init
		{
			"CSteamEngine::Init",
			"E8 ? ? ? ? 83 C4 10 8D 83 ? ? ? ? 83 EC 0C 89 AB",
			SigFollowMode::Relative
		};
		Pattern_t GetAPICallResult
		{
			"CSteamEngine::GetAPICallResult",
			"E8 ? ? ? ? 83 C4 20 84 C0 75 ? 8B 86 ? ? ? ? 83 C0 0F",
			SigFollowMode::Relative
		};
		Pattern_t SetAppIdForCurrentPipe
		{
			"CSteamEngine::SetAppIdForCurrentPipe",
			"E8 ? ? ? ? E9 ? ? ? ? ? ? ? ? ? 8B 85 ? ? ? ? 83 EC 08",
			SigFollowMode::Relative
		};
	}

	namespace CUser
	{
		Pattern_t CheckAppOwnership
		{
			"CUser::CheckAppOwnership",
			"E8 ? ? ? ? 88 45 ? 83 C4 10 84 C0 0F 84 ? ? ? ? 8B 45 ? 80 7D ? 00",
			SigFollowMode::Relative
		};
		Pattern_t GetSubscribedApps
		{
			"CUser::GetSubscribedApps",
			"E8 ? ? ? ? 89 C6 83 C4 10 85 C0 0F 84 ? ? ? ? 8B 9D ? ? ? ? 39 D8",
			SigFollowMode::Relative
		};
		Pattern_t UpdateAppOwnershipTicket
		{
			"IClientUser::UpdateAppOwnershipTicket",
			"E8 ? ? ? ? E9 ? ? ? ? ? ? ? ? ? ? 8D 45 ? 89 45 ? EB",
			SigFollowMode::Relative
		};
	}

	namespace IClientAppManager
	{
		Pattern_t PipeLoop
		{
			"IClientAppManager::PipeLoop",
			"FF B5 ? ? ? ? FF B5 ? ? ? ? FF B5 ? ? ? ? E8 ? ? ? ? 83 C4 20 E9 ? ? ? ? E8 ? ? ? ? 8D 8E ? ? ? ? 83 EC 08 66 0F 6E C0 66 0F 6E CA 89 8D ? ? ? ? 8D 86 ? ? ? ? 66 0F 62 C1 66 0F 7E 85 ? ? ? ? 66 0F 73 D0 20 50 51 66 0F 7E 85 ? ? ? ? 89 85 ? ? ? ? E8 ? ? ? ? 83 C4 10 85 FF 74 ? 8B 07 83 EC 04 FF B5 ? ? ? ? FF B5 ? ? ? ? 57 FF 10 83 C4 10 8D 85 ? ? ? ? 83 EC 04 89 F3 6A 04 50 FF B5 ? ? ? ? E8 ? ? ? ? 8D 86 ? ? ? ? 83 C4 10 8B 38",
			SigFollowMode::PrologueUpwards,
			std::vector<uint8_t> { 0x56, 0x57, 0xe5, 0x89, 0x55 }
		};
	}

	namespace IClientApps
	{
		Pattern_t PipeLoop
		{
			"IClientApps::PipeLoop",
			"FF B5 ? ? ? ? FF B5 ? ? ? ? FF B5 ? ? ? ? E8 ? ? ? ? 83 C4 20 E9 ? ? ? ? ? ? ? ? ? ? 8B 85 ? ? ? ? 83 EC 04 FF 70 ? 8B 85 ? ? ? ? FF 70 ? 8D 86 ? ? ? ? FF 30 FF B5 ? ? ? ? FF B5 ? ? ? ? FF B5 ? ? ? ? FF B5 ? ? ? ? E8 ? ? ? ? 83 C4 20 E9 ? ? ? ? ? ? ? ? ? ? 8B 85 ? ? ? ? 83 EC 04 FF 70 ? 8B 85 ? ? ? ? FF 70 ? 8D 86 ? ? ? ? FF 30 FF B5 ? ? ? ? FF B5 ? ? ? ? FF B5 ? ? ? ? FF B5 ? ? ? ? E8 ? ? ? ? 83 C4 20 E9 ? ? ? ? ? ? ? ? ? ? 8B 85 ? ? ? ? 83 EC 04 FF 70 ? 8B 85 ? ? ? ? FF 70 ? 8D 86 ? ? ? ? FF 30 FF B5 ? ? ? ? FF B5 ? ? ? ? FF B5 ? ? ? ? FF B5 ? ? ? ? E8 ? ? ? ? 83 C4 20 E9 ? ? ? ? ? ? ? ? ? ? 8B 85 ? ? ? ? 83 EC 04 FF 70 ? 8B 85 ? ? ? ? FF 70 ? 8D 86 ? ? ? ? FF 30 FF B5 ? ? ? ? FF B5 ? ? ? ? FF B5 ? ? ? ? FF B5 ? ? ? ? E8 ? ? ? ? 83 C4 20 E9 ? ? ? ? ? ? ? ? ? ? 8B 85 ? ? ? ? 83 EC 04",
			SigFollowMode::PrologueUpwards,
			std::vector<uint8_t> { 0x56, 0x57, 0xe5, 0x89, 0x55 }
		};
	}

	namespace IClientRemoteStorage
	{
		Pattern_t PipeLoop
		{
			"IClientRemoteStorage::PipeLoop",
			"8D 8E ? ? ? ? 83 EC 08 66 0F 6E C0 66 0F 6E CA 89 8D ? ? ? ? 8D 86 ? ? ? ? 66 0F 62 C1 66 0F 7E 85 ? ? ? ? 66 0F 73 D0 20 50 51 66 0F 7E 85 ? ? ? ? 89 85 ? ? ? ? E8 ? ? ? ? 83 C4 10 85 FF 74 ? 8B 07 83 EC 04 FF B5 ? ? ? ? FF B5 ? ? ? ? 57 FF 10 83 C4 10 8D 85 ? ? ? ? 83 EC 04 89 F3 6A 08 50 FF B5 ? ? ? ? E8 ? ? ? ? 8D 86 ? ? ? ? 83 C4 10 8B 18 85 DB",
			SigFollowMode::PrologueUpwards,
			std::vector<uint8_t> { 0x56, 0x57, 0xe5, 0x89, 0x55 }
		};
	}

	namespace IClientUser
	{
		Pattern_t PipeLoop
		{
			"IClientUser::PipeLoop",
			"66 0F 62 C1 66 0F 7E 85 ? ? ? ? 66 0F 73 D0 20 50 51 66 0F 7E 85 ? ? ? ? 89 85 ? ? ? ? E8 ? ? ? ? 83 C4 10 85 FF 74 ? 8B 07 83 EC 04 FF B5 ? ? ? ? FF B5 ? ? ? ? 57 FF 10 83 C4 10 8D 85 ? ? ? ? 83 EC 04 89 F3 C7 85 ? ? ? ? 00 00 00 00 6A 04 50 FF B5 ? ? ? ? E8 ? ? ? ? 8B 85 ? ? ? ? 83 C4 10 05 ? ? ? ? 3B 85 ? ? ? ? 74 ? 83 EC 0C FF B5 ? ? ? ? 8D 86 ? ? ? ? FF B5 ? ? ? ? 50 8D 86 ? ? ? ? 68 4E 01 00 00",
			SigFollowMode::PrologueUpwards,
			std::vector<uint8_t> { 0x56, 0x57, 0xe5, 0x89, 0x55 }
		};

		Pattern_t BIsSubscribedApp
		{
			"IClientUser::BIsSubscribedApp",
			"E8 ? ? ? ? 83 C4 10 84 C0 74 ? 8B 95 ? ? ? ? 83 EC 04",
			SigFollowMode::Relative
		};
		Pattern_t BLoggedOn
		{
			"IClientUser::BLoggedOn",
			"E9 ? ? ? ? ? ? ? ? ? ? 5B 5E 5F FF E0",
			SigFollowMode::Relative
		};
		Pattern_t BUpdateAppOwnershipTicket
		{
			"IClientUser::BUpdateAppOwnershipTicket",
			"83 EC 0C 89 F3 8B 7D ? FF 30 E8 ? ? ? ? 83 C4 10 83 FF 01 77 ? 84 C0 75 ? 80 7D ? 00 74 ? 80 7D ? 00 0F 84 ? ? ? ? 8D 65",
			SigFollowMode::PrologueUpwards,
			std::vector<uint8_t> { 0x56, 0x57, 0xe5, 0x89, 0x55 }
		};
		Pattern_t GetAppOwnershipTicketExtendedData
		{
			"IClientUser::GetAppOwnershipTicketExtendedData",
			"83 EC 24 FF 74 24 ? 8B 44 24",
			SigFollowMode::PrologueUpwards,
			std::vector<uint8_t> { 0x53, 0x56, 0x57, 0x55 }
		};
		Pattern_t GetSteamId
		{
			"IClientUser::GetSteamID",
			//Not unique. All matches point to correct function though
			"E8 ? ? ? ? 89 D8 83 C4 0C 83 C4 08 5B C2 04 00 ? 83 EC 08 50 53 FF D2 89 D8 83 C4 0C 83 C4 08 5B C2 04 00",
			SigFollowMode::Relative
		};
		Pattern_t IsUserSubscribedAppInTicket
		{
			"IClientUser::IsUserSubscribedAppInTicket",
			"E8 ? ? ? ? 89 C3 83 C4 20 8B ? ? ? ? ? 8B",
			SigFollowMode::Relative
		};
		Pattern_t RequiresLegacyCDKey
		{
			"IClientUser::RequiresLegacyCDKey",
			"C3 ? ? ? ? ? 8B 44 24 ? 83 C4 1C 89 F9 89 F2 5B 5E 5F 5D 2D 94 18 00 00",
			SigFollowMode::PrologueUpwards,
			std::vector<uint8_t> { 0x53, 0x56, 0x57, 0x55 }
		};
	}

	namespace IClientUGC
	{
		Pattern_t PipeLoop
		{
			"IClientUGC::PipeLoop",
			"66 0F 62 C1 66 0F 7E 85 ? ? ? ? 66 0F 73 D0 20 50 51 66 0F 7E 85 ? ? ? ? 89 85 ? ? ? ? E8 ? ? ? ? 83 C4 10 85 FF 74 ? 8B 07 83 EC 04 FF B5 ? ? ? ? FF B5 ? ? ? ? 57 FF 10 83 C4 10 8D 85 ? ? ? ? 83 EC 04 89 F3 6A 08 50 FF B5 ? ? ? ? E8 ? ? ? ? 8D 86 ? ? ? ? 83 C4 10 8B 38 89 85 ? ? ? ? 85 FF 74 ? 8D 86 ? ? ? ? 83 EC 0C FF B5 ? ? ? ? 8D BD ? ? ? ? FF B5 ? ? ? ? 50 6A 20 57 E8 ? ? ? ? 83 C4 14 57 E8 ? ? ? ? 83 C4 10 8D 85 ? ? ? ? 83 EC 04 89 F3 6A 01 50 FF B5 ? ? ? ? E8 ? ? ? ? 8B 85 ? ? ? ? 83 C4 10 8B 08 85 C9 74 ? 0F B6 85 ? ? ? ? E8 ? ? ? ? 8D 85 ? ? ? ? 83 EC 04 89 F3 C7 85 ? ? ? ? 00 00 00 00 6A 04 50 FF B5 ? ? ? ? E8 ? ? ? ? 8B 85 ? ? ? ? 83 C4 10 05 52 2F 4B 04",
			SigFollowMode::PrologueUpwards,
			std::vector<uint8_t> { 0x56, 0x57, 0xe5, 0x89, 0x55 }
		};
	}

	namespace IClientUserStats
	{
		Pattern_t PipeLoop
		{
			"IClientUserStats",
			"50 6A 20 57 E8 ? ? ? ? 89 3C 24 E8 ? ? ? ? 8B 85 ? ? ? ? 83 C4 10 83 EC 08 89 F3 50 FF B5 ? ? ? ? E8 ? ? ? ? 8B 45 ? 83 C4 0C 6A 04 C7 85 ? ? ? ? 00 00 00 00 89 85 ? ? ? ? 8D 85 ? ? ? ? 50 FF B5 ? ? ? ? E8 ? ? ? ? 8B 85 ? ? ? ? 83 C4 10 05 95 F1 FB 02",
			SigFollowMode::PrologueUpwards,
			std::vector<uint8_t> { 0x56, 0x57, 0xe5, 0x89, 0x55 }
		};
	}

	namespace IClientUtils
	{
		Pattern_t PipeLoop
		{
			"IClientUtils::PipeLoop",
			"83 EC 08 89 F3 50 57 E8 ? ? ? ? 58 FF B5 ? ? ? ? E8 ? ? ? ? 58 8D 45",
			SigFollowMode::PrologueUpwards,
			std::vector<uint8_t> { 0x56, 0x57, 0xe5, 0x89, 0x55 }
		};
	}

	std::vector<Pattern_t*> patterns;
}

