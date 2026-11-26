#include "Rig.h"
#include <cmath>

namespace Pools::Rig {
	constexpr float kPi = 3.14159265f;

	void Layout(Point* out)
	{
		out[0] = {};
		for (int r = 1; r <= kRings; ++r)
			for (int s = 0; s < kSegments; ++s) {
				float a = 2 * kPi * s / kSegments;
				Point& p = out[1 + (r - 1) * kSegments + s];
				p.dirX = std::cos(a);
				p.dirY = std::sin(a);
				p.depth = float(r) / kRings;
				p.parent = r == 1 ? 0 : 1 + (r - 2) * kSegments + s;
			}
	}

	void Triangles(uint16_t* out)
	{
		int n = 0;
		for (int s = 0; s < kSegments; ++s) {
			out[n++] = 0;
			out[n++] = uint16_t(1 + s);
			out[n++] = uint16_t(1 + (s + 1) % kSegments);
		}
		for (int r = 1; r < kRings; ++r) {
			int a = 1 + (r - 1) * kSegments, b = 1 + r * kSegments;
			for (int s = 0; s < kSegments; ++s) {
				int s1 = (s + 1) % kSegments;
				out[n++] = uint16_t(a + s);
				out[n++] = uint16_t(b + s);
				out[n++] = uint16_t(b + s1);
				out[n++] = uint16_t(a + s);
				out[n++] = uint16_t(b + s1);
				out[n++] = uint16_t(a + s1);
			}
		}
	}

	float Random(uint32_t seed, uint32_t salt)
	{
		uint32_t h = seed ^ (salt * 0x9E3779B1u);
		h ^= h >> 16;
		h *= 0x7FEB352Du;
		h ^= h >> 15;
		h *= 0x846CA68Bu;
		h ^= h >> 16;
		return float(h >> 8) * (1.f / 16777216.f);
	}

	float Flow(float t, uint32_t seed)
	{
		if (t <= 0) return 0;
		if (t >= 1) return 1;
		float held = 0, total = 0;
		for (uint32_t k = 0; k < 3; ++k) {
			float start = .15f + .25f * k + .12f * Random(seed, k * 2);
			float width = .03f + .06f * Random(seed, k * 2 + 1);
			total += width;
			if (t > start) held += t < start + width ? t - start : width;
		}
		float u = (t - held) / (1 - total);
		if (u < 0) u = 0;
		if (u > 1) u = 1;
		return 1 - (1 - u) * (1 - u);
	}

	static int SegmentOf(int i) { return (i - 1) % kSegments; }

	static void Smooth(float* v)
	{
		float copy[kSegments];
		for (int k = 0; k < kSegments; ++k) copy[k] = v[k];
		for (int k = 0; k < kSegments; ++k) v[k] = .25f * copy[(k + kSegments - 1) % kSegments] + .5f * copy[k] + .25f * copy[(k + 1) % kSegments];
	}

	static void Wave(uint32_t seed, uint32_t salt, float* out)
	{
		float lo = 1e9f, hi = -1e9f;
		for (int k = 0; k < kSegments; ++k) {
			float a = 2 * kPi * k / kSegments, v = 0;
			for (uint32_t h = 1; h <= 3; ++h) v += (.4f + .6f * Random(seed, salt + 2 * h)) / h * std::sin(h * a + 2 * kPi * Random(seed, salt + 2 * h + 1));
			out[k] = v;
			if (v < lo) lo = v;
			if (v > hi) hi = v;
		}
		for (int k = 0; k < kSegments; ++k) out[k] = hi > lo ? (out[k] - lo) / (hi - lo) : 0;
	}

	static void Shape(State& s, const Settings& settings)
	{
		float reach[kSegments], pace[kSegments];
		Wave(s.seed, 100, reach);
		Wave(s.seed, 200, pace);
		for (int k = 0; k < kSegments; ++k) {
			s.reach[k] = 1 - settings.lobes * reach[k];
			s.speed[k] = 1 + settings.rateJitter * pace[k];
		}
		s.shaped = true;
	}

	constexpr float kFinalTolerance = 1e-3f;

	static float Clamp01(float v) { return std::fmin(std::fmax(v, 0.f), 1.f); }

	static void Lock(State& s, const Point* points, int i)
	{
		s.locked[i] = true;
		for (int k = 0; k < kVertices; ++k)
			if (points[k].parent == i && !s.locked[k]) Lock(s, points, k);
	}

