#ifndef AMBER_TYPES_H
#define AMBER_TYPES_H

#include <stdint.h>

#if defined(_WIN32)
#define AMBER_CALL __cdecl
#if defined(AMBER_BRIDGE_EXPORTS)
#define AMBER_EXPORT __declspec(dllexport)
#else
#define AMBER_EXPORT __declspec(dllimport)
#endif
#else
#define AMBER_CALL
#define AMBER_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AmberInstance_t* AmberHandle;

typedef enum AmberResult {
    AMBER_OK = 0,
    AMBER_INVALID_ARGUMENT = 1,
    AMBER_UNSUPPORTED_VERSION = 2,
    AMBER_DLL_LOAD_FAILED = 3,
    AMBER_EXPORT_MISSING = 4,
    AMBER_INVALID_STATE = 5,
    AMBER_INSTANCE_LIMIT = 6,
    AMBER_INITIALISE_FAILED = 7,
    AMBER_INTERNAL_ERROR = 8,
    AMBER_NO_MORE_ITEMS = 9,
    AMBER_BUFFER_TOO_SMALL = 10
} AmberResult;

typedef struct AmberBridgeInfo {
    uint32_t struct_size;
    uint32_t api_version;
    const char* name;
    const char* bridge_version;
} AmberBridgeInfo;

typedef struct AmberCoreInfo {
    uint32_t struct_size;
    const char* core_id;
    const char* display_name;
} AmberCoreInfo;

typedef struct AmberInitialiseParams {
    uint32_t struct_size;
    const char* program_roms[4];
    const char* sound_roms[4];
} AmberInitialiseParams;

#ifdef __cplusplus
}
#endif
#endif
