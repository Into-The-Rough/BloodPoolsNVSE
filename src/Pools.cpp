#include "Pools.h"
#include "Kills.h"
#include "Body.h"
#include "Decals.h"
#include "PoolMesh.h"
#include "Config.h"
#include <cmath>
#include <cstring>

namespace Pools::Corpses {
	using namespace Engine;
	constexpr uint32_t kPoolTexture = 0x00064EC8, kSplatterTexture = 0x000312FD;
	constexpr int kCapacity = 32;
	constexpr int kTorso = 0;

	constexpr int kMaxSources = Wounds::kMaxPerBody * 2;

	struct Source {
		Object* node;
		float scale;
	};

	struct Placement {
		Vec3 at;
		float radius;
	};

	struct Corpse {
		uint32_t id = 0;
		int part = -1;
		bool explosion = false;
		Vec3 impact;
		Wounds::Wound wounds[Wounds::kMaxPerBody];
		int woundCount = 0;
		Vec3 last[kMaxSources];
		int tracked = 0;
		float age = 0, still = 0;
		Placement placements[kMaxSources];
		int placementCount = 0, placed = 0, pools = 0;
		bool placing = false;
	};

	static Corpse s_corpses[kCapacity];
	static int s_count = 0;
	static Form* s_pool = nullptr;
	static Form* s_splatter = nullptr;
	static uint32_t s_seed = 1;

	static float Roll()
	{
		s_seed ^= s_seed << 13;
		s_seed ^= s_seed >> 17;
		s_seed ^= s_seed << 5;
		return float(s_seed >> 8) * (1.f / 16777216.f);
	}

	bool Init()
	{
		Mesh::Init();
		s_pool = Lookup(kPoolTexture);
		s_splatter = Lookup(kSplatterTexture);
		return s_pool && s_pool->type == kTextureSetForm && s_splatter && s_splatter->type == kTextureSetForm;
	}

