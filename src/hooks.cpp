#include "config.hpp"
#include "globals.hpp"
#include "hooks.hpp"
#include "log.hpp"
#include "memhlp.hpp"
#include "patterns.hpp"
#include "sdk/IClientUser.hpp"
#include "vftableinfo.hpp"

#include "libmem/libmem.h"

#include "sdk/CAppOwnershipInfo.hpp"
#include "sdk/IClientApps.hpp"
#include "sdk/IClientAppManager.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <pthread.h>
#include <strings.h>
#include <unistd.h>
#include <vector>

template<typename T>
Hook<T>::Hook(const char* name)
{
	this->name = std::string(name);
}

template<typename T>
DetourHook<T>::DetourHook(const char* name) : Hook<T>::Hook(name)
{
	this->size = 0;
}

template<typename T>
VFTHook<T>::VFTHook(const char* name) : Hook<T>::Hook(name)
{
	this->hooked = false;
}

template<typename T>
bool DetourHook<T>::setup(const char* pattern, const MemHlp::SigFollowMode followMode, lm_byte_t* extraData, lm_size_t extraDataSize, T hookFn)
{
	//Hardcoding g_modSteamClient here is definitely bad design, but we can easily change that
	//in case we ever need to
	lm_address_t oFn = MemHlp::searchSignature(this->name.c_str(), pattern, g_modSteamClient, followMode, extraData, extraDataSize);
	if (oFn == LM_ADDRESS_BAD)
	{
		return false;
	}

	this->originalFn.address = oFn;
	this->hookFn.fn = hookFn;

	return true;
}

template<typename T>
bool DetourHook<T>::setup(const char* pattern, const MemHlp::SigFollowMode followMode, T hookFn)
{
	return setup(pattern, followMode, nullptr, 0, hookFn);
}

template<typename T>
void DetourHook<T>::place()
{
	this->size = LM_HookCode(this->originalFn.address, this->hookFn.address, &this->tramp.address);
	MemHlp::fixPICThunkCall(this->name.c_str(), this->originalFn.address, this->tramp.address);

	g_pLog->debug
	(
		"Detour hooked %s (%p) with hook at %p and tramp at %p\n",
		this->name.c_str(),
		this->originalFn.address,
		this->hookFn.address,
		this->tramp.address
	);
}

template<typename T>
void DetourHook<T>::remove()
{
	if (!this->size)
	{
		return;
	}

	LM_UnhookCode(this->originalFn.address, this->tramp.address, this->size);
	this->size = 0;

	g_pLog->debug("Unhooked %s\n", this->name.c_str());
}

template<typename T>
void VFTHook<T>::place()
{
	LM_VmtHook(this->vft.get(), this->index, this->hookFn.address);
	this->hooked = true;

	g_pLog->debug
	(
		"VFT hooked %s (%p) with hook at %p\n",
		this->name.c_str(),
		this->originalFn.address,
		this->hookFn.address
	);
}

template<typename T>
void VFTHook<T>::remove()
{
	//No clue how libmem reacts when unhooking a non existent hook
	//so we do this
	if (!this->hooked)
	{
		return;
	}

	LM_VmtUnhook(this->vft.get(), this->index);
	this->hooked = false;

	g_pLog->debug("Unhooked %s!\n", this->name.c_str());
}

template<typename T>
void VFTHook<T>::setup(std::shared_ptr<lm_vmt_t> vft, unsigned int index, T hookFn)
{
	this->vft = vft;
	this->index = index;

	this->originalFn.address = LM_VmtGetOriginal(this->vft.get(), this->index);
	this->hookFn.fn = hookFn;
}

__attribute__((hot))
static void hkLogSteamPipeCall(const char* iface, const char* fn)
{
	Hooks::LogSteamPipeCall.tramp.fn(iface, fn);

	if (g_config.extendedLogging)
	{
		g_pLog->debug("LogSteamPipeCall(%s, %s)\n", iface, fn);
	}
}

