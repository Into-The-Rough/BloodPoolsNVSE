#include "Config.h"
#include <windows.h>
#include <cstdlib>

namespace Pools {
	Settings settings;

	static float ReadFloat(const char* path, const char* section, const char* key, float fallback)
	{
		char text[64];
		if (!GetPrivateProfileStringA(section, key, "", text, sizeof text, path) || !text[0]) return fallback;
		return static_cast<float>(atof(text));
	}

	static bool ReadBool(const char* path, const char* section, const char* key, bool fallback) { return GetPrivateProfileIntA(section, key, fallback, path) != 0; }

	void LoadConfig(const char* path)
	{
		Settings d;
		settings.enabled = ReadBool(path, "Main", "bEnabled", d.enabled);
		settings.poolScale = ReadFloat(path, "Main", "fPoolScale", d.poolScale);
		settings.poolMin = ReadFloat(path, "Main", "fPoolMin", d.poolMin);
		settings.poolMax = ReadFloat(path, "Main", "fPoolMax", d.poolMax);
		settings.defaultSpan = ReadFloat(path, "Main", "fDefaultSpan", d.defaultSpan);
		settings.lifetime = ReadFloat(path, "Main", "fLifetime", d.lifetime);
		settings.settleSpeed = ReadFloat(path, "Main", "fSettleSpeed", d.settleSpeed);
		settings.settleSeconds = ReadFloat(path, "Main", "fSettleSeconds", d.settleSeconds);
		settings.minWait = ReadFloat(path, "Main", "fMinWait", d.minWait);
		settings.maxWait = ReadFloat(path, "Main", "fMaxWait", d.maxWait);
		settings.maxDistance = ReadFloat(path, "Main", "fMaxDistance", d.maxDistance);
		settings.woundScale = ReadFloat(path, "Main", "fWoundScale", d.woundScale);
		settings.stumpScale = ReadFloat(path, "Main", "fStumpScale", d.stumpScale);
		settings.severedScale = ReadFloat(path, "Main", "fSeveredScale", d.severedScale);
		settings.maxBodyPools = GetPrivateProfileIntA("Main", "iMaxBodyPools", d.maxBodyPools, path);
		settings.explosions = ReadBool(path, "Main", "bExplosions", d.explosions);
		settings.explosionScale = ReadFloat(path, "Main", "fExplosionScale", d.explosionScale);
		settings.explosionDelay = ReadFloat(path, "Main", "fExplosionDelay", d.explosionDelay);
		settings.colour.x = ReadFloat(path, "Main", "fRed", d.colour.x);
		settings.colour.y = ReadFloat(path, "Main", "fGreen", d.colour.y);
		settings.colour.z = ReadFloat(path, "Main", "fBlue", d.colour.z);
		settings.pools = ReadBool(path, "Pools", "bPools", d.pools);
		settings.visualProbe = ReadBool(path, "Pools", "bVisualProbe", d.visualProbe);
		settings.poolDuration = ReadFloat(path, "Pools", "fDuration", d.poolDuration);
		settings.poolFade = ReadFloat(path, "Pools", "fFade", d.poolFade);
		settings.lift = ReadFloat(path, "Pools", "fLift", d.lift);
		settings.maxStep = ReadFloat(path, "Pools", "fMaxStep", d.maxStep);
		settings.slopeUp = ReadFloat(path, "Pools", "fSlopeUp", d.slopeUp);
		settings.slopeDown = ReadFloat(path, "Pools", "fSlopeDown", d.slopeDown);
		settings.lobes = ReadFloat(path, "Pools", "fLobes", d.lobes);
		settings.rateJitter = ReadFloat(path, "Pools", "fRateJitter", d.rateJitter);
		settings.creep = ReadFloat(path, "Pools", "fCreep", d.creep);
		settings.creepSeconds = ReadFloat(path, "Pools", "fCreepSeconds", d.creepSeconds);
		settings.sizeJitter = ReadFloat(path, "Pools", "fSizeJitter", d.sizeJitter);
		settings.durationJitter = ReadFloat(path, "Pools", "fDurationJitter", d.durationJitter);
		settings.tintJitter = ReadFloat(path, "Pools", "fTintJitter", d.tintJitter);
		settings.hueJitter = ReadFloat(path, "Pools", "fHueJitter", d.hueJitter);
		settings.raysPerTick = GetPrivateProfileIntA("Pools", "iRaysPerTick", d.raysPerTick, path);
		settings.raysPerFrame = GetPrivateProfileIntA("Pools", "iRaysPerFrame", d.raysPerFrame, path);
		settings.maxPools = GetPrivateProfileIntA("Pools", "iMaxPools", d.maxPools, path);
	}
}
