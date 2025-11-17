#pragma once
#include "memhlp.hpp"
#include "patterns.hpp"

#include "libmem/libmem.h"

#include "sdk/CAppOwnershipInfo.hpp"

#include <cstddef>
#include <memory>
#include <string>

template<typename T>
union FunctionUnion_t
{
	T fn;
	lm_address_t address;
};

//TODO: Look up if there's an interface kinda thing for C++
template<typename T>
class Hook
{
public:
	//TODO: Add base setup fn to set hookFn
	std::string name;
	FunctionUnion_t<T> originalFn;
	FunctionUnion_t<T> hookFn;

	Hook(const char* name);

	virtual void place() = 0;
	virtual void remove() = 0;
};

template<typename T>
class DetourHook : public Hook<T>
{
public:
	FunctionUnion_t<T> tramp;
	size_t size;

	DetourHook();
	//DetourHook(const char* name);

	virtual void place();
	virtual void remove();

	bool setup(Pattern_t pattern, T hookFn);
};

template<typename T>
class VFTHook : public Hook<T>
{
public:
	std::shared_ptr<lm_vmt_t> vft;
	unsigned int index;
	bool hooked;

	VFTHook(const char* name);

	virtual void place();
	virtual void remove();

	void setup(std::shared_ptr<lm_vmt_t> vft, unsigned int index, T hookFn);
};

namespace Hooks
{
	typedef void(*LogSteamPipeCall_t)(const char*, const char*);
	typedef void(*ParseProtoBufResponse_t)(void*, void*);

	typedef void(*IClientAppManager_PipeLoop_t)(void*, void*, void*, void*);
	typedef void(*IClientApps_PipeLoop_t)(void*, void*, void*, void*);
	typedef void(*IClientRemoteStorage_PipeLoop_t)(void*, void*, void*, void*);
	typedef void(*IClientUtils_PipeLoop_t)(void*, void*, void*, void*);
	typedef void(*IClientUser_PipeLoop_t)(void*, void*, void*, void*);

	typedef uint32_t(*CAPIJob_RequestUserStats_t)(void*);

	typedef bool(*CUser_CheckAppOwnership_t)(void*, uint32_t, CAppOwnershipInfo*);
	typedef bool(*CUser_GetEncryptedAppTicket_t)(void*, void*, uint32_t, uint32_t*);
	typedef uint32_t(*CUser_GetSubscribedApps_t)(void*, uint32_t*, size_t, bool);

	typedef bool(*IClientUser_BIsSubscribedApp_t)(void*, uint32_t);
	typedef bool(*IClientUser_BLoggedOn_t)(void*);
	typedef uint32_t(*IClientUser_BUpdateAppOwnershipInfo_t)(void*, uint32_t, bool);
	typedef bool(*IClientUser_GetAPICallResult_t)(void*, uint32_t, uint32_t, void*, uint32_t, uint32_t, bool*);
	typedef uint32_t(*IClientUser_GetAppOwnershipTicketExtendedData_t)(void*, uint32_t, void*, uint32_t, uint32_t*, uint32_t*, uint32_t*, uint32_t*);
	typedef uint8_t(*IClientUser_IsUserSubscribedAppInTicket_t)(void*, uint32_t, uint32_t, uint32_t, uint32_t);
	typedef bool(*IClientUser_RequiresLegacyCDKey_t)(void*, uint32_t, uint32_t*);

	typedef bool(*IClientUtils_GetOfflineMode_t)(void*);

	extern DetourHook<LogSteamPipeCall_t> LogSteamPipeCall;
	extern DetourHook<ParseProtoBufResponse_t> ParseProtoBufResponse;

	extern DetourHook<IClientAppManager_PipeLoop_t> IClientAppManager_PipeLoop;
	extern DetourHook<IClientApps_PipeLoop_t> IClientApps_PipeLoop;
	extern DetourHook<IClientRemoteStorage_PipeLoop_t> IClientRemoteStorage_PipeLoop;
	extern DetourHook<IClientUtils_PipeLoop_t> IClientUtils_PipeLoop;
	extern DetourHook<IClientUser_PipeLoop_t> IClientUser_PipeLoop;

	extern DetourHook<CAPIJob_RequestUserStats_t> CAPIJob_RequestUserStats;

	extern DetourHook<CUser_CheckAppOwnership_t> CUser_CheckAppOwnership;
	extern DetourHook<CUser_GetEncryptedAppTicket_t> CUser_GetEncryptedAppTicket;
	extern DetourHook<CUser_GetSubscribedApps_t> CUser_GetSubscribedApps;

	extern DetourHook<IClientUser_BIsSubscribedApp_t> IClientUser_BIsSubscribedApp;
	extern DetourHook<IClientUser_BLoggedOn_t> IClientUser_BLoggedOn;
	extern DetourHook<IClientUser_BUpdateAppOwnershipInfo_t> IClientUser_BUpdateAppOwnershipInfo;
	extern DetourHook<IClientUser_GetAPICallResult_t> IClientUser_GetAPICallResult;
	extern DetourHook<IClientUser_GetAppOwnershipTicketExtendedData_t> IClientUser_GetAppOwnershipTicketExtendedData;
	extern DetourHook<IClientUser_IsUserSubscribedAppInTicket_t> IClientUser_IsUserSubscribedAppInTicket;
	extern DetourHook<IClientUser_RequiresLegacyCDKey_t> IClientUser_RequiresLegacyCDKey;

	typedef bool(*IClientAppManager_BIsDlcEnabled_t)(void*, uint32_t, uint32_t, void*);
	typedef bool(*IClientAppManager_GetAppUpdateInfo_t)(void*, uint32_t, uint32_t*);
	typedef void*(*IClientAppManager_LaunchApp_t)(void*, uint32_t*, void*, void*, void*);
	typedef bool(*IClientAppManager_IsAppDlcInstalled_t)(void*, uint32_t, uint32_t);
	typedef unsigned int(*IClientApps_GetDLCCount_t)(void*, uint32_t);
	typedef bool(*IClientApps_GetDLCDataByIndex_t)(void*, uint32_t, int, uint32_t*, bool*, char*, size_t);
	typedef bool(*IClientRemoteStorage_IsCloudEnabledForApp_t)(void*, uint32_t);

	extern VFTHook<IClientAppManager_BIsDlcEnabled_t> IClientAppManager_BIsDlcEnabled;
	extern VFTHook<IClientAppManager_GetAppUpdateInfo_t> IClientAppManager_GetAppUpdateInfo;
	extern VFTHook<IClientAppManager_LaunchApp_t> IClientAppManager_LaunchApp;
	extern VFTHook<IClientAppManager_IsAppDlcInstalled_t> IClientAppManager_IsAppDlcInstalled;

	extern VFTHook<IClientApps_GetDLCDataByIndex_t> IClientApps_GetDLCDataByIndex;
	extern VFTHook<IClientApps_GetDLCCount_t> IClientApps_GetDLCCount;

	extern VFTHook<IClientRemoteStorage_IsCloudEnabledForApp_t> IClientRemoteStorage_IsCloudEnabledForApp;

	extern VFTHook<IClientUtils_GetOfflineMode_t> IClientUtils_GetOfflineMode;

	extern lm_address_t IClientUser_GetSteamId;

	bool setup();
	void place();
	void remove();
}