{


}

static bool applistRequested = false;
static auto appIdOwnerOverride = std::map<uint32_t, int>();

static void* hkClientAppManager_LaunchApp(void* pClientAppManager, uint32_t* pAppId, void* a2, void* a3, void* a4)
{
	if (pAppId)
	{
		g_pLog->once("IClientAppManager::LaunchApp(%p, %u, %p, %p, %p)\n", pClientAppManager, *pAppId, a2, a3, a4);
		appIdOwnerOverride[*pAppId] = 0;
	}

	//Do not do anything in post! Otherwise App launching will break
	return Hooks::IClientAppManager_LaunchApp.originalFn.fn(pClientAppManager, pAppId, a2, a3, a4);
}

static bool hkClientAppManager_IsAppDlcInstalled(void* pClientAppManager, uint32_t appId, uint32_t dlcId)
{
	const bool ret = Hooks::IClientAppManager_IsAppDlcInstalled.originalFn.fn(pClientAppManager, appId, dlcId);
	g_pLog->once("IClientAppManager::IsAppDlcInstalled(%p, %u, %u) -> %i\n", pClientAppManager, appId, dlcId, ret);

	//Do not pretend things are installed while downloading Apps, otherwise downloads will break for some of them
	auto state = g_pClientAppManager->getAppInstallState(appId);
	if (state & APPSTATE_DOWNLOADING || state & APPSTATE_INSTALLING)
	{
		g_pLog->once("Skipping DlcId %u because AppId %u has AppState %i\n", dlcId, appId, state);
		return ret;
	}

	if (g_config.shouldExcludeAppId(dlcId))
	{
		return ret;
	}

	return true;
}

static bool hkClientAppManager_BIsDlcEnabled(void* pClientAppManager, uint32_t appId, uint32_t dlcId, void* a3)
{
	const bool ret = Hooks::IClientAppManager_BIsDlcEnabled.originalFn.fn(pClientAppManager, appId, dlcId, a3);
	g_pLog->once("IClientAppManager::BIsDlcEnabled(%p, %u, %u, %p) -> %i\n", pClientAppManager, appId, dlcId, a3, ret);

	//TODO: Add check for legit ownership to allow toggle on/off
	if (g_config.shouldExcludeAppId(dlcId))
	{
		return ret;
	}

	return true;
}

static bool hkClientAppManager_GetUpdateInfo(void* pClientAppManager, uint32_t appId, uint32_t* a2)
{
	const bool success = Hooks::IClientAppManager_GetAppUpdateInfo.originalFn.fn(pClientAppManager, appId, a2);
	g_pLog->info("IClientAppManager::GetUpdateInfo(%p, %u, %p) -> %i\n", pClientAppManager, appId, a2, success);

	if (g_config.isAddedAppId(appId))
	{
		g_pLog->once("Disabled updates for %u\n", appId);
		return false;
	}

	return success;
}

__attribute__((hot))
static void hkClientAppManager_PipeLoop(void* pClientAppManager, void* a1, void* a2, void* a3)
{
	g_pClientAppManager = reinterpret_cast<IClientAppManager*>(pClientAppManager);

	std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
	LM_VmtNew(*reinterpret_cast<lm_address_t**>(pClientAppManager), vft.get());

	Hooks::IClientAppManager_BIsDlcEnabled.setup(vft, VFTIndexes::IClientAppManager::BIsDlcEnabled, hkClientAppManager_BIsDlcEnabled);
	Hooks::IClientAppManager_GetAppUpdateInfo.setup(vft, VFTIndexes::IClientAppManager::GetUpdateInfo, hkClientAppManager_GetUpdateInfo);
	Hooks::IClientAppManager_LaunchApp.setup(vft, VFTIndexes::IClientAppManager::LaunchApp, hkClientAppManager_LaunchApp);
	Hooks::IClientAppManager_IsAppDlcInstalled.setup(vft, VFTIndexes::IClientAppManager::IsAppDlcInstalled, hkClientAppManager_IsAppDlcInstalled);

	Hooks::IClientAppManager_BIsDlcEnabled.place();
	Hooks::IClientAppManager_GetAppUpdateInfo.place();
	Hooks::IClientAppManager_LaunchApp.place();
	Hooks::IClientAppManager_IsAppDlcInstalled.place();

	g_pLog->debug("IClientAppManager->vft at %p\n", vft->vtable);

	Hooks::IClientAppManager_PipeLoop.remove();
	Hooks::IClientAppManager_PipeLoop.originalFn.fn(pClientAppManager, a1, a2, a3);
}

