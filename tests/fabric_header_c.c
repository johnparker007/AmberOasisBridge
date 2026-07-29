#include "fabric/fabric.h"
#include <stddef.h>
_Static_assert(sizeof(((FabricCharacterDisplay*)0)->characters) == FABRIC_CHARACTER_CAPACITY * sizeof(uint32_t), "inline character ownership");
_Static_assert(sizeof(((FabricSegmentDisplay*)0)->segment_masks) == FABRIC_SEGMENT_DIGIT_CAPACITY * sizeof(uint64_t), "inline segment ownership");
int main(void) { return offsetof(FabricLaunchRequest, rom_resources) <= offsetof(FabricLaunchRequest, rom_resource_count) ? 0 : 1; }
