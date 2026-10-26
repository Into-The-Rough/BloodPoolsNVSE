#include "Body.h"
#include <cstring>

namespace Pools::Body {
	using namespace Engine;
	constexpr const char* kSeveredPrefix = "DismemberedLimb";
	constexpr size_t kSeveredPrefixLength = 15;
	constexpr int kMaxDepth = 64;

	static bool IsSeveredRoot(const Object* node) { return node->name && !strncmp(node->name, kSeveredPrefix, kSeveredPrefixLength); }

	static bool InSevered(const Object* node, const Object* root)
	{
		for (int depth = 0; node && node != root && depth < kMaxDepth; node = node->parent, ++depth)
			if (IsSeveredRoot(node)) return true;
		return false;
	}

	//severed clones keep the original bone names
	static Object* FindOnBody(Object* node, const char* name, int depth)
	{
		if (!node || depth > kMaxDepth || IsSeveredRoot(node)) return nullptr;
		if (node->name && !strcmp(node->name, name)) return node;
		Object* asNode = AsNode(node);
		if (!asNode) return nullptr;
		int n = ChildCount(asNode);
		for (int i = 0; i < n; ++i)
			if (Object* found = FindOnBody(ChildAt(asNode, i), name, depth + 1)) return found;
		return nullptr;
	}

	Object* Stump(const Form* actor, Object* root, int part)
	{
		Object* node = PartNode(actor, part);
		if (!node || !InSevered(node, root)) return node;
		return node->name ? FindOnBody(root, node->name, 0) : nullptr;
	}

	Object* Severed(const Form* actor, Object* root, int part)
	{
		Object* node = PartNode(actor, part);
		return node && InSevered(node, root) ? node : nullptr;
	}
}
