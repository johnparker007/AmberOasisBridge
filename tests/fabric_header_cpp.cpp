#include "fabric/fabric.h"
#include <type_traits>
static_assert(std::is_standard_layout<FabricMachineSnapshot>::value, "C ABI snapshot");
static_assert(std::is_standard_layout<FabricRomResource>::value, "C ABI resource");
int main() { return 0; }