static unsigned int hkClientApps_GetDLCCount(void* pClientApps, uint32_t appId)
{
	unsigned int count = Hooks::IClientApps_GetDLCCount.originalFn.fn(pClientApps, appId);
	if (g_config.dlcData.contains(appId))
	{
		count = g_config.dlcData[appId].dlcIds.size();
	}

	g_pLog->once("IClientApps::GetDLCCount(%p, %u) -> %u\n", pClientApps, appId, count);
	return count;
}

static bool hkClientApps_GetDLCDataByIndex(void* pClientApps, uint32_t appId, int dlcIndex, uint32_t* pDlcId, bool* pIsAvailable, char* pChDlcName, size_t dlcNameLen)
{
	bool ret;

	if (g_config.dlcData.contains(appId))
	{
		auto& data = g_config.dlcData[appId];
		auto dlc = std::next(data.dlcIds.begin(), dlcIndex);

		*pDlcId = dlc->first;

		//No clue if we have to check for errors during printf since the devs hopefully didn't fuck
		//up the dlcNameLen. Who knows though
		snprintf(pChDlcName, dlcNameLen, "%s", dlc->second.c_str());

		ret = true;
	}
	else
	{
		ret = Hooks::IClientApps_GetDLCDataByIndex.originalFn.fn(pClientApps, appId, dlcIndex, pDlcId, pIsAvailable, pChDlcName, dlcNameLen);
	}

	g_pLog->once("IClientApps::GetDLCDataByIndex(%p, %u, %i, %p, %p, %s, %i) -> %i\n", pClientApps, appId, dlcIndex, pDlcId, pIsAvailable, pChDlcName, dlcNameLen, ret);

	if (pIsAvailable && pDlcId && !g_config.shouldExcludeAppId(*pDlcId))
	{
		*pIsAvailable = true;
	}

	return ret;
}

__attribute__((hot))
static void hkClientApps_PipeLoop(void* pClientApps, void* a1, void* a2, void* a3)
{
	g_pClientApps = reinterpret_cast<IClientApps*>(pClientApps);

	std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
	LM_VmtNew(*reinterpret_cast<lm_address_t**>(pClientApps), vft.get());

	Hooks::IClientApps_GetDLCDataByIndex.setup(vft, VFTIndexes::IClientApps::GetDLCDataByIndex, hkClientApps_GetDLCDataByIndex);
	Hooks::IClientApps_GetDLCCount.setup(vft, VFTIndexes::IClientApps::GetDLCCount, hkClientApps_GetDLCCount);

	Hooks::IClientApps_GetDLCDataByIndex.place();
	Hooks::IClientApps_GetDLCCount.place();

	g_pLog->debug("IClientApps->vft at %p\n", vft->vtable);

	Hooks::IClientApps_PipeLoop.remove();
	Hooks::IClientApps_PipeLoop.originalFn.fn(pClientApps, a1, a2, a3);
}

static bool hkClientRemoteStorage_IsCloudEnabledForApp(void* pClientRemoteStorage, uint32_t appId)
{
	const bool enabled = Hooks::IClientRemoteStorage_IsCloudEnabledForApp.originalFn.fn(pClientRemoteStorage, appId);
	g_pLog->once("IClientRemoteStorage::IsCloudEnabledForApp(%p, %u) -> %i\n", pClientRemoteStorage, appId, enabled);

	if (g_config.isAddedAppId(appId))
	{
		g_pLog->once("Disabled cloud for %u\n", appId);
		return false;
	}

	return enabled;
}

