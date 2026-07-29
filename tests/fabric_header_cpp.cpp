#include "fabric/fabric.h"
#include <cstring>
#include <type_traits>
static_assert(std::is_standard_layout<FabricMachineSnapshot>::value,
              "C ABI snapshot");
static_assert(std::is_standard_layout<FabricRomResource>::value,
              "C ABI resource");
struct OriginalLaunch {
  uint32_t struct_size, struct_version;
  char backend_kind[64], machine_identifier[64], backend_path[1024];
  const char *const *rom_paths;
  uint32_t rom_path_count;
  const void *machine_configuration;
  uint32_t machine_configuration_size, reserved;
};
int main() {
  OriginalLaunch q{};
  q.struct_size = sizeof(q);
  q.struct_version = FABRIC_ABI_VERSION_1;
  std::strcpy(q.backend_kind, "amber-api-v2");
  std::strcpy(q.machine_identifier, "jpm-system6");
  std::strcpy(q.backend_path, FAKE_AMBER_PATH);
  FabricRuntime *r = nullptr;
  FabricMachineSession *s = nullptr;
  if (FabricCreateRuntime(FABRIC_ABI_VERSION_1, &r) != FABRIC_OK)
    return 1;
  if (FabricCreateSession(r, reinterpret_cast<const FabricLaunchRequest *>(&q),
                          &s) != FABRIC_OK)
    return 2;
  FabricDestroySession(s);
  FabricDestroyRuntime(r);
  return 0;
}
