#pragma once
#include <cstdint>
#include <cstddef>
#include <intrin.h>

namespace Pools::Engine {
	struct Vec3 {
		float x = 0, y = 0, z = 0;
	};

	struct Transform {
		float rot[3][3];
		Vec3 pos;
		float scale;
	};

	struct Form {
		void** vtable;
		uint8_t type;
		uint8_t pad05[3];
		uint32_t flags;
		uint32_t id;
	};

	constexpr uint8_t kTextureSetForm = 0x04, kCreatureBase = 0x2B, kCharacter = 0x3B, kCreature = 0x3C;
	constexpr uint32_t kPlayerRefID = 0x14;

	struct Object {
		void** vtable;
		uint32_t references;
		const char* name;
		uint8_t pad0C[0x0C];
		Object* parent;
		void* collision;
		void* worldBound;
		uint8_t properties[0x0C];
		uint32_t flags;
		Transform local, world;
	};

	static_assert(offsetof(Object, parent) == 0x18 && offsetof(Object, worldBound) == 0x20 && offsetof(Object, flags) == 0x30 && offsetof(Object, world) == 0x68 && offsetof(Object, world.pos) == 0x8C && sizeof(Object) == 0x9C);

	//dirty and consistency bits share +0x0e
	struct GeometryData {
		void** vtable;
		uint32_t references;
		uint16_t numVertices, id, dataFlags, dirtyFlags;
		Vec3 boundCenter;
		float boundRadius;
		Vec3* vertices;
		Vec3* normals;
		float* colours;
		float* uvs;
		void* additional;
		void* buffer;
		uint8_t keepFlags, compressFlags, pad3A[2];
		uint8_t canSave, pad3D[3];
	};

	static_assert(offsetof(GeometryData, numVertices) == 0x08 && offsetof(GeometryData, dirtyFlags) == 0x0E && offsetof(GeometryData, vertices) == 0x20 && offsetof(GeometryData, uvs) == 0x2C && offsetof(GeometryData, keepFlags) == 0x38 && sizeof(GeometryData) == 0x40);
	constexpr uint16_t kDataUVSet = 1, kDataNBT = 0x1000, kDirtyVertices = 1, kDirtyNormals = 2, kDirtyColours = 4, kDirtyUVs = 8, kConsistencyMutable = 0;
	constexpr uint8_t kKeepAll = 0x3F;

	inline GeometryData* ModelData(Object* geometry) { return *reinterpret_cast<GeometryData**>(reinterpret_cast<uint8_t*>(geometry) + 0xB8); }

	inline void SetModelData(Object* geometry, GeometryData* data) { reinterpret_cast<void(__thiscall*)(Object*, GeometryData*)>(geometry->vtable[0xE4 / 4])(geometry, data); }

	inline void UpdateWorldBound(Object* o) { reinterpret_cast<void(__thiscall*)(Object*)>(o->vtable[0xBC / 4])(o); }

	inline void UpdateUpwardPass(Object* node) { reinterpret_cast<void(__thiscall*)(Object*)>(node->vtable[0xFC / 4])(node); }

	inline void* NiNew(size_t bytes) { return reinterpret_cast<void*(__cdecl*)(size_t)>(0xAA13E0)(bytes); }

	inline void NiDelete(void* memory, size_t bytes) { reinterpret_cast<void(__cdecl*)(void*, size_t)>(0xAA1460)(memory, bytes); }

	//ctor owns all arrays, colours are rgba floats
	inline GeometryData* NewTriShapeData(uint32_t numVertices, Vec3* vertices, Vec3* normals, float* colours, float* uvs, uint16_t uvSets, uint16_t nbt, uint32_t numTriangles, uint16_t* triangles)
	{
		void* memory = NiNew(0x58);
		if (!memory) return nullptr;
		return reinterpret_cast<GeometryData*(__thiscall*)(void*, uint32_t, Vec3*, Vec3*, float*, float*, uint16_t, uint16_t, uint32_t, uint16_t*)>(0xA7B630)(memory, numVertices, vertices, normals, colours, uvs, uvSets, nbt, numTriangles, triangles);
	}

	template<class T>
	inline void AddRef(T* object) { _InterlockedIncrement(reinterpret_cast<volatile long*>(&object->references)); }

	//zero refs delete through vtable slot 1
	inline void Release(void* object) { reinterpret_cast<void(__thiscall*)(void*)>(0x401970)(object); }

	inline void MarkAsChanged(GeometryData* data, uint16_t flags) { reinterpret_cast<void(__thiscall*)(GeometryData*, uint16_t)>(0xA67090)(data, flags); }

	inline void SetConsistency(GeometryData* data, uint16_t flags) { reinterpret_cast<void(__thiscall*)(GeometryData*, uint16_t)>(0xA67050)(data, flags); }