static void hkClientRemoteStorage_PipeLoop(void* pClientRemoteStorage, void* a1, void* a2, void* a3)
{
	std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
	LM_VmtNew(*reinterpret_cast<lm_address_t**>(pClientRemoteStorage), vft.get());

	Hooks::IClientRemoteStorage_IsCloudEnabledForApp.setup(vft, VFTIndexes::IClientRemoteStorage::IsCloudEnabledForApp, hkClientRemoteStorage_IsCloudEnabledForApp);
	Hooks::IClientRemoteStorage_IsCloudEnabledForApp.place();

	g_pLog->debug("IClientRemoteStorage->vft at %p\n", vft->vtable);

	Hooks::IClientRemoteStorage_PipeLoop.remove();
	Hooks::IClientRemoteStorage_PipeLoop.originalFn.fn(pClientRemoteStorage, a1, a2, a3);
}

static bool hkClientUser_BIsSubscribedApp(void* pClientUser, uint32_t appId)
{
	const bool ret = Hooks::IClientUser_BIsSubscribedApp.tramp.fn(pClientUser, appId);

	g_pLog->once("IClientUser::BIsSubscribedApp(%p, %u) -> %i\n", pClientUser, appId, ret);

	if (g_config.shouldExcludeAppId(appId))
	{
		return ret;
	}

	return true;
}

__attribute__((hot))
static bool hkClientUser_CheckAppOwnership(void* pClientUser, uint32_t appId, CAppOwnershipInfo* pOwnershipInfo)
{
	const bool ret = Hooks::IClientUser_CheckAppOwnership.tramp.fn(pClientUser, appId, pOwnershipInfo);

	//Do not log pOwnershipInfo because it gets deleted very quickly, so it's pretty much useless in the logs
	g_pLog->once("IClientUser::CheckAppOwnership(%p, %u) -> %i\n", pClientUser, appId, ret);

	//Wait Until GetSubscribedApps gets called once to let Steam request and populate legit data first.
	//Afterwards modifying should hopefully not affect false positives anymore
	if (!applistRequested || g_config.shouldExcludeAppId(appId) || !pOwnershipInfo || !g_currentSteamId)
	{
		return ret;
	}

	const uint32_t denuvoOwner = g_config.getDenuvoGameOwner(appId);
	//Do not modify Denuvo enabled Games
	if (!g_config.denuvoSpoof && denuvoOwner && denuvoOwner != g_currentSteamId)
	{
		//Would love to log the SteamId, but for users anonymity I won't
		g_pLog->once("Skipping %u because it's a Denuvo game from someone else\n", appId);
		return ret;
	}

	if (g_config.isAddedAppId(appId) || (g_config.playNotOwnedGames && !pOwnershipInfo->purchased))
	{
		if (!denuvoOwner || denuvoOwner == g_currentSteamId)
		{
			//Changing the purchased field is enough, but just for nicety in the Steamclient UI we change the owner too
			pOwnershipInfo->ownerSteamId = g_currentSteamId;
			pOwnershipInfo->familyShared = false;
		}
		else if (denuvoOwner)
		{
			pOwnershipInfo->ownerSteamId = denuvoOwner;
			pOwnershipInfo->familyShared = true;
		}

		pOwnershipInfo->purchased = true;
		//Unnessecary but whatever
		pOwnershipInfo->permanent = true;

		//Found in backtrace
		pOwnershipInfo->releaseState = 4;
		pOwnershipInfo->field10_0x25 = 0;
		//Seems to do nothing in particular, some dlc have this as 1 so I uncomented this for now. Might be free stuff?
		//pOwnershipInfo->field27_0x36 = 1;

		g_config.addAdditionalAppId(appId);
	}

	//Doing that might be not worth it since this will most likely be easier to mantain
	//TODO: Backtrace those 4 calls and only patch the really necessary ones since this might be prone to breakage
	if (!denuvoOwner && g_config.disableFamilyLock && appIdOwnerOverride.count(appId) && appIdOwnerOverride.at(appId) < 4)
	{
		pOwnershipInfo->ownerSteamId = 1; //Setting to "arbitrary" steam Id instead of own, otherwise bypass won't work for own games
		//Unnessecarry again, but whatever
		pOwnershipInfo->permanent = true;
		pOwnershipInfo->familyShared = false;

		appIdOwnerOverride[appId]++;
	}

	//Returning false after we modify data shouldn't cause any problems because it should just get discarded

	if (!g_pClientApps)
		return ret;

	auto type = g_pClientApps->getAppType(appId);
	if (type == APPTYPE_DLC) //Don't touch DLC here, otherwise downloads might break. Hopefully this won't decrease compatibility
	{
		return ret;
	}

	if (g_config.automaticFilter)
	{
		switch(type)
		{
			case APPTYPE_APPLICATION:
			case APPTYPE_GAME:
				break;

			default:
				return ret;
		}
	}

	return true;
}

