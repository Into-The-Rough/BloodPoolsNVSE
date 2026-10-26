#pragma once
#include "Engine.h"
#include "Wounds.h"

namespace Pools::Kills {
	struct Kill {
		uint32_t target = 0;
		int location = -1;
		bool explosion = false;
		Engine::Vec3 impact;
		Wounds::Wound wounds[Wounds::kMaxPerBody];
		int woundCount = 0;
	};

	bool Install();
	int Drain(Kill* out, int capacity);
	void Clear();
}
