#ifndef FABRIC_FABRIC_AMBER_H
#define FABRIC_FABRIC_AMBER_H

#include "fabric.h"
#include "amber/amber_api.h"

#define FABRIC_AMBER_CONFIGURATION_MAGIC UINT32_C(0x32424146) /* FAB2 */
#define FABRIC_AMBER_CONFIGURATION_VERSION_1 1u

/* Versioned backend-only blob. It is deliberately not part of FabricLaunchRequest. */
typedef struct FabricAmberConfigurationV1 {
    uint32_t magic;
    uint32_t struct_size;
    uint32_t version;
    uint32_t flags;
    AmberReelConfigurationV1 reels;
    AmberCoinConfigurationV1 coins;
    uint32_t percentage_switch;
    uint32_t reserved[3];
} FabricAmberConfigurationV1;

#define FABRIC_AMBER_CONFIGURE_REELS UINT32_C(1)
#define FABRIC_AMBER_CONFIGURE_COINS UINT32_C(2)
#define FABRIC_AMBER_CONFIGURE_PERCENTAGE UINT32_C(4)

#endif