static uint8_t hkClientUser_IsUserSubscribedAppInTicket(void* pClientUser, uint32_t steamId, uint32_t a2, uint32_t a3, uint32_t appId)
{
	const uint8_t ticketState = Hooks::IClientUser_IsUserSubscribedAppInTicket.tramp.fn(pClientUser, steamId, a2, a3, appId);
	//g_pLog->once("IClientUser::IsUserSubscribedAppInTicket(%p, %u, %u, %u, %u) -> %i\n", pClientUser, steamId, a2, a3, appId, ticketState);
	//Don't log the steamId, protect users from themselves and stuff
	g_pLog->once("IClientUser::IsUserSubscribedAppInTicket(%p, %u, %u, %u) -> %i\n", pClientUser, a2, a3, appId, ticketState);
	
	//Might want to compare the steamId param to the g_currentSteamId in the future
	//Although not doing that might also work for Dedicated servers?
	if (!g_config.shouldExcludeAppId(appId))
	{
		//Owned and subscribed hehe :)
		return 0;
	}

	return ticketState;
}

static uint32_t hkClientUser_GetSubscribedApps(void* pClientUser, uint32_t* pAppList, size_t size, bool a3)
{
	uint32_t count = Hooks::IClientUser_GetSubscribedApps.tramp.fn(pClientUser, pAppList, size, a3);
	g_pLog->once("IClientUser::GetSubscribedApps(%p, %p, %i, %i) -> %i\n", pClientUser, pAppList, size, a3, count);

	//Valve calls this function twice, once with size of 0 then again
	if (!size || !pAppList)
		return count + g_config.addedAppIds.size();

	//TODO: Maybe Add check if AppId already in list before blindly appending
	for(auto& appId : g_config.addedAppIds)
	{
		pAppList[count++] = appId;
	}

	applistRequested = true;

	return count;
}

static bool hkClientUser_RequiresLegacyCDKey(void* pClientUser, uint32_t appId, uint32_t* a2)
{
	const bool requiresKey = Hooks::IClientUser_RequiresLegacyCDKey.tramp.fn(pClientUser, appId, a2);
	g_pLog->once("IClientUser::RequiresLegacyCDKey(%p, %u, %u) -> %i\n", pClientUser, appId, a2, requiresKey);
		
	if (requiresKey && g_config.isAddedAppId(appId))
	{
		g_pLog->once("Disable CD Key for %u\n", appId);
		return false;
	}

	return requiresKey;
}

