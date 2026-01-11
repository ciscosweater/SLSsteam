#include "apps.hpp"

#include "../sdk/CAppOwnershipInfo.hpp"
#include "../sdk/CProtoBufMsgBase.hpp"
#include "../sdk/CSteamEngine.hpp"
#include "../sdk/CUser.hpp"
#include "../sdk/IClientApps.hpp"

#include "../config.hpp"
#include "../globals.hpp"


bool Apps::applistRequested;
std::map<uint32_t, int> Apps::appIdOwnerOverride;

bool Apps::unlockApp(uint32_t appId, CAppOwnershipInfo* info, uint32_t ownerId)
{
	//Changing the purchased field is enough, but just for nicety in the Steamclient UI we change the owner too
	info->ownerSteamId = ownerId;
	info->familyShared = ownerId != g_currentSteamId;

	info->purchased = true;
	//Unnessecary but whatever
	info->permanent = !info->familyShared;

	//Found in backtrace
	info->releaseState = 4;
	info->field10_0x25 = 0;
	//Seems to do nothing in particular, some dlc have this as 1 so I uncomented this for now. Might be free stuff?
	//pOwnershipInfo->field27_0x36 = 1;

	//g_pLog->debug("Unlocked %u for %u\n", appId, ownerId);
	g_pLog->debug("Unlocked %u\n", appId);
	return true;
}

bool Apps::unlockApp(uint32_t appId, CAppOwnershipInfo* info)
{
	return unlockApp(appId, info, g_currentSteamId);
}

bool Apps::checkAppOwnership(uint32_t appId, CAppOwnershipInfo* pInfo)
{
	//Wait Until GetSubscribedApps gets called once to let Steam request and populate legit data first.
	//Afterwards modifying should hopefully not affect false positives anymore
	if (!applistRequested || !pInfo || !g_currentSteamId)
	{
		return false;
	}

	const uint32_t denuvoOwner = g_config.getDenuvoGameOwner(appId);

	//Do not modify Denuvo enabled Games
	if (denuvoOwner && denuvoOwner != g_currentSteamId)
	{
		//Would love to log the SteamId, but for users anonymity I won't
		g_pLog->once("Skipping %u because it's a Denuvo game from someone else\n", appId);
		return false;
	}

	if (g_config.shouldExcludeAppId(appId))
	{
		return false;
	}

	if (!g_config.isAddedAppId(appId) && (!g_config.playNotOwnedGames.get() || pInfo->purchased))
	{
		return false;
	}

	//Returning false after we modify data shouldn't cause any problems because it should just get discarded
	if (!g_pClientApps)
	{
		return false;
	}

	auto type = g_pClientApps->getAppType(appId);
	if (type == APPTYPE_DLC) //Don't touch DLC here, otherwise downloads might break. Hopefully this won't decrease compatibility
	{
		return false;
	}

	if (g_config.automaticFilter.get())
	{
		switch(type)
		{
			case APPTYPE_APPLICATION:
			case APPTYPE_GAME:
				break;

			default:
				return false;
		}
	}

	unlockApp(appId, pInfo);

	return true;
}

void Apps::getSubscribedApps(uint32_t* appList, size_t size, uint32_t& count)
{
	//Valve calls this function twice, once with size of 0 then again
	if (!size || !appList)
	{
		count = count + g_config.addedAppIds.get().size();
		return;
	}

	//TODO: Maybe Add check if AppId already in list before blindly appending
	for(auto& appId : g_config.addedAppIds.get())
	{
		appList[count++] = appId;
	}

	applistRequested = true;
}

bool Apps::shouldDisableCloud(uint32_t appId)
{
	return !g_pSteamEngine->getUser(0)->checkAppOwnership(appId);
}

bool Apps::shouldDisableCDKey(uint32_t appId)
{
	return !g_pSteamEngine->getUser(0)->checkAppOwnership(appId);
}

bool Apps::shouldDisableUpdates(uint32_t appId)
{
	//Using AdditionalApps here aswell so users can manually block updates
	return g_config.isAddedAppId(appId) || !g_pSteamEngine->getUser(0)->checkAppOwnership(appId);
}

void Apps::sendGamesPlayed(CMsgClientGamesPlayed* msg)
{
	bool owned = false;

	for(int i = 0; i < msg->games_played_size(); i++)
	{
		auto game = msg->mutable_games_played(i);

		if (!game->game_id())
		{
			continue;
		}

		if(!owned && g_pSteamEngine->getUser(0)->checkAppOwnership(game->game_id()))
		{
			owned = true;
		}

		if (g_config.disableFamilyLock.get())
		{
			game->set_owner_id(1);
		}

		g_pLog->debug("Playing game %llu with flags %u\n", game->game_id(), game->game_flags());
	}

	if (owned)
	{
		return;
	}

	const int games = msg->games_played_size();
	const auto statusApp = games ? g_config.unownedStatus.get() : g_config.idleStatus.get();
	if (statusApp.appId)
	{
		//pMsg->send(); //Send original message first, otherwise Valve's backend might fuck up the order
		//Only happens in owned games for some reason, so idk

		// If playing a game and UnownedStatus is set, use the actual game's name
		// for the status display, but only if Title is not already configured
		std::string gameName = statusApp.title;
		if (games && gameName.empty())
		{
			uint32_t playedAppId = msg->games_played(0).game_id();
			std::string actualName = g_config.getAppName(playedAppId);
			if (!actualName.empty())
			{
				gameName = actualName;
				g_pLog->debug("Using game name '%s' for UnownedStatus\n", gameName.c_str());
			}
		}

		auto game = msg->add_games_played();
		game->set_game_id(statusApp.appId);
		game->set_game_extra_info(gameName);
		game->set_game_flags(0);
		//game->set_game_flags(EGAMEFLAG_MULTIPLAYER);
	}
}

void Apps::sendPICSInfoRequest(CMsgClientPICSProductInfoRequest* msg)
{
	const auto tokens = g_config.appTokens.get();

	for(int i = 0; i < msg->apps_size(); i++)
	{
		auto app = msg->mutable_apps(i);
		if (tokens.contains(app->appid()))
		{
			app->set_access_token(tokens.at(app->appid()));
			g_pLog->debug("Used access token from config for %u\n", app->appid());
		}
	}
}

void Apps::sendMsg(CProtoBufMsgBase *msg)
{
	switch(msg->type)
	{
		case EMSG_PICS_PRODUCTINFO_REQUEST:
			sendPICSInfoRequest(reinterpret_cast<CMsgClientPICSProductInfoRequest*>(msg->body));
			break;

		case EMSG_GAMESPLAYED:
		case EMSG_GAMESPLAYED_NO_DATABLOB:
		case EMSG_GAMESPLAYED_WITH_DATABLOB:
			sendGamesPlayed(reinterpret_cast<CMsgClientGamesPlayed*>(msg->body));
			break;
	}
}
