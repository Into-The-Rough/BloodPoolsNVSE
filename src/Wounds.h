#pragma once
#include <cstdint>

namespace Pools::Wounds {
	constexpr int kMaxPerBody = 6;

	struct Wound {
		int part = -1;
		float damage = 0;
		uint32_t flags = 0;
	};

	void Record(uint32_t actor, int part, float damage, uint32_t flags);
	int Take(uint32_t actor, int killingPart, Wound* out);
	int Merge(Wound* list, int count, int part, float damage, uint32_t flags);
	void Clear();
}