	struct CloneProcess {
		void* copies;
		void* processed;
		uint32_t copyType;
		uint8_t appendChar, pad0D[3];
		Vec3 scale;
	};

	static_assert(offsetof(CloneProcess, scale) == 0x10 && sizeof(CloneProcess) == 0x1C);

	inline Object* CloneModel(const char* path)
	{
		void* loader = *reinterpret_cast<void**>(0x11C3B3C);
		Object* source = reinterpret_cast<Object*(__thiscall*)(void*, const char*, uint32_t, bool, uint32_t, bool, bool)>(0x447080)(loader, path, 0, true, 0, false, false);
		Object* copy = nullptr;
		if (source) {
			CloneProcess process;
			reinterpret_cast<void(__thiscall*)(CloneProcess*, float)>(0x4AD050)(&process, 1.f);
			copy = reinterpret_cast<Object*(__thiscall*)(Object*, CloneProcess*)>(0xA5D2C0)(source, &process);
			if (copy) AddRef(copy);
			reinterpret_cast<void(__thiscall*)(CloneProcess*)>(0x4AD270)(&process);
		}
		//balance the model cache count
		reinterpret_cast<void(__thiscall*)(void*, const char*, bool, int16_t)>(0x445300)(loader, path, false, 1);
		return copy;
	}

	constexpr uint32_t kCellAttached = 6, kCellStaticNode = 3;
	constexpr uint32_t kHidden = 1, kIgnoreFade = 0x8000;

	inline bool CellAttached(void* cell) { return reinterpret_cast<uint32_t(__thiscall*)(void*)>(0x450FD0)(cell) == kCellAttached; }

	inline Object* CellNode(void* cell) { return reinterpret_cast<Object*(__thiscall*)(void*, uint32_t)>(0x456FC0)(cell, kCellStaticNode); }

	inline void Attach(Object* parent, Object* child) { reinterpret_cast<void(__thiscall*)(Object*, Object*, bool)>(parent->vtable[0xDC / 4])(parent, child, true); }

	inline void Detach(Object* parent, Object* child) { reinterpret_cast<void(__thiscall*)(Object*, Object*)>(parent->vtable[0xE8 / 4])(parent, child); }

	inline void RotateZ(Object* root, float yaw) { reinterpret_cast<void(__thiscall*)(void*, float)>(0x4A0C90)(root->local.rot, yaw); }

	inline void UpdateProperties(Object* root) { reinterpret_cast<void(__thiscall*)(Object*)>(0xA5A040)(root); }

	struct UpdateData {
		float seconds;
		bool controllers, threaded;
		uint8_t unk06;
		bool geomorphs, shadows;
		uint8_t pad09[3];
	};

	static_assert(offsetof(UpdateData, controllers) == 0x04 && offsetof(UpdateData, shadows) == 0x08 && sizeof(UpdateData) == 0x0C);

	inline void UpdateObject(Object* root)
	{
		UpdateData data{};
		reinterpret_cast<void(__thiscall*)(Object*, UpdateData*)>(0xA59C60)(root, &data);
	}

	inline Object* AsNode(Object* o) { return reinterpret_cast<Object*(__thiscall*)(Object*)>(o->vtable[3])(o); }

	inline int ChildCount(Object* node) { return reinterpret_cast<int(__thiscall*)(Object*)>(0x43B480)(node); }

	inline Object* ChildAt(Object* node, int i) { return reinterpret_cast<Object*(__thiscall*)(Object*, int)>(0x43B4A0)(node, i); }

	//0x9b4d90 builds hitdata
	struct HitData {
		Form* source;
		Form* target;
		Form* projectile;
		uint32_t weaponAV;
		int32_t location;
		float healthDamage;
		uint8_t pad18[0x18];
		Form* weapon;
		float pad34;
		Vec3 impact, impactDirection;
		uint8_t pad50[8];
		uint32_t flags;
		uint8_t pad5C[8];
	};

	static_assert(offsetof(HitData, location) == 0x10 && offsetof(HitData, healthDamage) == 0x14 && offsetof(HitData, impact) == 0x38 && offsetof(HitData, flags) == 0x58 && sizeof(HitData) == 0x64);
	constexpr uint32_t kHitExplosion = 0x2000;
	//0x8b43a0 dismemberment flags
	constexpr uint32_t kHitDismember = 0x20, kHitExplode = 0x40;

