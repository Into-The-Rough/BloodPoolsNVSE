#pragma once
#include <cstdint>

namespace Pools::Rig {
	constexpr int kRings = 3, kSegments = 16;
	constexpr int kVertices = 1 + kRings * kSegments;
	constexpr int kTriangles = kSegments + 2 * kSegments * (kRings - 1);

	struct Point {
		float dirX = 0, dirY = 0;
		float depth = 0;
		//inward neighbour, -1 at the centre
		int parent = -1;
	};

	struct Settings {
		float radius = 48;
		float duration = 6;
		float lobes = .3f, rateJitter = .6f;
		float creep = .2f, creepSeconds = 40;
		float maxStep = 6, slopeUp = 20, slopeDown = 35;
		float reprobe = 4;
	};

	struct State {
		float age = 0, progress = 0;
		uint32_t seed = 1;
		float radius[kVertices] = {};
		float target[kVertices] = {};
		float z[kVertices] = {};
		float probedAt[kVertices] = {};
		bool locked[kVertices] = {};
		bool probed[kVertices] = {};
		bool capped[kVertices] = {};
		float reach[kSegments] = {};
		float speed[kSegments] = {};
		float segment[kSegments] = {};
		bool shaped = false;
		int cursor = 0;
	};

	struct Vertex {
		float x, y, z;
	};

	struct UV {
		float u, v;
	};

	void Layout(Point* out);
	float Random(uint32_t seed, uint32_t salt);
	void Triangles(uint16_t* out);
	float Flow(float t, uint32_t seed);
	enum class Reading {
		kGround,
		kNone,
		kDeferred
	};

	using Probe = Reading (*)(void* context, float x, float y, float& z);
	int Step(State& s, const Settings& settings, const Point* points, float dt, int rays, Probe probe, void* context);
	bool Settled(const State& s);
	void Write(const State& s, const Settings& settings, const Point* points, float lift, Vertex* vertices, UV* uvs, float* alpha);
}