	static float Distance(const Vec3& a, const Vec3& b)
	{
		float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	static void Remove(int i) { s_corpses[i] = s_corpses[--s_count]; }

	static void Admit(const Kills::Kill& kill)
	{
		if (s_count >= kCapacity) return;
		Corpse& c = s_corpses[s_count++];
		c = {};
		c.id = kill.target;
		c.part = kill.location;
		c.explosion = kill.explosion;
		c.impact = kill.impact;
		c.woundCount = kill.woundCount;
		for (int i = 0; i < kill.woundCount; ++i) c.wounds[i] = kill.wounds[i];
	}

	static Object* Anchor(Form* actor, int part, Object* root)
	{
		Object* node = Body::Stump(actor, root, part);
		if (!node) node = PartNode(actor, kTorso);
		return node ? node : root;
	}

	//dropped weapons distort the body span
	constexpr int kWeaponPart = 14;

	static float Span(Form* actor, Object* root)
	{
		Object* torso = PartNode(actor, kTorso);
		if (!torso) torso = root;
		float farthest = 0;
		for (int k = 1; k < kWeaponPart; ++k) {
			Object* node = Body::Stump(actor, root, k);
			if (!node) continue;
			float d = Distance(node->world.pos, torso->world.pos);
			if (d > farthest) farthest = d;
		}
		return farthest > 1 ? farthest * 2 : settings.defaultSpan;
	}

	static Mesh::Fluid FluidOf(Form* actor)
	{
		if (IsRobot(actor)) return Mesh::Fluid::kOil;
		Form* base = BaseForm(actor);
		const char* id = base ? EditorID(base) : nullptr;
		if (id && std::strstr(id, "Glowing")) return Mesh::Fluid::kGlow;
		return Mesh::Fluid::kBlood;
	}

	static float BodySize(Form* actor, Object* root)
	{
		float size = Span(actor, root) * settings.poolScale;
		if (size < settings.poolMin) size = settings.poolMin;
		if (size > settings.poolMax) size = settings.poolMax;
		return size;
	}

	static Decals::Cast Splatter(Form* actor, const Vec3& pos, float size)
	{
		Decals::Request request;
		request.corpse = actor;
		request.pos = pos;
		request.width = size * (.85f + .3f * Roll());
		request.height = size * (.85f + .3f * Roll());
		request.textureSet = s_splatter;
		request.colour = settings.colour;
		request.lifetime = settings.lifetime;
		request.yaw = Roll() * 6.2831853f;
		request.quadrant = int(Roll() * 4) & 3;
		return Decals::Place(request);
	}

	static Decals::Cast Spread(Form* actor, const Vec3& pos, float size, uint32_t salt)
	{
		if (!settings.pools) return Decals::Cast::kMiss;
		Decals::Surface surface;
		Mesh::Fluid fluid = FluidOf(actor);
		Decals::Cast ground = Decals::Ground(ActorFilter(actor), pos, 240, surface);
		if (ground != Decals::Cast::kHit) return ground;
		return Mesh::Spawn(surface.pos, size * .5f, ActorFilter(actor), s_seed ^ actor->id ^ (salt * 0x9E3779B9u), fluid);
	}

	constexpr int kPoolCasts = Decals::kGroundCasts + Mesh::kHeightCasts;
	constexpr int kFirstPoolCasts = kPoolCasts + Decals::kGroundCasts;

	static bool PoolOrSplatter(Form* actor, const Vec3& at, float size, uint32_t salt, bool fallback)
	{
		if (Decals::CastsLeft() < (fallback ? kFirstPoolCasts : kPoolCasts)) return false;
		Decals::Cast spread = Spread(actor, at, size, salt);
		if (spread == Decals::Cast::kDeferred) return false;
		if (spread == Decals::Cast::kMiss && fallback) return Splatter(actor, at, size) != Decals::Cast::kDeferred;
		return true;
	}

	static int Sources(Form* actor, Object* root, const Corpse& c, Source* out)
	{
		float total = 0;
		for (int i = 0; i < c.woundCount; ++i) total += std::fmax(c.wounds[i].damage, 0.f);
		int n = 0;
		for (int i = 0; i < c.woundCount; ++i) {
			const Wounds::Wound& w = c.wounds[i];
			if (w.part == kWeaponPart) continue;
			float share = total > 0 ? std::fmax(w.damage, 0.f) / total : 0;
			float scale = i ? std::fmin(settings.woundScale * (.5f + share), 1.f) : 1;
			Object* severed = Body::Severed(actor, root, w.part);
			if (severed || (w.flags & (kHitDismember | kHitExplode))) scale *= settings.stumpScale;
			Object* stump = Body::Stump(actor, root, w.part);
			if (stump) out[n++] = {stump, scale};
			if (severed) out[n++] = {severed, settings.severedScale};
		}
		if (!n) out[n++] = {Anchor(actor, c.part, root), 1};
		return n;
	}

	constexpr float kMergeOverlap = .5f;

	static void Settle(Form* actor, Object* root, Corpse& c)
	{
		Source sources[kMaxSources];
		int n = Sources(actor, root, c, sources);
		float size = BodySize(actor, root);
		for (int i = 0; i < n; ++i) c.placements[i] = {sources[i].node->world.pos, size * sources[i].scale * .5f};
		c.placementCount = n;
		c.placing = true;
	}

	static bool PlaceNext(Form* actor, Corpse& c)
	{
		for (; c.placed < c.placementCount && c.pools < settings.maxBodyPools; ++c.placed) {
			const Placement& p = c.placements[c.placed];
			bool merged = false;
			for (int k = 0; k < c.placed && !merged; ++k) {
				const Placement& q = c.placements[k];
				merged = q.radius > 0 && std::hypot(p.at.x - q.at.x, p.at.y - q.at.y) < (p.radius + q.radius) * kMergeOverlap;
			}
			if (merged) {
				c.placements[c.placed].radius = 0;
				continue;
			}
			if (!PoolOrSplatter(actor, p.at, p.radius * 2, c.placed, !c.placed)) return false;
			++c.pools;
		}
		return true;
	}

	void Update()
	{
		float dt = FrameSeconds();
		if (!(dt > 0)) return;
		Decals::BeginFrame(settings.raysPerFrame > kFirstPoolCasts ? settings.raysPerFrame : kFirstPoolCasts);
		Kills::Kill kills[kCapacity];
		int n = Kills::Drain(kills, kCapacity);
		for (int i = 0; i < n; ++i)
			if (settings.enabled) Admit(kills[i]);
		Form* player = Player();
		for (int i = 0; i < s_count;) {
			Corpse& c = s_corpses[i];
			Form* actor = Lookup(c.id);
			Object* root = IsActor(actor) ? RefRoot(actor) : nullptr;
			//robots can have the no-blood flag
			if (!root || (NoBloodDecal(actor) && !IsRobot(actor)) || (player && Distance(RefPosition(actor), RefPosition(player)) > settings.maxDistance)) {
				Remove(i);
				continue;
			}
			c.age += dt;
			if (c.placing) {
				if (PlaceNext(actor, c)) Remove(i);
				else ++i;
				continue;
			}
			//blast decals use the same budget
			if (c.explosion) {
				if (c.age < settings.explosionDelay) {
					++i;
					continue;
				}
				if (settings.explosions) {
					float size = BodySize(actor, root) * settings.explosionScale;
					if (Decals::CastsLeft() < Decals::kGroundCasts || Splatter(actor, RefPosition(actor), size) == Decals::Cast::kDeferred) {
						++i;
						continue;
					}
				}
				Remove(i);
				continue;
			}
			Source sources[kMaxSources];
			int tracked = Sources(actor, root, c, sources);
			float speed = 0;
			if (tracked == c.tracked)
				for (int k = 0; k < tracked; ++k) speed = std::fmax(speed, Distance(sources[k].node->world.pos, c.last[k]) / dt);
			c.still = tracked == c.tracked && speed < settings.settleSpeed ? c.still + dt : 0;
			for (int k = 0; k < tracked; ++k) c.last[k] = sources[k].node->world.pos;
			c.tracked = tracked;
			bool settled = c.age >= settings.minWait && c.still >= settings.settleSeconds;
			if ((settled && TimeMultiplier() > .8f) || c.age >= settings.maxWait) {
				Settle(actor, root, c);
				if (PlaceNext(actor, c)) {
					Remove(i);
					continue;
				}
			}
			++i;
		}
		Mesh::Update(dt);
		Decals::EndFrame();
	}

	void Clear()
	{
		s_count = 0;
		Kills::Clear();
		Decals::EndFrame();
		Mesh::Clear();
	}
}