static void patchRetn(lm_address_t address)
{
	constexpr lm_byte_t retn = 0xC3;

	lm_prot_t oldProt;
	LM_ProtMemory(address, 1, LM_PROT_XRW, &oldProt); //LM_PROT_W Should be enough, but just in case something tries to execute it inbetween us setting the prot and writing to it
	LM_WriteMemory(address, &retn, 1);
	LM_ProtMemory(address, 1, oldProt, LM_NULL);
}

static lm_address_t hkGetSteamId;
static bool createAndPlaceSteamIdHook()
{
	hkGetSteamId = LM_AllocMemory(0, LM_PROT_XRW);
	if (hkGetSteamId == LM_ADDRESS_BAD)
	{
		g_pLog->debug("Failed to allocate memory for GetSteamId!\n");
		return false;
	}

	g_pLog->debug("Allocated memory for GetSteamId hook at %p\n", hkGetSteamId);

	auto insts = std::vector<lm_inst_t>();
	lm_address_t readAddr = Hooks::IClientUser_GetSteamId;
	for(;;)
	{
		lm_inst_t inst;
		if (!LM_Disassemble(readAddr, &inst))
		{
			g_pLog->debug("Failed to disassemble function at %p!\n", readAddr);
			return false;
		}

		insts.emplace_back(inst);
		readAddr = inst.address + inst.size;

		if (strcmp(inst.mnemonic, "ret") == 0)
		{
			break;
		}
	}

	const unsigned int retIdx = insts.size() - 1;

	g_pLog->debug("Ret is instruction number %u\n", retIdx);
	//TODO: Create InlineHook class for this
	size_t totalBytes = 0;
	unsigned int instsToOverwrite = 0;
	for(int i = retIdx; i >= 0; i--)
	{
		lm_inst_t inst = insts.at(i);
		totalBytes += inst.size;
		instsToOverwrite++;

		//Need only 5 bytes to place relative jmp
		if (totalBytes >= 5)
		{
			break;
		}
	}

	lm_address_t writeAddr = hkGetSteamId;
	//TODO: Dynamically resolve register which holds SteamId
	MemHlp::assembleCodeAt(writeAddr, "mov [%p], ecx", &g_currentSteamId);

	//Write the overwritten instructions after our hook code
	for (unsigned int i = 0; i < instsToOverwrite; i++)
	{
		lm_inst_t inst = insts.at(insts.size() - instsToOverwrite + i);
		memcpy(reinterpret_cast<void*>(writeAddr), inst.bytes, inst.size);

		writeAddr += inst.size;
		g_pLog->debug("Copied %s %s to tramp\n", inst.mnemonic, inst.op_str);
	}

	lm_address_t jmpAddr = insts.at(insts.size() - instsToOverwrite).address;
	g_pLog->debug("Placing jmp at %p\n", jmpAddr);

	//Might be worth to convert to LM_AssembleEx, but whatever
	lm_prot_t oldProt;
	LM_ProtMemory(jmpAddr, 5, LM_PROT_XRW, &oldProt);
	*reinterpret_cast<lm_byte_t*>(jmpAddr) = 0xE9;
	*reinterpret_cast<lm_address_t*>(jmpAddr + 1) = hkGetSteamId - jmpAddr - 5;
	LM_ProtMemory(jmpAddr, 5, oldProt, nullptr);

	return true;
}

namespace Hooks
{
	//TODO: Replace logging in hooks with Hook::name
	DetourHook<LogSteamPipeCall_t> LogSteamPipeCall("LogSteamPipeCall");

	DetourHook<IClientAppManager_PipeLoop_t> IClientAppManager_PipeLoop("IClientAppManager::PipeLoop");
	DetourHook<IClientApps_PipeLoop_t> IClientApps_PipeLoop("IClientApps::PipeLoop");
	DetourHook<IClientRemoteStorage_PipeLoop_t> IClientRemoteStorage_PipeLoop("IClientRemoteStorage::PipeLoop");

