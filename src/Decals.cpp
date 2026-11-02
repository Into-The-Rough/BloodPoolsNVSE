#include "Decals.h"
#include "PoolMesh.h"
#include <cmath>

namespace Pools::Decals {
	using namespace Engine;
	constexpr float kAbove = 24, kReach = 240;
	static int s_castsLeft = 0;
	static PickStats s_pickStats;

	PickStats TakePickStats()
	{
		PickStats stats = s_pickStats;
		s_pickStats = {};
		return stats;
	}

	void BeginFrame(int casts) { s_castsLeft = casts; }

	int CastsLeft() { return s_castsLeft; }

	static bool Charge()
	{
		if (s_castsLeft <= 0) return false;
		--s_castsLeft;
		return true;
	}

	static bool HasGeometry(Object* o, int depth = 0)
	{
		if (!o) return false;
		Object* node = AsNode(o);
		if (!node) return true;
		if (depth > 8) return false;
		int n = ChildCount(node);
		for (int i = 0; i < n; ++i)
			if (HasGeometry(ChildAt(node, i), depth + 1)) return true;
		return false;
	}

	//collision nodes can sit below the drawn geometry
	static Object* Drawn(Object* node, Object* root)
	{
		while (node && !HasGeometry(node) && node != root && node->parent)
			node = node->parent;
		return node;
	}

	//0x88e8d0 impact splatter ray
	Cast Ground(uint32_t filter, const Vec3& from, float reach, Surface& out)
	{
		if (!PickWorldReady()) return Cast::kMiss;
		Vec3 start = from;
		start.z += kAbove;
		Vec3 end = start;
		end.z -= reach;
		for (int attempt = 0; attempt < kGroundCasts; ++attempt) {
			if (!Charge()) return Cast::kDeferred;
			PickData pick;
			PickInit(pick);
			PickFilter(pick) = filter;
			for (int k = 0; k < 8; ++k) PickShapeKeys(pick)[k] = 0xFFFFFFFF;
			PickFrom(pick, start);
			PickTo(pick, end);
			++s_pickStats.ground;
			CastPick(pick);
			//0xc69748 sets this when no cast ran
			if (PickSkipped(pick)) return Cast::kDeferred;
			auto collidable = PickCollidable(pick);
			if (!collidable) return Cast::kMiss;
			float fraction = PickFraction(pick);
			Vec3 hit{start.x + (end.x - start.x) * fraction, start.y + (end.y - start.y) * fraction, start.z + (end.z - start.z) * fraction};
			bool land = (CollidableFilter(collidable) >> 16) == 1;
			Object* node = land ? LandNodeAt(hit) : CollidableNode(collidable);
			if (land && node && node->parent) node = node->parent;
			Form* ref = node ? FindReferenceFor3D(node) : nullptr;
			if (node && (land || (ref && !IsActor(ref)))) {
				if (!land) node = Drawn(node, RefRoot(ref));
				out.pos = hit;
				out.normal = PickNormal(pick);
				out.node = node;
				return Cast::kHit;
			}
			start = hit;
			start.z -= 2;
			if (start.z <= end.z) return Cast::kMiss;
		}
		return Cast::kMiss;
	}

	//pick records retain hit objects
	static ScenePick s_pick;
	static bool s_pickBuilt = false;

	void EndFrame()
	{
		if (!s_pickBuilt) return;
		ScenePickFree(s_pick);
		s_pickBuilt = false;
	}

	Cast Visual(const Vec3& from, float reach, Vec3& hit)
	{
		if (!PickWorldReady()) return Cast::kMiss;
		if (!Charge()) return Cast::kDeferred;
		Vec3 origin{from.x, from.y, from.z + kAbove};
		Vec3 down{0, 0, -1};
		if (!s_pickBuilt) {
			ScenePickInit(s_pick);
			s_pickBuilt = true;
		}
		ScenePickTarget(s_pick);
		bool found = false;
		++s_pickStats.visual;
		int n = ScenePickCast(s_pick, origin, down) ? ScenePickCount(s_pick) : 0;
		for (int i = 0; i < n && !found; ++i) {
			const uint8_t* r = ScenePickRecord(s_pick, i);
			if (!r || RecordDistance(r) > reach + kAbove) continue;
			Object* object = RecordObject(r);
			if (Mesh::Contains(object)) continue;
			Form* ref = object ? FindReferenceFor3D(object) : nullptr;
			if (ref && (IsActor(ref) || IsLooseItem(ref))) continue;
			hit = RecordPoint(r);
			found = true;
		}
		return found ? Cast::kHit : Cast::kMiss;
	}

	Cast Place(const Request& request)
	{
		Surface surface;
		Cast ground = Ground(ActorFilter(request.corpse), request.pos, kReach, surface);
		if (ground != Cast::kHit) return ground;
		float length = std::sqrt(surface.normal.x * surface.normal.x + surface.normal.y * surface.normal.y + surface.normal.z * surface.normal.z);
		if (length < .5f) surface.normal = {0, 0, 1};
		DecalData data;
		DecalInit(data);
		data.pos = surface.pos;
		data.normal = surface.normal;
		data.node = surface.node;
		data.textureSet = request.textureSet;
		data.width = request.width;
		data.height = request.height;
		//0x4a2d50 blood splatter defaults
		data.depth = 48;
		data.yaw = request.yaw;
		data.shininess = 300;
		data.angle = 80;
		data.spacing = 0;
		data.quadrant = static_cast<uint8_t>(request.quadrant);
		data.occlusion = 0;
		data.alphaBlend = 0;
		data.alphaTest = 1;
		data.colour = request.colour;
		void* cell = CellAt(surface.pos);
		if (!cell) return Cast::kMiss;
		float saved = DecalLifetime();
		DecalLifetime() = request.lifetime;
		CellAddDecal(cell, data, kGeometryDecal, true);
		DecalLifetime() = saved;
		return Cast::kHit;
	}
}
