#undef NDEBUG
#include "../src/Wounds.h"
#include <cassert>
#include <cstdio>
using namespace Pools::Wounds;

int main()
{
	for (int p = 0; p < 7; ++p) Record(5, p, 10.f + p, 0);
	Record(9, 3, 1, 0);
	Wound w[kMaxPerBody];
	int n = Take(5, 1, w);
	assert(n == kMaxPerBody && w[0].part == 1);
	for (int i = 1; i < n; ++i) assert(w[i].part != 1);
	assert(Take(5, 1, w) == 0);
	assert(Take(9, 3, w) == 1 && w[0].part == 3);
	Wound list[kMaxPerBody] = {{1, 5, 0}};
	int count = Merge(list, 1, 1, 3, 0x20);
	assert(count == 1 && list[0].damage == 8 && list[0].flags == 0x20);
	count = Merge(list, count, 7, 2, 0);
	assert(count == 2 && list[1].part == 7);
	puts("WoundsTest OK");
	return 0;
}
