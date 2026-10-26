#include "Wounds.h"
#include <windows.h>

namespace Pools::Wounds {
	constexpr int kCapacity = 128;
	constexpr ULONGLONG kMemoryMs = 180000;

	struct Entry {
		uint32_t actor = 0;
		Wound wound;
		ULONGLONG at = 0;
	};

	static Entry s_entries[kCapacity];
	static int s_count = 0;

	static void Forget(int i) { s_entries[i] = s_entries[--s_count]; }

	void Record(uint32_t actor, int part, float damage, uint32_t flags)
	{
		if (part < 0) return;
		ULONGLONG now = GetTickCount64();
		int oldest = -1;
		for (int i = 0; i < s_count; ++i) {
			Entry& e = s_entries[i];
			if (e.actor == actor && e.wound.part == part) {
				e.wound.damage += damage;
				e.wound.flags |= flags;
				e.at = now;
				return;
			}
			if (oldest < 0 || e.at < s_entries[oldest].at) oldest = i;
		}
		if (s_count >= kCapacity) Forget(oldest);
		s_entries[s_count++] = {actor, {part, damage, flags}, now};
	}

	int Take(uint32_t actor, int killingPart, Wound* out)
	{
		ULONGLONG now = GetTickCount64();
		int n = 0;
		for (int i = 0; i < s_count; ++i)
			if (s_entries[i].actor == actor && s_entries[i].wound.part == killingPart) out[n++] = s_entries[i].wound;
		for (int i = 0; i < s_count;) {
			Entry& e = s_entries[i];
			if (e.actor == actor && e.wound.part != killingPart && n < kMaxPerBody && now - e.at <= kMemoryMs) out[n++] = e.wound;
			if (e.actor == actor || now - e.at > kMemoryMs) Forget(i);
			else ++i;
		}
		return n;
	}

	int Merge(Wound* list, int count, int part, float damage, uint32_t flags)
	{
		if (part < 0) return count;
		for (int i = 0; i < count; ++i)
			if (list[i].part == part) {
				list[i].damage += damage;
				list[i].flags |= flags;
				return count;
			}
		if (count < kMaxPerBody) list[count++] = {part, damage, flags};
		return count;
	}

	void Clear() { s_count = 0; }
}