	int Step(State& s, const Settings& settings, const Point* points, float dt, int rays, Probe probe, void* context)
	{
		if (!s.shaped) Shape(s, settings);
		s.age += dt;
		float raw[kSegments];
		for (int k = 0; k < kSegments; ++k) {
			float t = settings.duration > 0 ? s.age * s.speed[k] / settings.duration : 1;
			float seep = settings.creepSeconds > 0 ? Clamp01(s.age * s.speed[k] / (settings.duration + settings.creepSeconds)) : 1;
			raw[k] = (1 - settings.creep) * Flow(t, s.seed + 0x632BE5ABu * (k + 1)) + settings.creep * seep;
		}
		Smooth(raw);
		s.progress = 1;
		for (int k = 0; k < kSegments; ++k) {
			s.segment[k] = raw[k];
			if (raw[k] < s.progress) s.progress = raw[k];
		}
		for (int i = 1; i < kVertices; ++i) {
			if (s.locked[i]) continue;
			int k = SegmentOf(i);
			float target = points[i].depth * settings.radius * s.reach[k] * s.segment[k];
			int parent = points[i].parent;
			if (parent > 0 && s.locked[parent] && target > s.radius[parent]) target = s.radius[parent];
			s.target[i] = target;
		}
		//outer rings first
		for (int i = kVertices - 1; i > 0; --i) {
			int parent = points[i].parent;
			float bound = s.locked[i] ? s.radius[i] : s.target[i];
			if (parent > 0 && !s.locked[parent] && s.target[parent] > bound) s.target[parent] = bound;
		}
		for (int i = 1; i < kVertices; ++i) {
			if (s.locked[i]) continue;
			float limit = s.probedAt[i] + settings.reprobe;
			s.capped[i] = probe && s.target[i] > limit;
			s.radius[i] = s.capped[i] ? limit : s.target[i];
		}
		for (int i = kVertices - 1; i > 0; --i) {
			int parent = points[i].parent;
			if (parent > 0 && !s.locked[parent] && s.radius[parent] > s.radius[i]) s.radius[parent] = s.radius[i];
		}
		for (int i = 1; i < kVertices; ++i)
			if (!s.locked[i] && !s.probed[i]) s.z[i] = points[i].parent >= 0 ? s.z[points[i].parent] : 0;
		if (!probe) return 0;
		int budget = rays;
		static_assert(kVertices <= 64);
		uint64_t sampled = 0;
		for (int n = 0; n < kVertices && budget > 0; ++n) {
			int i = 1 + (s.cursor + n) % (kVertices - 1);
			if (s.locked[i]) continue;
			bool due = !s.probed[i] || s.capped[i] || s.probedAt[i] - s.radius[i] >= settings.reprobe || (s.progress >= 1 && std::fabs(s.radius[i] - s.probedAt[i]) >= kFinalTolerance);
			if (!due) continue;
			float x = points[i].dirX * s.radius[i], y = points[i].dirY * s.radius[i], z = 0;
			Reading reading;
			uint64_t bit = uint64_t{1} << i;
			//radii and the scene stay fixed during this step
			if (sampled & bit) {
				z = s.z[i];
				reading = Reading::kGround;
			} else reading = probe(context, x, y, z);
			if (reading == Reading::kDeferred) break;
			--budget;
			s.cursor = i;
			if (reading == Reading::kNone) {
				Lock(s, points, i);
				continue;
			}
			int parent = points[i].parent;
			float parentZ = parent >= 0 ? s.z[parent] : 0;
			float run = s.radius[i] - (parent >= 0 ? s.radius[parent] : 0);
			float rise = z - parentZ;
			//short runs amplify slope noise
			constexpr float kMinRun = 8;
			float angle = run >= kMinRun ? std::atan2(rise, run) * 180 / kPi : 0;
			if (std::fabs(rise) > settings.maxStep || angle > settings.slopeUp || angle < -settings.slopeDown) {
				Lock(s, points, i);
				continue;
			}
			s.z[i] = z;
			s.probed[i] = true;
			s.probedAt[i] = s.radius[i];
			sampled |= bit;
		}
		return rays - budget;
	}

	bool Settled(const State& s)
	{
		if (s.progress < 1) return false;
		for (int i = 1; i < kVertices; ++i)
			if (!s.locked[i] && (!s.probed[i] || s.capped[i] || std::fabs(s.radius[i] - s.target[i]) >= kFinalTolerance || std::fabs(s.radius[i] - s.probedAt[i]) >= kFinalTolerance)) return false;
		return true;
	}

	void Write(const State& s, const Settings& settings, const Point* points, float lift, Vertex* vertices, UV* uvs, float* alpha)
	{
		for (int i = 0; i < kVertices; ++i) {
			const Point& p = points[i];
			vertices[i] = {p.dirX * s.radius[i], p.dirY * s.radius[i], s.z[i] + lift};
			float full = i ? settings.radius * s.reach[SegmentOf(i)] : 1;
			float reveal = full > 0 ? .5f * s.radius[i] / full : 0;
			uvs[i] = {.5f + p.dirX * reveal, .5f - p.dirY * reveal};
			alpha[i] = s.locked[i] || p.depth >= 1 ? 0 : 1.f;
		}
	}
}
