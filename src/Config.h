#pragma once
#include "Engine.h"

namespace Pools {
	//sizes in game units, times in game seconds
	struct Settings {
		bool enabled = true;
		float poolScale = 1.2f, poolMin = 60, poolMax = 300, defaultSpan = 100;
		float lifetime = 600;
		float settleSpeed = 2, settleSeconds = .3f, minWait = .5f, maxWait = 4;
		float maxDistance = 6000;
		float woundScale = .5f, stumpScale = 1.2f, severedScale = .35f;
		int maxBodyPools = 4;
		bool explosions = true;
		float explosionScale = 1.5f, explosionDelay = .5f;
		Engine::Vec3 colour{.706f, .094f, .027f};
		bool pools = true, visualProbe = true;
		float poolDuration = 6, poolFade = 5, lift = 3;
		float maxStep = 6, slopeUp = 20, slopeDown = 35;
		float lobes = .3f, rateJitter = .6f, creep = .2f, creepSeconds = 40;
		float sizeJitter = .2f, durationJitter = .35f, tintJitter = .15f, hueJitter = .2f;
		int raysPerTick = 6, raysPerFrame = 32, maxPools = 16;
	};

	extern Settings settings;
	void LoadConfig(const char* path);
}
