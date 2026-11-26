#include "PoolMesh.h"
#include "Rig.h"
#include "Decals.h"
#include "Config.h"
#include <cmath>

namespace Pools::Mesh {
	using namespace Engine;
	constexpr int kCapacity = 16;
	constexpr int kBloodVariants = 3;
	constexpr const char* kBloodMeshes[kBloodVariants] = {"BloodPools\\blood01.nif", "BloodPools\\blood02.nif", "BloodPools\\blood03.nif"};
	constexpr float kProbeReach = 120;

	struct Pool {
		uint32_t cell = 0;
		uint32_t filter = 0;
		Object* root = nullptr;
		Object* disc = nullptr;
		Rig::State rig;
		Rig::Settings settings;
		Vec3 origin, tint{1, 1, 1};
		float age = 0;
		GeometryData* data = nullptr;
		bool done = false, dead = false;
	};

	static Pool s_pools[kCapacity];
	static int s_count = 0;
	static Rig::Point s_points[Rig::kVertices];
	static uint32_t s_turn = 0;

	bool Contains(const Object* object)
	{
		for (int i = 0; i < s_count; ++i)
			if (s_pools[i].disc == object) return true;
		return false;
	}

	void Init() { Rig::Layout(s_points); }

	static void Remove(int i)
	{
		Pool p = s_pools[i];
		s_pools[i] = s_pools[--s_count];
		if (p.root->parent) Detach(p.root->parent, p.root);
		Release(p.data);
		Release(p.disc);
		Release(p.root);
	}

	static Object* Disc(Object* root)
	{
		Object* node = AsNode(root);
		if (!node) return root;
		int n = ChildCount(node);
		for (int i = 0; i < n; ++i) {
			Object* child = ChildAt(node, i);
			if (!child) continue;
			if (!AsNode(child)) return child;
			if (Object* deeper = Disc(child)) return deeper;
		}
		return nullptr;
	}

	static GeometryData* Bind(Object* disc, const Vec3& tint)
	{
		constexpr uint32_t n = Rig::kVertices;
		auto vertices = static_cast<Vec3*>(NiNew(sizeof(Vec3) * n));
		auto normals = static_cast<Vec3*>(NiNew(sizeof(Vec3) * n * 3));
		auto colours = static_cast<float*>(NiNew(sizeof(float) * 4 * n));
		auto uvs = static_cast<float*>(NiNew(sizeof(float) * 2 * n));
		auto triangles = static_cast<uint16_t*>(NiNew(sizeof(uint16_t) * Rig::kTriangles * 3));
		GeometryData* data = nullptr;
		if (vertices && normals && colours && uvs && triangles) {
			for (uint32_t i = 0; i < n; ++i) {
				vertices[i] = {};
				normals[i] = {0, 0, 1};
				normals[n + i] = {0, 1, 0};
				normals[2 * n + i] = {1, 0, 0};
				colours[4 * i] = tint.x;
				colours[4 * i + 1] = tint.y;
				colours[4 * i + 2] = tint.z;
				colours[4 * i + 3] = 1;
				uvs[2 * i] = uvs[2 * i + 1] = .5f;
			}
			Rig::Triangles(triangles);
			data = NewTriShapeData(n, vertices, normals, colours, uvs, kDataUVSet, kDataNBT, Rig::kTriangles, triangles);
		}
		if (!data) {
			NiDelete(vertices, sizeof(Vec3) * n);
			NiDelete(normals, sizeof(Vec3) * n * 3);
			NiDelete(colours, sizeof(float) * 4 * n);
			NiDelete(uvs, sizeof(float) * 2 * n);
			NiDelete(triangles, sizeof(uint16_t) * Rig::kTriangles * 3);
			return nullptr;
		}
		data->keepFlags = kKeepAll;
		SetConsistency(data, kConsistencyMutable);
		SetModelData(disc, data);
		UpdateWorldBound(disc);
		AddRef(data);
		return data;
	}

	struct ProbeContext {
		const Pool* pool;
		const Transform* frame;
	};

	constexpr float kDrawnAgreement = 28;

	static Decals::Cast Height(uint32_t filter, const Vec3& from, float& z)
	{
		if (Decals::CastsLeft() < kHeightCasts) return Decals::Cast::kDeferred;
		Decals::Surface surface;
		Decals::Cast ground = Decals::Ground(filter, from, kProbeReach, surface);
		if (ground != Decals::Cast::kHit) return ground;
		z = surface.pos.z;
		if (!settings.visualProbe) return ground;
		Vec3 drawn;
		Decals::Cast visual = Decals::Visual(from, kProbeReach, drawn);
		if (visual == Decals::Cast::kDeferred) return visual;
		if (visual == Decals::Cast::kHit && std::fabs(drawn.z - z) <= kDrawnAgreement) z = drawn.z;
		return Decals::Cast::kHit;
	}

	static Rig::Reading Probe(void* context, float x, float y, float& z)
	{
		auto c = static_cast<ProbeContext*>(context);
		auto pool = c->pool;
		const auto& r = c->frame->rot;
		Vec3 from{pool->origin.x + r[0][0] * x + r[0][1] * y, pool->origin.y + r[1][0] * x + r[1][1] * y, pool->origin.z};
		float height;
		Decals::Cast cast = Height(pool->filter, from, height);
		if (cast == Decals::Cast::kDeferred) return Rig::Reading::kDeferred;
		if (cast == Decals::Cast::kMiss) return Rig::Reading::kNone;
		z = height - pool->origin.z;
		return Rig::Reading::kGround;
	}

