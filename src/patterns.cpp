#include "patterns.hpp"
#include "globals.hpp"
#include "memhlp.hpp"

#include "libmem/libmem.h"

#include <algorithm>
#include <memory>


CPattern Patterns::UpdateAppOwnershipTicket
(
	"IClientUser::UpdateAppOwnershipTicket",
	"E8 ? ? ? ? E9 ? ? ? ? ? ? ? ? ? ? 8D 45 ? 89 45 ? EB",
	MemHlp::SigFollowMode::Relative
);

CPattern::CPattern(const char* name, const char* pattern, MemHlp::SigFollowMode followMode)
	:
	name(name),
	pattern(pattern),
	followMode(followMode)
{

}

bool CPattern::find()
{
	address = MemHlp::searchSignature(name.c_str(), pattern.c_str(), g_modSteamClient, followMode);
	g_pLog->once("Address at %p\n", address);
	return address != LM_ADDRESS_BAD;
}

bool Patterns::init()
{
	//TODO: Make CPattern auto aggregate into a list then iterate that to find all
	return UpdateAppOwnershipTicket.find();
}
