#undef NDEBUG
#include "../src/PoolMesh.h"
#include "../src/Config.h"
#include "../src/Rig.h"
#include <windows.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace Pools;
using namespace Pools::Engine;

extern "C" uint8_t EngineSpace[];

namespace Pools {
	Settings settings;
}

namespace Pools::Decals {
	int CastsLeft() { return 1000; }

	Cast Ground(uint32_t, const Vec3& from, float, Surface& out)
	{
		out.pos = from;
		out.pos.z = 0;
		return Cast::kHit;
	}

	Cast Visual(const Vec3& from, float, Vec3& hit)
	{
		hit = from;
		hit.z = 0;
		return Cast::kHit;
	}
}

namespace {
	struct Node : Object {
		Object* children[32]{};
		uint32_t count = 0;
	};

	struct Geometry : Object {
		uint8_t pad[0xB8 - sizeof(Object)]{};
		GeometryData* data = nullptr;
	};

	struct Data : GeometryData {
		uint16_t* triangles;
	};

	struct Cell : Form {
		Node* scene;
		uint32_t state;
	};

	static_assert(offsetof(Geometry, data) == 0xB8 && sizeof(Data) <= 0x58);
	void* s_nodeTable[64]{};
	void* s_geometryTable[64]{};
	void* s_dataTable[2]{};
	Node* s_model = nullptr;
	Cell s_cell{};
	int32_t s_nodes = 0, s_geometry = 0, s_data = 0, s_allocations = 0;
	int32_t s_cacheCount = 0, s_processes = 0, s_failAllocation = -1;
	bool s_missing = false, s_failClone = false;

	void* __cdecl Allocate(size_t bytes)
	{
		if (s_failAllocation == 0) {
			s_failAllocation = -1;
			return nullptr;
		}
		if (s_failAllocation > 0) --s_failAllocation;
		void* p = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
		assert(p);
		++s_allocations;
		return p;
	}

	void __cdecl Free(void* memory, size_t)
	{
		if (!memory) return;
		assert(HeapFree(GetProcessHeap(), 0, memory));
		--s_allocations;
	}

	void __fastcall Drop(Object* object, void*)
	{
		assert(object->references > 0);
		if (!--object->references) reinterpret_cast<void(__fastcall*)(Object*, void*)>(object->vtable[1])(object, nullptr);
	}

	void __fastcall FreeData(Data* data, void*)
	{
		Free(data->vertices, 0);
		Free(data->normals, 0);
		Free(data->colours, 0);
		Free(data->uvs, 0);
		Free(data->triangles, 0);
		Free(data, 0);
		--s_data;
	}

	void __fastcall FreeGeometry(Geometry* object, void*)
	{
		Release(object->data);
		delete object;
		--s_geometry;
	}

	void __fastcall FreeNode(Node* node, void*)
	{
		while (node->count) {
			Object* child = node->children[--node->count];
			child->parent = nullptr;
			Release(child);
		}
		delete node;
		--s_nodes;
	}

	Object* __fastcall NodeCast(Object* object, void*) { return object; }

	Object* __fastcall GeometryCast(Object*, void*) { return nullptr; }

	void __fastcall AddChild(Node* node, void*, Object* child, bool)
	{
		assert(!child->parent && node->count < 32);
		node->children[node->count++] = child;
		child->parent = node;
		AddRef(child);
	}

	void __fastcall RemoveChild(Node* node, void*, Object* child)
	{
		for (uint32_t i = 0; i < node->count; ++i) {
			if (node->children[i] != child) continue;
			node->children[i] = node->children[--node->count];
			child->parent = nullptr;
			Release(child);
			return;
		}
		assert(false);
	}

	void __fastcall SetData(Geometry* object, void*, GeometryData* data)
	{
		AddRef(data);
		if (object->data) Release(object->data);
		object->data = data;
	}

	void __fastcall UpdateBounds(Object*, void*) {}

	Node* NewNode()
	{
		auto node = new Node{};
		node->vtable = s_nodeTable;
		++s_nodes;
		return node;
	}

	Geometry* NewGeometry(GeometryData* data)
	{
		auto object = new Geometry{};
		object->vtable = s_geometryTable;
		SetData(object, nullptr, data);
		++s_geometry;
		return object;
	}

