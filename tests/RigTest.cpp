#undef NDEBUG
#include "../src/Rig.h"
#include <cassert>
#include <cmath>
#include <cstdio>

using namespace Pools::Rig;

static Reading Flat(void*, float, float, float& z)
{
	z = 0;
	return Reading::kGround;
}

static Reading Starved(void*, float, float, float&) { return Reading::kDeferred; }

static Reading Kerb(void*, float x, float, float& z)
{
	z = x > 20 ? 12.f : 0.f;
	return Reading::kGround;
}

struct Surface {
	int calls = 0;
	float height = 1;
	Reading reading = Reading::kGround;
};

static Reading Sample(void* context, float, float, float& z)
{
	auto& surface = *static_cast<Surface*>(context);
	++surface.calls;
	z = surface.height;
	return surface.reading;
}

static State GrowingVertex()
{
	State s;
	for (int i = 2; i < kVertices; ++i) {
		s.locked[i] = true;
		s.radius[i] = 100;
	}
	return s;
}

static void RepeatedProbe(const Point* points)
{
	Settings settings;
	settings.duration = settings.creepSeconds = settings.lobes = 0;
	Surface surface;
	State s = GrowingVertex();
	TakeProbeStats();
	assert(Step(s, settings, points, .01f, 6, Sample, &surface) == 2);
	assert(surface.calls == 1 && s.z[1] == 1 && s.probed[1] && !s.locked[1]);
	auto stats = TakeProbeStats();
	assert(stats.calls == 1 && stats.reused == 1 && stats.deferred == 0);
	assert(!Settled(s));
	surface.height = 2;
	assert(Step(s, settings, points, .01f, 6, Sample, &surface) == 1);
	assert(surface.calls == 2 && s.z[1] == 2 && !s.locked[1]);
	surface.reading = Reading::kNone;
	assert(Step(s, settings, points, .01f, 6, Sample, &surface) == 1);
	assert(surface.calls == 3 && s.locked[1]);

	s = GrowingVertex();
	surface.reading = Reading::kDeferred;
	assert(Step(s, settings, points, .01f, 6, Sample, &surface) == 0);
	assert(surface.calls == 4 && !s.probed[1] && !s.locked[1]);
	surface.reading = Reading::kGround;
	surface.height = 3;
	assert(Step(s, settings, points, .01f, 6, Sample, &surface) == 2);
	assert(surface.calls == 5 && s.z[1] == 3);

	State other = GrowingVertex();
	surface.height = 4;
	assert(Step(other, settings, points, .01f, 6, Sample, &surface) == 2);
	assert(surface.calls == 6 && other.z[1] == 4 && s.z[1] == 3);
}

