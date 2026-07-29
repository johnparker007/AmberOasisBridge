#include "fabric/fabric.h"
#include <stddef.h>
#include <string.h>
_Static_assert(sizeof(((FabricCharacterDisplay *)0)->characters) ==
                   FABRIC_CHARACTER_CAPACITY * sizeof(uint32_t),
               "inline character ownership");
_Static_assert(sizeof(((FabricSegmentDisplay *)0)->segment_masks) ==
                   FABRIC_SEGMENT_DIGIT_CAPACITY * sizeof(uint64_t),
               "inline segment ownership");
typedef struct OriginalLaunch {
  uint32_t struct_size, struct_version;
  char backend_kind[64], machine_identifier[64], backend_path[1024];
  const char *const *rom_paths;
  uint32_t rom_path_count;
  const void *machine_configuration;
  uint32_t machine_configuration_size, reserved;
} OriginalLaunch;
int main(void) {
  FabricRuntime *r = 0;
  FabricMachineSession *s = 0;
  OriginalLaunch q = {0};
  q.struct_size = sizeof(q);
  q.struct_version = FABRIC_ABI_VERSION_1;
  strcpy(q.backend_kind, "amber-api-v2");
  strcpy(q.machine_identifier, "jpm-system6");
  strcpy(q.backend_path, FAKE_AMBER_PATH);
  if (FabricCreateRuntime(FABRIC_ABI_VERSION_1, &r) != FABRIC_OK)
    return 1;
  if (FabricCreateSession(r, (const FabricLaunchRequest *)&q, &s) != FABRIC_OK)
    return 2;
  FabricDestroySession(s);
  FabricDestroyRuntime(r);
  return 0;
}