	DetourHook<IClientUser_BIsSubscribedApp_t> IClientUser_BIsSubscribedApp("IClientUser::BIsSubscribedApp");
	DetourHook<IClientUser_CheckAppOwnership_t> IClientUser_CheckAppOwnership("IClientUser::CheckAppOwnership");
	DetourHook<IClientUser_IsUserSubscribedAppInTicket_t> IClientUser_IsUserSubscribedAppInTicket("IClientUser::IsUserSubscribedAppInTicket");
	DetourHook<IClientUser_GetSubscribedApps_t> IClientUser_GetSubscribedApps("IClientUser::GetSubscribedApps");
	DetourHook<IClientUser_RequiresLegacyCDKey_t> IClientUser_RequiresLegacyCDKey("IClientUser::RequiresLegacyCDKey");

	VFTHook<IClientAppManager_BIsDlcEnabled_t> IClientAppManager_BIsDlcEnabled("IClientAppManager::BIsDlcEnabled");
	VFTHook<IClientAppManager_GetAppUpdateInfo_t> IClientAppManager_GetAppUpdateInfo("IClientAppManager::GetAppUpdateInfo");
	VFTHook<IClientAppManager_LaunchApp_t> IClientAppManager_LaunchApp("IClientAppManager::LaunchApp");
	VFTHook<IClientAppManager_IsAppDlcInstalled_t> IClientAppManager_IsAppDlcInstalled("IClientAppManager::IsAppDlcInstalled");

	VFTHook<IClientApps_GetDLCDataByIndex_t> IClientApps_GetDLCDataByIndex("IClientApps::GetDLCDataByIndex");
	VFTHook<IClientApps_GetDLCCount_t> IClientApps_GetDLCCount("IClientApps::GetDLCCount");

	VFTHook<IClientRemoteStorage_IsCloudEnabledForApp_t> IClientRemoteStorage_IsCloudEnabledForApp("IClientRemoteStorage::IsCloudEnabledForApp");

	lm_address_t IClientUser_GetSteamId;
}