	Decals::Cast Spawn(const Vec3& ground, float radius, uint32_t filter, uint32_t seed, Fluid fluid)
	{
		if (!settings.pools) return Decals::Cast::kMiss;
		const char* path = fluid == Fluid::kOil ? "BloodPools\\oil01.nif" : fluid == Fluid::kGlow ? "BloodPools\\glow01.nif"
																								  : kBloodMeshes[seed % kBloodVariants];
		auto cell = static_cast<Form*>(CellAt(ground));
		if (!cell) return Decals::Cast::kMiss;
		if (!CellAttached(cell)) return Decals::Cast::kDeferred;
		Object* parent = CellNode(cell);
		if (!parent) return Decals::Cast::kDeferred;
		Vec3 origin = ground;
		float height;
		Decals::Cast measured = Height(filter, ground, height);
		if (measured == Decals::Cast::kDeferred) return measured;
		if (measured == Decals::Cast::kHit) origin.z = height;
		int limit = settings.maxPools < 1 ? 1 : settings.maxPools > kCapacity ? kCapacity
																			  : settings.maxPools;
		if (s_count && s_count >= limit) {
			int oldest = 0;
			for (int i = 1; i < s_count; ++i)
				if (s_pools[i].age > s_pools[oldest].age) oldest = i;
			Remove(oldest);
		}
		Object* root = CloneModel(path);
		if (!root) return Decals::Cast::kMiss;
		Object* disc = Disc(root);
		if (!disc || !ModelData(disc)) {
			Release(root);
			return Decals::Cast::kMiss;
		}
		Pool p;
		p.cell = cell->id;
		p.root = root;
		p.disc = disc;
		p.filter = filter;
		p.origin = origin;
		p.rig.seed = seed;
		auto jitter = [seed](uint32_t salt, float amount) {
			return 1 + amount * (2 * Rig::Random(seed, salt) - 1);
		};
		p.settings.radius = radius * jitter(1, settings.sizeJitter);
		p.settings.duration = settings.poolDuration * jitter(2, settings.durationJitter);
		p.settings.lobes = settings.lobes;
		p.settings.rateJitter = settings.rateJitter;
		p.settings.creep = settings.creep * jitter(3, .5f);
		p.settings.creepSeconds = settings.creepSeconds * jitter(4, .5f);
		float shade = 1 - settings.tintJitter * Rig::Random(seed, 5);
		float hue = fluid == Fluid::kBlood ? 1 - settings.hueJitter * Rig::Random(seed, 6) : 1;
		p.tint = {shade, shade * hue, shade * hue};
		p.settings.maxStep = settings.maxStep;
		p.settings.slopeUp = settings.slopeUp;
		p.settings.slopeDown = settings.slopeDown;
		p.data = Bind(disc, p.tint);
		if (!p.data) {
			Release(root);
			return Decals::Cast::kMiss;
		}
		AddRef(disc);
		//bsfadenode fading writes shared shader state
		root->flags = (root->flags | kIgnoreFade) & ~kHidden;
		RotateZ(root, (seed % 628) * .01f);
		root->local.pos = origin;
		root->local.scale = 1;
		Attach(parent, root);
		UpdateProperties(root);
		UpdateObject(root);
		s_pools[s_count++] = p;
		return Decals::Cast::kHit;
	}

	static bool Tick(Pool& p, float dt)
	{
		Form* cell = Lookup(p.cell);
		if (!cell || !CellAttached(cell) || p.root->parent != CellNode(cell)) return false;
		Object* root = p.root;
		Object* disc = p.disc;
		GeometryData* data = p.data;
		p.age += dt;
		float lifetime = settings.lifetime, fade = settings.poolFade;
		if (p.age >= lifetime) return false;
		float opacity = fade > 0 && p.age > lifetime - fade ? (lifetime - p.age) / fade : 1;
		if (!p.done) {
			ProbeContext context{&p, &disc->world};
			Rig::Step(p.rig, p.settings, s_points, dt, settings.raysPerTick < 1 ? 1 : settings.raysPerTick, Probe, &context);
		}
		if (!p.done || opacity < 1) {
			Rig::Vertex vertices[Rig::kVertices];
			Rig::UV uvs[Rig::kVertices];
			float alpha[Rig::kVertices];
			Rig::Write(p.rig, p.settings, s_points, settings.lift, vertices, uvs, alpha);
			float radius = 0;
			for (int k = 0; k < Rig::kVertices; ++k) {
				data->vertices[k] = {vertices[k].x, vertices[k].y, vertices[k].z};
				data->uvs[2 * k] = uvs[k].u;
				data->uvs[2 * k + 1] = uvs[k].v;
				//materials are shared, vertex colours are private
				data->colours[4 * k + 3] = alpha[k] * opacity;
				float r = std::sqrt(vertices[k].x * vertices[k].x + vertices[k].y * vertices[k].y + vertices[k].z * vertices[k].z);
				if (r > radius) radius = r;
			}
			data->boundCenter = {};
			data->boundRadius = radius + 1;
			MarkAsChanged(data, kDirtyVertices | kDirtyColours | kDirtyUVs);
			UpdateWorldBound(disc);
			UpdateUpwardPass(root);
		}
		if (!p.done && Rig::Settled(p.rig)) p.done = true;
		return true;
	}

	void Update(float dt)
	{
		int n = s_count;
		for (int k = 0; k < n; ++k)
			if (!Tick(s_pools[(s_turn + k) % n], dt)) s_pools[(s_turn + k) % n].dead = true;
		++s_turn;
		for (int i = 0; i < s_count;)
			if (s_pools[i].dead) Remove(i);
			else ++i;
	}

	void Clear()
	{
		while (s_count) Remove(s_count - 1);
		s_turn = 0;
	}
}
