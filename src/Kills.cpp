#include "Kills.h"
#include "Hooks.h"

namespace Pools::Kills {
	using HitMe = void(__thiscall*)(Engine::Form*, Engine::HitData*, char);
	constexpr int kCapacity = 32;
	static Kill s_kills[kCapacity];
	static int s_count = 0;
	static DWORD s_mainThread = 0;

	//off-thread hitme queues work for the main thread
	static void Observe(Engine::Form* target, Engine::HitData* data, char flag, uintptr_t original)
	{
		bool main = GetCurrentThreadId() == s_mainThread;
		bool watch = main && data && Engine::IsActor(target) && data->target == target && target->id != Engine::kPlayerRefID && Engine::LifeState(target) == 0;
		reinterpret_cast<HitMe>(original)(target, data, flag);
		//a hit another plugin absorbed comes back with its damage zeroed
		if (!watch || !(data->healthDamage > 0)) return;
		//nested hits can queue the death first
		for (int i = 0; i < s_count; ++i)
			if (s_kills[i].target == target->id) {
				Kill& queued = s_kills[i];
				queued.woundCount = Wounds::Merge(queued.wounds, queued.woundCount, data->location, data->healthDamage, data->flags);
				return;
			}
		Wounds::Record(target->id, data->location, data->healthDamage, data->flags);
		if (!Engine::IsDeadState(Engine::LifeState(target)) || s_count >= kCapacity) return;
		Kill& k = s_kills[s_count++];
		k.target = target->id;
		k.location = data->location;
		k.explosion = (data->flags & Engine::kHitExplosion) != 0;
		k.impact = data->impact;
		k.woundCount = Wounds::Take(target->id, data->location, k.wounds);
	}

	template<uintptr_t kSite> class HitSite {
		static inline CallDetour s_detour;

		static void __fastcall Hook(Engine::Form* target, void*, Engine::HitData* data, char flag) { Observe(target, data, flag, s_detour.Original()); }

	public:
		bool Install() { return s_detour.ReplaceCall(kSite, Hook); }
	};

	bool Install()
	{
		s_mainThread = GetCurrentThreadId();
		bool ok = HitSite<0x87C4DA>().Install(); //deferred hits
		ok &= HitSite<0x89A738>().Install(); //actor::combathit
		ok &= HitSite<0x8B91E1>().Install();
		ok &= HitSite<0x9B0503>().Install(); //explosion damage
		ok &= HitSite<0x9C1E96>().Install(); //projectile::processimpacts
		ok &= HitSite<0x9CBDE8>().Install(); //combatprocedureattacklow::update
		return ok;
	}

	int Drain(Kill* out, int capacity)
	{
		int n = s_count < capacity ? s_count : capacity;
		for (int i = 0; i < n; ++i) out[i] = s_kills[i];
		s_count = 0;
		return n;
	}

	void Clear()
	{
		s_count = 0;
		Wounds::Clear();
	}
}
