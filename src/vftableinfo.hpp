#pragma once

namespace VFTIndexes
{
	namespace IClientApps
	{
		constexpr int RequestAppInfoUpdate = 7;
		constexpr int GetDLCCount = 8;
		constexpr int GetDLCDataByIndex = 9;
		constexpr int GetAppType = 10;
	}

	namespace IClientAppManager
	{
		constexpr int InstallApp = 0;
		constexpr int UninstallApp = 1;
		constexpr int LaunchApp = 2;
		constexpr int GetAppInstallState = 4;
		constexpr int IsAppDlcInstalled = 9;
		constexpr int BIsDlcEnabled = 11;
		constexpr int GetUpdateInfo = 20;
	}

	namespace IClientRemoteStorage
	{
		constexpr int IsCloudEnabledForApp = 24;
	}

	namespace IClientUser
	{
		constexpr int GetSteamID = 10;
	}

	namespace IClientUtils
	{
		constexpr int GetAppId = 19;
	}
}
