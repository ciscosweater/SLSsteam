#pragma once

#include <cstdint>

class CUser;

class CSteamEngine
{
public:
	//uint8_t __pad0x0[0xa54]; //0x0
	//CUser*** userMap; //0xa54
	//uint8_t __pad0xa54[0x8]; //0xa58
	//uint32_t userCount; //0xa5c
	
	CUser* getUser(uint32_t index);
	void setAppIdForCurrentPipe(uint32_t appId);
};

extern CSteamEngine* g_pSteamEngine;
