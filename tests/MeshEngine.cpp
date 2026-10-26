#include <cstdint>

#pragma code_seg(".text")
extern "C" __declspec(allocate(".text")) uint8_t EngineSpace[0xE00000]{};