	Object* __fastcall Load(void*, void*, const char*, uint32_t, bool, uint32_t, bool, bool dontCount)
	{
		assert(!dontCount);
		if (s_missing) return nullptr;
		++s_cacheCount;
		return s_model;
	}

	void __fastcall Unload(void*, void*, const char*, bool, int16_t count)
	{
		if (s_missing) return;
		assert(count == 1 && s_cacheCount > 0);
		--s_cacheCount;
	}

	void __fastcall StartClone(CloneProcess* process, void*, float scale)
	{
		process->scale = {scale, scale, scale};
		++s_processes;
	}

	void __fastcall FinishClone(CloneProcess*, void*) { --s_processes; }

	Object* __fastcall Copy(Object* source, void*, CloneProcess* process)
	{
		assert(source == s_model && s_processes == 1 && process->scale.x == 1);
		if (s_failClone) return nullptr;
		Node* node = NewNode();
		AddChild(node, nullptr, NewGeometry(ModelData(s_model->children[0])), true);
		return node;
	}

	uint32_t __fastcall CellState(Cell* cell, void*) { return cell->state; }

	Object* __fastcall GetCellNode(Cell* cell, void*, uint32_t index)
	{
		assert(index == kCellStaticNode);
		return cell->scene;
	}

	void* __fastcall GetCell(void*, void*, float, float, float) { return &s_cell; }

	Form* __cdecl GetForm(uint32_t id) { return id == s_cell.id ? &s_cell : nullptr; }

	int32_t __fastcall Count(Node* node, void*) { return node->count; }

	Object* __fastcall Child(Node* node, void*, int32_t i) { return node->children[i]; }

	void __fastcall Rotate(float* matrix, void*, float yaw)
	{
		std::memset(matrix, 0, sizeof(float) * 9);
		matrix[0] = matrix[4] = std::cos(yaw);
		matrix[1] = std::sin(yaw);
		matrix[3] = -matrix[1];
		matrix[8] = 1;
	}

	void __fastcall Update(Node* node, void*, UpdateData*)
	{
		node->world = node->local;
		for (uint32_t i = 0; i < node->count; ++i) node->children[i]->world = node->world;
	}

	GeometryData* __fastcall NewData(Data* data, void*, uint32_t n, Vec3* vertices, Vec3* normals, float* colours, float* uvs, uint16_t, uint16_t, uint32_t, uint16_t* triangles)
	{
		for (uint32_t i = 0; i < n; ++i) assert(vertices[i].x == 0 && vertices[i].y == 0 && vertices[i].z == 0);
		data->vtable = s_dataTable;
		data->numVertices = static_cast<uint16_t>(n);
		data->vertices = vertices;
		data->normals = normals;
		data->colours = colours;
		data->uvs = uvs;
		data->triangles = triangles;
		++s_data;
		return data;
	}

	void __fastcall SetFlags(GeometryData*, void*, uint16_t) {}

	template<class T>
	void Patch(uint32_t address, T target)
	{
		auto code = reinterpret_cast<uint8_t*>(address);
		DWORD previous;
		assert(VirtualProtect(code, 5, PAGE_EXECUTE_READWRITE, &previous));
		code[0] = 0xE9;
		*reinterpret_cast<uint32_t*>(code + 1) = reinterpret_cast<uint32_t>(target) - address - 5;
	}

