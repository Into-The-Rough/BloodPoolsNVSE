#pragma once
#include "Engine.h"

namespace Pools::Decals {
	struct Request {
		Engine::Form* corpse = nullptr;
		Engine::Vec3 pos;
		float width = 100, height = 100;
		Engine::Form* textureSet = nullptr;
		Engine::Vec3 colour;
		float lifetime = 600;
		float yaw = 0;
		int quadrant = 0;
	};

	struct Surface {
		Engine::Vec3 pos, normal;
		Engine::Object* node = nullptr;
	};

	enum class Cast {
		kHit,
		kMiss,
		kDeferred
	};

	constexpr int kGroundCasts = 4;
	struct PickStats {
		uint64_t ground = 0, visual = 0;
	};

	PickStats TakePickStats();
	void BeginFrame(int casts);
	int CastsLeft();
	Cast Ground(uint32_t filter, const Engine::Vec3& from, float reach, Surface& out);
	Cast Visual(const Engine::Vec3& from, float reach, Engine::Vec3& hit);
	Cast Place(const Request& request);
	void EndFrame();
}