	//yaw in radians, angle in degrees, quadrant >= 4 uses the whole texture
	struct DecalData {
		Vec3 pos, normal, up;
		Form* ref;
		Object* node;
		uint32_t unk44;
		Form* textureSet;
		int32_t unk52;
		float width, height, depth, yaw;
		void* cell;
		float parallaxScale, unk80, shininess, angle, spacing;
		Vec3 colour;
		uint32_t unk108;
		uint8_t quadrant, mirror, occlusion, parallax, alphaBlend, alphaTest, parallaxPasses, force, unk120, unk121;
		uint8_t pad7A[6];
	};

	static_assert(offsetof(DecalData, node) == 40 && offsetof(DecalData, textureSet) == 48 && offsetof(DecalData, width) == 56 && offsetof(DecalData, cell) == 72 && offsetof(DecalData, spacing) == 92
		&& offsetof(DecalData, colour) == 96 && offsetof(DecalData, occlusion) == 114 && offsetof(DecalData, force) == 119 && sizeof(DecalData) == 128);
	constexpr int kGeometryDecal = 1;

	//pickdata is copied with movaps
	struct alignas(16) PickData {
		uint8_t bytes[0xB0];
	};

	inline uint32_t& PickFilter(PickData& p) { return *reinterpret_cast<uint32_t*>(p.bytes + 0x24); }

	inline const Vec3& PickNormal(const PickData& p) { return *reinterpret_cast<const Vec3*>(p.bytes + 0x30); }

	inline float PickFraction(const PickData& p) { return *reinterpret_cast<const float*>(p.bytes + 0x40); }

	inline uint32_t* PickShapeKeys(PickData& p) { return reinterpret_cast<uint32_t*>(p.bytes + 0x50); }

	inline void* PickCollidable(const PickData& p) { return *reinterpret_cast<void* const*>(p.bytes + 0x80); }

	inline bool PickSkipped(const PickData& p) { return p.bytes[0xAC] != 0; }