	void Install()
	{
		//reserve engine addresses below the test code
		assert(GetModuleHandleA(nullptr) == reinterpret_cast<HMODULE>(0x400000));
		assert(reinterpret_cast<uint32_t>(EngineSpace) <= 0x401970 && reinterpret_cast<uint32_t>(EngineSpace) + 0xE00000 > 0x11DEA10);
		Patch(0xAA13E0, Allocate);
		Patch(0xAA1460, Free);
		Patch(0x401970, Drop);
		Patch(0x447080, Load);
		Patch(0x445300, Unload);
		Patch(0x4AD050, StartClone);
		Patch(0x4AD270, FinishClone);
		Patch(0xA5D2C0, Copy);
		Patch(0x450FD0, CellState);
		Patch(0x456FC0, GetCellNode);
		Patch(0x4519D0, GetCell);
		Patch(0x4839C0, GetForm);
		Patch(0x43B480, Count);
		Patch(0x43B4A0, Child);
		Patch(0x4A0C90, Rotate);
		Patch(0xA5A040, UpdateBounds);
		Patch(0xA59C60, Update);
		Patch(0xA7B630, NewData);
		Patch(0xA67050, SetFlags);
		Patch(0xA67090, SetFlags);
		FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
		s_nodeTable[1] = reinterpret_cast<void*>(FreeNode);
		s_nodeTable[3] = reinterpret_cast<void*>(NodeCast);
		s_nodeTable[0xDC / 4] = reinterpret_cast<void*>(AddChild);
		s_nodeTable[0xE8 / 4] = reinterpret_cast<void*>(RemoveChild);
		s_nodeTable[0xFC / 4] = reinterpret_cast<void*>(UpdateBounds);
		s_geometryTable[1] = reinterpret_cast<void*>(FreeGeometry);
		s_geometryTable[3] = reinterpret_cast<void*>(GeometryCast);
		s_geometryTable[0xE4 / 4] = reinterpret_cast<void*>(SetData);
		s_geometryTable[0xBC / 4] = reinterpret_cast<void*>(UpdateBounds);
		s_dataTable[1] = reinterpret_cast<void*>(FreeData);
		s_cell.id = 1;
		s_cell.state = kCellAttached;
		s_cell.scene = NewNode();
		AddRef(s_cell.scene);
		s_model = NewNode();
		AddRef(s_model);
		auto data = NewTriShapeData(1, static_cast<Vec3*>(Allocate(sizeof(Vec3))), nullptr, nullptr, nullptr, 0, 0, 0, nullptr);
		data->vertices[0].x = 123;
		AddChild(s_model, nullptr, NewGeometry(data), true);
		Mesh::Init();
	}

	Decals::Cast Spawn() { return Mesh::Spawn({0, 0, 0}, 48, 0, 7, Mesh::Fluid::kBlood); }

	void Empty()
	{
		assert(!s_cell.scene->count && !s_cacheCount && !s_processes);
		assert(s_nodes == 2 && s_geometry == 1 && s_data == 1 && s_allocations == 2);
	}
}

int main()
{
	_set_error_mode(_OUT_TO_STDERR);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	Install();
	assert(Spawn() == Decals::Cast::kHit);
	auto first = static_cast<Node*>(s_cell.scene->children[0]);
	Object* firstDisc = first->children[0];
	assert(first != s_model && ModelData(firstDisc) != ModelData(s_model->children[0]));
	assert(Spawn() == Decals::Cast::kHit);
	auto second = static_cast<Node*>(s_cell.scene->children[1]);
	assert(ModelData(firstDisc) != ModelData(second->children[0]));
	assert(Mesh::Contains(firstDisc) && Mesh::Contains(second->children[0]) && !Mesh::Contains(s_model->children[0]));
	settings.lifetime = 2;
	settings.poolFade = 1;
	Mesh::Update(1.5f);
	assert(ModelData(firstDisc)->colours[3] == .5f);
	assert(ModelData(s_model->children[0])->vertices[0].x == 123);
	//pick records retain geometry
	AddRef(firstDisc);
	Mesh::Clear();
	assert(!firstDisc->parent && firstDisc->references == 1 && !Mesh::Contains(firstDisc));
	Release(firstDisc);
	Empty();
	assert(Spawn() == Decals::Cast::kHit);
	Release(s_cell.scene);
	s_cell.scene = NewNode();
	AddRef(s_cell.scene);
	Mesh::Update(.1f);
	Empty();
	assert(Spawn() == Decals::Cast::kHit);
	s_cell.state = 4;
	Mesh::Update(.1f);
	Empty();
	assert(Spawn() == Decals::Cast::kDeferred);
	s_cell.state = kCellAttached;
	settings.maxPools = 1;
	assert(Spawn() == Decals::Cast::kHit && Spawn() == Decals::Cast::kHit);
	assert(s_cell.scene->count == 1 && s_nodes == 3);
	Mesh::Update(3);
	Empty();
	s_missing = true;
	assert(Spawn() == Decals::Cast::kMiss);
	s_missing = false;
	s_failClone = true;
	assert(Spawn() == Decals::Cast::kMiss);
	s_failClone = false;
	Empty();
	for (int32_t i = 0; i < 6; ++i) {
		s_failAllocation = i;
		assert(Spawn() == Decals::Cast::kMiss);
		Empty();
	}
	Release(s_model);
	Release(s_cell.scene);
	assert(!s_nodes && !s_geometry && !s_data && !s_allocations);
	puts("MeshTest OK");
}
