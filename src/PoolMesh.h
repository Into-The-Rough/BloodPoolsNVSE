#pragma once
#include "Engine.h"
#include "Decals.h"

namespace Pools::Mesh {
	enum class Fluid {
		kBlood,
		kOil,
		kGlow
	};

	void Init();
	bool Contains(const Engine::Object* object);
	constexpr int kHeightCasts = Decals::kGroundCasts + 1;
	Decals::Cast Spawn(const Engine::Vec3& ground, float radius, uint32_t filter, uint32_t seed, Fluid fluid);
	void Update(float dt);
	void Clear();
}
