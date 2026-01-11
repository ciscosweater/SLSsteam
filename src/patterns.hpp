#pragma once

#include "memhlp.hpp"

#include "libmem/libmem.h"

#include <string>
#include <vector>


struct Pattern_t
{
public:
	const std::string name;
	const std::string pattern;
	const MemHlp::SigFollowMode followMode;
	std::vector<uint8_t> prologue;

	lm_address_t address;

	Pattern_t(const char* name, const char* pattern, MemHlp::SigFollowMode followMode);
	Pattern_t(const char* name, const char* pattern, MemHlp::SigFollowMode followMode, std::vector<uint8_t> prologue);
	//~CPattern();

	bool find();
};

namespace Patterns
{
	extern Pattern_t FamilyGroupRunningApp;
	extern Pattern_t StopPlayingBorrowedApp;

	extern Pattern_t LogSteamPipeCall;

	namespace CProtoBufMsgBase
	{
		extern Pattern_t New;
		extern Pattern_t Send;
	};

	namespace CSteamEngine
	{
		extern Pattern_t Init;
		extern Pattern_t GetAPICallResult;
		extern Pattern_t SetAppIdForCurrentPipe;

		extern Pattern_t Offset_User;
	}

	namespace CUser
	{
		//TODO: Order & Convert old patterns
		extern Pattern_t CheckAppOwnership;
		extern Pattern_t GetSubscribedApps;
		extern Pattern_t PostCallback;
		extern Pattern_t UpdateAppOwnershipTicket;
	}

	namespace IClientAppManager
	{
		extern Pattern_t PipeLoop;
		extern Pattern_t BCanRemotePlayTogether;
	}

	namespace IClientApps
	{
		extern Pattern_t PipeLoop;
	}

	namespace IClientControllerSerialized
	{
		extern Pattern_t PipeLoop;
	}

	namespace IClientRemoteStorage
	{
		extern Pattern_t PipeLoop;
	}

	namespace IClientUGC
	{
		extern Pattern_t PipeLoop;
	}

	namespace IClientUser
	{
		extern Pattern_t PipeLoop;

		extern Pattern_t BIsSubscribedApp;
		extern Pattern_t BLoggedOn;
		extern Pattern_t BUpdateAppOwnershipTicket;
		extern Pattern_t GetAppOwnershipTicketExtendedData;
		extern Pattern_t GetSteamId;
		extern Pattern_t IsUserSubscribedAppInTicket;
		extern Pattern_t RequiresLegacyCDKey;
	}

	namespace IClientUserStats
	{
		extern Pattern_t PipeLoop;
	}

	//Controller config IPC handlers - intercept AppId from game process
	namespace ControllerConfig
	{
		extern Pattern_t AddToConfigCacheHandler;
		extern Pattern_t QueueControllerActivation;
	}

	namespace IClientUtils
	{
		extern Pattern_t PipeLoop;
		extern Pattern_t Offset_GetPipeIndex;
	}


	extern std::vector<Pattern_t*> patterns;
	bool init();
}