int main()
{
	Point points[kVertices];
	Layout(points);
	RepeatedProbe(points);
	uint16_t tris[kTriangles * 3];
	Triangles(tris);
	for (int i = 0; i < kTriangles * 3; ++i) assert(tris[i] < kVertices);
	assert(points[1].parent == 0 && points[1 + kSegments].parent == 1 && std::fabs(points[1 + 2 * kSegments].depth - 1) < 1e-6f);

	float last = 0;
	for (int k = 0; k <= 100; ++k) {
		float p = Flow(k / 100.f, 7);
		assert(p >= last - 1e-6f && p >= 0 && p <= 1);
		last = p;
	}
	assert(std::fabs(Flow(1, 7) - 1) < 1e-6f && Flow(0, 7) == 0);
	bool differs = false;
	for (int k = 1; k < 100 && !differs; ++k) differs = std::fabs(Flow(k / 100.f, 7) - Flow(k / 100.f, 8)) > 1e-3f;
	assert(differs);

	Settings settings;
	settings.radius = 48;
	settings.duration = 1;
	settings.creepSeconds = 1;
	State s;
	s.seed = 3;
	float lastRadius[kVertices] = {};
	for (int k = 0; k < 400; ++k) {
		Step(s, settings, points, .01f, 8, Flat, nullptr);
		for (int i = 1; i < kVertices; ++i) {
			assert(s.radius[i] >= lastRadius[i] - 1e-4f);
			lastRadius[i] = s.radius[i];
		}
	}
	assert(std::fabs(s.progress - 1) < 1e-5f && Settled(s));
	float longest = 0, shortest = 2;
	for (int k = 0; k < kSegments; ++k) {
		longest = std::fmax(longest, s.reach[k]);
		shortest = std::fmin(shortest, s.reach[k]);
	}
	assert(std::fabs(longest - 1) < 1e-6f && shortest < .99f && shortest >= 1 - settings.lobes - 1e-6f);
	for (int i = 1; i < kVertices; ++i) {
		assert(!s.locked[i]);
		assert(std::fabs(s.radius[i] - points[i].depth * 48 * s.reach[(i - 1) % kSegments]) < 1e-3f);
	}
	Vertex v[kVertices];
	UV uv[kVertices];
	float alpha[kVertices];
	Write(s, settings, points, 1.5f, v, uv, alpha);
	int rim = 1 + 2 * kSegments;
	for (int i = 0; i < kVertices; ++i) assert(alpha[i] == (i >= rim ? 0.f : 1.f));
	assert(std::fabs(v[0].z - 1.5f) < 1e-6f);
	assert(std::fabs(uv[rim].u - 1) < 1e-3f && std::fabs(uv[0].u - .5f) < 1e-6f);

	State m;
	m.seed = 3;
	for (int k = 0; k < 30; ++k) Step(m, settings, points, .01f, 8, Flat, nullptr);
	float lo = 2, hi = 0;
	for (int k = 0; k < kSegments; ++k) {
		lo = std::fmin(lo, m.segment[k]);
		hi = std::fmax(hi, m.segment[k]);
	}
	assert(hi - lo > .02f);

	Settings round = settings;
	round.lobes = 0;
	round.rateJitter = 0;
	State r;
	r.seed = 3;
	for (int k = 0; k < 400; ++k) Step(r, round, points, .01f, 8, Flat, nullptr);
	for (int i = 1; i < kVertices; ++i) assert(std::fabs(r.radius[i] - points[i].depth * 48) < 1e-3f);

	State k;
	k.seed = 3;
	for (int n = 0; n < 400; ++n) Step(k, settings, points, .01f, 8, Kerb, nullptr);
	int east = rim, west = rim + kSegments / 2;
	assert(k.locked[east] && !k.locked[west]);
	assert(k.radius[east] < 30 && std::fabs(k.radius[west] - 48 * k.reach[kSegments / 2]) < 1e-3f);
	for (int i = 0; i < kVertices; ++i) assert(k.z[i] == 0);
	Write(k, settings, points, 1.5f, v, uv, alpha);
	assert(alpha[east] == 0 && alpha[1] == 1);
	float stoppedU = uv[east].u;
	for (int n = 0; n < 100; ++n) Step(k, settings, points, .01f, 8, Kerb, nullptr);
	Write(k, settings, points, 1.5f, v, uv, alpha);
	assert(uv[east].u == stoppedU && stoppedU < 1);
	State d;
	d.seed = 3;
	for (int n = 0; n < 100; ++n) assert(Step(d, settings, points, .01f, 0, Kerb, nullptr) == 0);
	for (int i = 1; i < kVertices; ++i) assert(!d.locked[i] && !d.probed[i]);
	assert(Step(d, settings, points, .01f, 3, Kerb, nullptr) == 3);
	State e;
	e.seed = 3;
	for (int n = 0; n < 20; ++n) Step(e, settings, points, .01f, 8, Flat, nullptr);
	for (int n = 0; n < 400; ++n) assert(Step(e, settings, points, .01f, 8, Starved, nullptr) == 0);
	assert(std::fabs(e.progress - 1) < 1e-5f && !Settled(e));
	for (int i = 1; i < kVertices; ++i) {
		assert(!e.locked[i]);
		assert(e.radius[i] <= e.probedAt[i] + settings.reprobe + 1e-4f);
	}
	for (int n = 0; n < 400 && !Settled(e); ++n) Step(e, settings, points, .01f, 8, Flat, nullptr);
	assert(Settled(e));
	for (int i = 1; i < kVertices; ++i) assert(std::fabs(e.radius[i] - e.probedAt[i]) < 1e-3f && std::fabs(e.radius[i] - points[i].depth * 48 * e.reach[(i - 1) % kSegments]) < 1e-3f);
	Settings slow = settings;
	slow.duration = 10;
	State f;
	f.seed = 3;
	Step(f, slow, points, .01f, 0, Flat, nullptr);
	for (int i = 1; i < kVertices; ++i) {
		f.probed[i] = true;
		f.probedAt[i] = .2f;
	}
	for (int n = 0; n < 300; ++n) Step(f, slow, points, .01f, 48, Flat, nullptr);
	assert(f.progress < 1);
	for (int i = 1 + 2 * kSegments; i < kVertices; ++i) assert(f.radius[i] > 4.3f);

	State o;
	o.seed = 3;
	for (int n = 0; n < 400; ++n) Step(o, settings, points, .01f, 8, Starved, nullptr);
	for (int n = 0; n < 20000 && !Settled(o); ++n) {
		Step(o, settings, points, .01f, 1, Flat, nullptr);
		for (int i = 1; i < kVertices; ++i)
			if (points[i].parent > 0) assert(o.radius[points[i].parent] <= o.radius[i] + 1e-4f);
	}
	assert(Settled(o));

	State q;
	q.seed = 3;
	for (int n = 0; n < 5; ++n) Step(q, settings, points, .01f, 8, Flat, nullptr);
	int rim0 = 1 + 2 * kSegments, middle0 = 1 + kSegments, inner0 = 1;
	assert(q.radius[middle0] < 8 && q.radius[inner0] < 8);
	q.locked[rim0] = true;
	q.radius[rim0] = 8;
	for (int n = 0; n < 2000 && !Settled(q); ++n) {
		Step(q, settings, points, .01f, 8, Flat, nullptr);
		for (int i = 1; i < kVertices; ++i)
			if (points[i].parent > 0) assert(q.radius[points[i].parent] <= q.radius[i] + 1e-4f);
	}
	assert(Settled(q) && !q.locked[middle0] && !q.locked[inner0]);
	assert(q.radius[middle0] <= 8 && q.radius[inner0] <= 8);
	std::puts("RigTest OK");
	return 0;
}
