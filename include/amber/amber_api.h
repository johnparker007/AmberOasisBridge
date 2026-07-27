#ifndef AMBER_API_H
#define AMBER_API_H

#include "amber_types.h"
#include "amber_version.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AmberApiV1 {
    uint32_t struct_size;
    uint32_t api_version;
    AmberResult (AMBER_CALL *GetBridgeInfo)(AmberBridgeInfo* info);
    AmberResult (AMBER_CALL *EnumerateCore)(uint32_t index, AmberCoreInfo* info);
    AmberResult (AMBER_CALL *Create)(const char* core_id, AmberHandle* handle);
    AmberResult (AMBER_CALL *Destroy)(AmberHandle handle);
    AmberResult (AMBER_CALL *Initialise)(AmberHandle handle, const AmberInitialiseParams* params);
    AmberResult (AMBER_CALL *Reset)(AmberHandle handle);
    AmberResult (AMBER_CALL *Run)(AmberHandle handle, uint32_t cycles, int32_t* cycles_run);
    AmberResult (AMBER_CALL *Shutdown)(AmberHandle handle);
    AmberResult (AMBER_CALL *GetLastError)(AmberHandle handle, char* buffer, uint32_t capacity, uint32_t* required);
} AmberApiV1;

AMBER_EXPORT AmberResult AMBER_CALL AmberGetApi(uint32_t requested_version,
                                                uint32_t api_size,
                                                AmberApiV1* api);

#ifdef __cplusplus
}
#endif
#endif
