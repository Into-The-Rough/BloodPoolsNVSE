#include "PluginAPI.h"
#include "Config.h"
#include "Kills.h"
#include "Pools.h"
#include <windows.h>
#include <cstdio>
#include <cstring>

namespace Pools {
	static char s_iniPath[MAX_PATH];
	static bool s_ready = false;

	static void Message(NVSEMessagingInterface::Message* msg)
	{
		switch (msg->type) {
			case NVSEMessagingInterface::DeferredInit:
				s_ready = Kills::Install() && Corpses::Init();
				break;
			case NVSEMessagingInterface::MainGameLoop:
				if (s_ready) Corpses::Update();
				break;
			case NVSEMessagingInterface::ReloadConfig:
				if (msg->data && msg->dataLen && !_stricmp(static_cast<const char*>(msg->data), "BloodPoolsNVSE")) LoadConfig(s_iniPath);
				break;
			case NVSEMessagingInterface::ExitGame:
			case NVSEMessagingInterface::ExitGameConsole:
			case NVSEMessagingInterface::ExitToMainMenu:
			case NVSEMessagingInterface::PreLoadGame:
			case NVSEMessagingInterface::PostLoadGame:
			case NVSEMessagingInterface::NewGame: Corpses::Clear(); break;
		}
	}

	static bool Load(const NVSEInterface* nvse)
	{
		const char* runtime = nvse->GetRuntimeDirectory();
		if (!runtime) return false;
		if (sprintf_s(s_iniPath, "%sData\\config\\BloodPoolsNVSE.ini", runtime) < 0) return false;
		LoadConfig(s_iniPath);
		auto messages = static_cast<NVSEMessagingInterface*>(nvse->QueryInterface(kInterface_Messaging));
		return messages && messages->RegisterListener(nvse->GetPluginHandle(), "NVSE", Message);
	}
}

extern "C" __declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info)
{
	info->infoVersion = 1;
	info->name = "BloodPoolsNVSE";
	info->version = 10000;
	return !nvse->isEditor && !nvse->isNogore;
}

extern "C" __declspec(dllexport) bool NVSEPlugin_Load(const NVSEInterface* nvse) { return Pools::Load(nvse); }
