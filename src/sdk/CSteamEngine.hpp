#pragma once
#include "IClientUser.hpp"

#include "../log.hpp"

#include "libmem/libmem.h"

class CInterfaceList;

class CSteamEngine
{
	template<typename T>
	T getInterface(lm_address_t offset)
	{
		const auto ifaces = *reinterpret_cast<lm_address_t*>(this + 0x960);
		g_pLog->debug("ifaces: %p\n", ifaces);
		const auto magicFn = **reinterpret_cast<lm_address_t(***)()>(ifaces + 0x94);
		g_pLog->debug("MagicFN: %p\n", magicFn);
		const auto ptr = magicFn();
		g_pLog->debug("ptr: %p\n", ptr);

		return reinterpret_cast<T>(ptr + offset);
	}

public:
	IClientUser* getSteamUser();
};