	//filter hiword 1 is landscape
	inline uint32_t CollidableFilter(const void* collidable) { return *reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(collidable) + 0x1C); }

	constexpr uint32_t kLayerLOS = 37;

	inline void* TES() { return *reinterpret_cast<void**>(0x11DEA10); }

	inline Form* Player() { return *reinterpret_cast<Form**>(0x11DEA3C); }

	//scaled by the time multiplier
	inline float FrameSeconds() { return *reinterpret_cast<float*>(0x11F63A0); }

	inline float TimeMultiplier() { return *reinterpret_cast<float*>(0x11AC3A0); }

	//lifetime is read when the decal is created
	inline float& DecalLifetime() { return *reinterpret_cast<float*>(0x11C5878); }

	inline Form* Lookup(uint32_t id) { return reinterpret_cast<Form*(__cdecl*)(uint32_t)>(0x4839C0)(id); }

	inline Form* FindReferenceFor3D(Object* node) { return reinterpret_cast<Form*(__cdecl*)(Object*)>(0x56F930)(node); }

	inline bool IsActor(const Form* form) { return form && (form->type == kCharacter || form->type == kCreature); }

	inline Form* BaseForm(const Form* ref) { return *reinterpret_cast<Form* const*>(reinterpret_cast<const uint8_t*>(ref) + 0x20); }

	inline bool IsLooseItem(const Form* ref)
	{
		auto base = BaseForm(ref);
		return base && reinterpret_cast<bool(__cdecl*)(uint32_t)>(0x481F30)(base->type);
	}

	inline const Vec3& RefPosition(const Form* ref) { return *reinterpret_cast<const Vec3*>(reinterpret_cast<const uint8_t*>(ref) + 0x30); }

	inline Object* RefRoot(const Form* ref)
	{
		auto state = *reinterpret_cast<uint8_t* const*>(reinterpret_cast<const uint8_t*>(ref) + 0x64);
		return state ? *reinterpret_cast<Object**>(state + 0x14) : nullptr;
	}

	inline uint32_t LifeState(const Form* actor) { return *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(actor) + 0x108); }

	//state 6 is essential knockdown, not death
	inline bool IsDeadState(uint32_t state) { return state == 1 || state == 2; }

	constexpr int kBodyParts = 15;

	//0x8b3ef0 walks these body part nodes
	inline Object* PartNode(const Form* actor, int part)
	{
		auto process = *reinterpret_cast<void** const*>(reinterpret_cast<const uint8_t*>(actor) + 0x68);
		if (!process || part < 0 || part >= kBodyParts) return nullptr;
		return reinterpret_cast<Object*(__thiscall*)(void*, int)>((*reinterpret_cast<void***>(process))[0x6D0 / 4])(process, part);
	}

	//tesactorbasedata is at +0x30 in tescreature
	inline bool NoBloodDecal(const Form* ref)
	{
		auto base = BaseForm(ref);
		return base && base->type == kCreatureBase && reinterpret_cast<bool(__thiscall*)(const uint8_t*)>(0x60B1F0)(reinterpret_cast<const uint8_t*>(base) + 0x30);
	}

	inline uint32_t ActorFilter(Form* actor)
	{
		uint32_t fi = 0;
		reinterpret_cast<uint32_t*(__thiscall*)(Form*, uint32_t*)>(0x931ED0)(actor, &fi);
		return (fi & 0xFFFF0000) | kLayerLOS;
	}

	inline void PickInit(PickData& p) { reinterpret_cast<void(__thiscall*)(PickData*)>(0x4A3C20)(&p); }

	inline void PickFrom(PickData& p, const Vec3& v) { reinterpret_cast<void(__thiscall*)(PickData*, const Vec3*)>(0x4A3DA0)(&p, &v); }

	inline void PickTo(PickData& p, const Vec3& v) { reinterpret_cast<void(__thiscall*)(PickData*, const Vec3*)>(0x4A3EB0)(&p, &v); }

	inline void CastPick(PickData& p) { reinterpret_cast<void(__thiscall*)(void*, PickData*)>(0x458420)(TES(), &p); }

	//the pick wrapper dereferences an empty grid
	inline bool PickWorldReady()
	{
		auto tes = static_cast<uint8_t*>(TES());
		if (!tes) return false;
		if (*reinterpret_cast<void**>(tes + 0x34)) return true;
		auto grid = *reinterpret_cast<uint8_t**>(tes + 0x08);
		return grid && *reinterpret_cast<uint32_t*>(grid + 0x0C) != 0;
	}

	inline Object* LandNodeAt(const Vec3& pos) { return reinterpret_cast<Object*(__thiscall*)(void*, const Vec3*)>(0x457620)(TES(), &pos); }

	inline Object* CollidableNode(void* collidable) { return reinterpret_cast<Object*(__cdecl*)(void*)>(0xC7FA90)(collidable); }

	inline void* CellAt(const Vec3& pos) { return reinterpret_cast<void*(__thiscall*)(void*, float, float, float)>(0x4519D0)(TES(), pos.x, pos.y, pos.z); }

	inline void DecalInit(DecalData& d) { reinterpret_cast<void(__thiscall*)(DecalData*)>(0x4A37B0)(&d); }

	inline void CellAddDecal(void* cell, DecalData& d, int type, bool force) { reinterpret_cast<void(__thiscall*)(void*, DecalData*, int, bool)>(0x4A3FE0)(cell, &d, type, force); }

	//0xe99330 reuses pick records between casts
	struct alignas(16) ScenePick {
		uint8_t bytes[0x100];
	};

	inline void ScenePickInit(ScenePick& p)
	{
		for (uint8_t& b : p.bytes) b = 0;
		reinterpret_cast<void(__thiscall*)(ScenePick*, uint16_t, int16_t)>(0xE98F20)(&p, 0, 8);
		p.bytes[0x11] = 1;
	}

	inline void ScenePickFree(ScenePick& p) { reinterpret_cast<void(__thiscall*)(ScenePick*)>(0xE98FA0)(&p); }

	inline void ScenePickTarget(ScenePick& p) { reinterpret_cast<void(__thiscall*)(ScenePick*, void*)>(0x705FC0)(&p, *reinterpret_cast<void**>(static_cast<uint8_t*>(TES()) + 0x0C)); }

	inline bool ScenePickCast(ScenePick& p, const Vec3& origin, const Vec3& direction) { return reinterpret_cast<bool(__thiscall*)(ScenePick*, const Vec3*, const Vec3*, bool)>(0xE98E20)(&p, &origin, &direction, false) != 0; }

	inline int ScenePickCount(const ScenePick& p) { return *reinterpret_cast<const int*>(p.bytes + 0x28); }

	inline const uint8_t* ScenePickRecord(const ScenePick& p, int i)
	{
		auto records = *reinterpret_cast<uint8_t* const* const*>(p.bytes + 0x1C);
		return records ? records[i] : nullptr;
	}

	inline Object* RecordObject(const uint8_t* r) { return *reinterpret_cast<Object* const*>(r); }

	inline const Vec3& RecordPoint(const uint8_t* r) { return *reinterpret_cast<const Vec3*>(r + 0x08); }

	inline float RecordDistance(const uint8_t* r) { return *reinterpret_cast<const float*>(r + 0x14); }

	inline const char* EditorID(Form* form) { return reinterpret_cast<const char*(__thiscall*)(Form*)>(form->vtable[0x130 / 4])(form); }

	inline bool IsRobot(const Form* ref)
	{
		auto base = BaseForm(ref);
		return base && base->type == kCreatureBase && reinterpret_cast<const uint8_t*>(base)[0x12C] == 6;
	}
}
