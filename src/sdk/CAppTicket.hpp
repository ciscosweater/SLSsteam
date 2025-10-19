#pragma once

#include <cstdint>


class CAppTicket
{
public:
	uint32_t extraData[4 * sizeof(uint32_t)];
	uint8_t bytes[0x400];

	uint32_t getSteamId() const;
	uint32_t getAppId() const;
	uint32_t getSize() const;
};

class CEncryptedAppTicket
{
public:
	uint32_t steamId;
	uint32_t size;
	uint8_t bytes[0x1000];
};