bool Hooks::setup()
{
	g_pLog->debug("Hooks::setup()\n");

	IClientUser_GetSteamId = MemHlp::searchSignature("IClientUser::GetSteamId", Patterns::GetSteamId, g_modSteamClient, MemHlp::SigFollowMode::Relative);

	lm_address_t runningApp = MemHlp::searchSignature("RunningApp", Patterns::FamilyGroupRunningApp, g_modSteamClient, MemHlp::SigFollowMode::Relative);

	auto prologue = std::vector<lm_byte_t>({
		0x56, 0x57, 0xe5, 0x89, 0x55
	});
	lm_address_t stopPlayingBorrowedApp = MemHlp::searchSignature
	(
		"StopPlayingBorrowedApp",
		Patterns::StopPlayingBorrowedApp,
		g_modSteamClient,
		MemHlp::SigFollowMode::PrologueUpwards,
		&prologue[0],
		prologue.size()
	);

	//TODO: Automate these
	bool clientApps_PipeLoop = IClientApps_PipeLoop.setup
	(
		Patterns::IClientApps_PipeLoop,
		MemHlp::SigFollowMode::PrologueUpwards,
		&prologue[0],
		prologue.size(),
		&hkClientApps_PipeLoop
	);

	bool clientAppManager_PipeLoop = IClientAppManager_PipeLoop.setup
	(
		Patterns::IClientAppManager_PipeLoop,
		MemHlp::SigFollowMode::PrologueUpwards,
		&prologue[0],
		prologue.size(),
		&hkClientAppManager_PipeLoop
	);

	bool clientRemoteStorage_PipeLoop = IClientRemoteStorage_PipeLoop.setup
	(
		Patterns::IClientRemoteStorage_PipeLoop,
		MemHlp::SigFollowMode::PrologueUpwards,
		&prologue[0],
		prologue.size(),
		&hkClientRemoteStorage_PipeLoop
	);

	//TODO: Make this shit less verbose in case I fail my reversing & refactor for all this crap
	prologue = std::vector<lm_byte_t>({
		0x53, 0x56, 0x57, 0x55
	});
	bool requiresLegacyCDKey = IClientUser_RequiresLegacyCDKey.setup
	(
		Patterns::RequiresLegacyCDKey,
		MemHlp::SigFollowMode::PrologueUpwards,
		&prologue[0],
		prologue.size(),
		&hkClientUser_RequiresLegacyCDKey
	);

	bool succeeded =
		LogSteamPipeCall.setup(Patterns::LogSteamPipeCall, MemHlp::SigFollowMode::Relative, &hkLogSteamPipeCall)
		&& IClientUser_BIsSubscribedApp.setup(Patterns::IsSubscribedApp, MemHlp::SigFollowMode::Relative, &hkClientUser_BIsSubscribedApp)
		&& IClientUser_CheckAppOwnership.setup(Patterns::CheckAppOwnership, MemHlp::SigFollowMode::Relative, &hkClientUser_CheckAppOwnership)
		&& IClientUser_IsUserSubscribedAppInTicket.setup(Patterns::IsUserSubscribedAppInTicket, MemHlp::SigFollowMode::Relative, &hkClientUser_IsUserSubscribedAppInTicket)
		&& IClientUser_GetSubscribedApps.setup(Patterns::GetSubscribedApps, MemHlp::SigFollowMode::Relative, &hkClientUser_GetSubscribedApps)

		&& runningApp != LM_ADDRESS_BAD
		&& stopPlayingBorrowedApp != LM_ADDRESS_BAD
		&& IClientUser_GetSteamId != LM_ADDRESS_BAD

		&& clientApps_PipeLoop
		&& clientAppManager_PipeLoop
		&& clientRemoteStorage_PipeLoop
		&& requiresLegacyCDKey;

	if (!succeeded)
	{
		g_pLog->warn("Failed to find all patterns! Aborting...");
		return false;
	}

	//TODO: Elegantly move into Hooks::place()
	if (g_config.disableFamilyLock)
	{
		patchRetn(runningApp);
		patchRetn(stopPlayingBorrowedApp);
	}

	//Might move this into main()
	Hooks::place();
	return true;
}

void Hooks::place()
{
	//Detours
	LogSteamPipeCall.place();

	IClientApps_PipeLoop.place();
	IClientAppManager_PipeLoop.place();
	IClientRemoteStorage_PipeLoop.place();

	IClientUser_BIsSubscribedApp.place();
	IClientUser_CheckAppOwnership.place();
	IClientUser_IsUserSubscribedAppInTicket.place();
	IClientUser_GetSubscribedApps.place();
	IClientUser_RequiresLegacyCDKey.place();

	createAndPlaceSteamIdHook();
}

void Hooks::remove()
{
	//Detours
	LogSteamPipeCall.remove();

	IClientApps_PipeLoop.remove();
	IClientAppManager_PipeLoop.remove();
	IClientRemoteStorage_PipeLoop.remove();

	IClientUser_BIsSubscribedApp.remove();
	IClientUser_CheckAppOwnership.remove();
	IClientUser_IsUserSubscribedAppInTicket.remove();
	IClientUser_GetSubscribedApps.remove();
	IClientUser_RequiresLegacyCDKey.remove();

	//VFT Hooks
	IClientAppManager_BIsDlcEnabled.remove();
	IClientAppManager_GetAppUpdateInfo.remove();
	IClientAppManager_LaunchApp.remove();
	IClientAppManager_IsAppDlcInstalled.remove();

	IClientApps_GetDLCDataByIndex.remove();
	IClientApps_GetDLCCount.remove();

	IClientRemoteStorage_IsCloudEnabledForApp.remove();
	
	//TODO: Remove jmp
	if (hkGetSteamId != LM_ADDRESS_BAD)
	{
		LM_FreeMemory(hkGetSteamId, 0);
	}
}
