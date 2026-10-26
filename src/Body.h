#pragma once
#include "Engine.h"

namespace Pools::Body {
	Engine::Object* Stump(const Engine::Form* actor, Engine::Object* root, int part);
	Engine::Object* Severed(const Engine::Form* actor, Engine::Object* root, int part);
}
