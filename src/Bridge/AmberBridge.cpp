#include "amber/amber_api.h"

#include <windows.h>
#include <cstring>
#include <mutex>
#include <new>
#include <string>

enum class State { LibraryLoaded, Created, Initialised, Running, Shutdown, Destroyed, Error };
typedef float (__cdecl *GetVersionFn)();
typedef unsigned char (__cdecl *InitialiseFn)();
typedef unsigned char (__cdecl *ShutdownFn)();
typedef void (__cdecl *ResetFn)();
typedef int32_t (__cdecl *RunFn)(uint32_t);
typedef uint32_t (__cdecl *LoadRomFn)(unsigned char*, unsigned char*, unsigned char*, unsigned char*);

struct Exports {
    GetVersionFn version;
    InitialiseFn initialise;
    ShutdownFn shutdown;
    ResetFn reset;
    RunFn run;
    LoadRomFn load_rom;
    LoadRomFn load_sound_rom;
};

std::mutex g_mutex;
struct AmberInstance_t* g_instance = nullptr;
thread_local std::string g_error;

void SetError(struct AmberInstance_t* instance, const char* text);

struct AmberInstance_t {
    HMODULE library;
    Exports exports;
    State state;
    std::string error;
};

namespace {
void SetError(AmberInstance_t* instance, const char* text) {
    g_error = text;
    if (instance) instance->error = text;
}

template<typename T> bool Resolve(HMODULE library, const char* name, T& target) {
    target = reinterpret_cast<T>(GetProcAddress(library, name));
    if (!target) {
        SetError(nullptr, (std::string("required JPM export missing: ") + name).c_str());
        return false;
    }
    return true;
}

AmberResult AMBER_CALL BridgeInfo(AmberBridgeInfo* info) {
    if (!info || info->struct_size < sizeof(*info)) return AMBER_INVALID_ARGUMENT;
    info->api_version = AMBER_API_VERSION_1; info->name = "Amber Bridge"; info->bridge_version = "0.1.0";
    return AMBER_OK;
}
AmberResult AMBER_CALL Enumerate(uint32_t index, AmberCoreInfo* info) {
    if (!info || info->struct_size < sizeof(*info)) return AMBER_INVALID_ARGUMENT;
    if (index != 0) return AMBER_INVALID_ARGUMENT;
    info->core_id = "jpm-system6"; info->display_name = "JPM System 6"; return AMBER_OK;
}
AmberResult AMBER_CALL Create(const char* core, AmberHandle* output) {
    if (!core || !output) return AMBER_INVALID_ARGUMENT;
    *output = nullptr;
    if (std::strcmp(core, "jpm-system6") != 0) { SetError(nullptr, "unknown core id"); return AMBER_INVALID_ARGUMENT; }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_instance) { SetError(nullptr, "one emulator instance is already active"); return AMBER_INSTANCE_LIMIT; }
    HMODULE dll = LoadLibraryExW(L"AmberOasis.JPMSystem6.dll", nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!dll) { SetError(nullptr, "could not load AmberOasis.JPMSystem6.dll"); return AMBER_DLL_LOAD_FAILED; }
    AmberInstance_t* instance = new (std::nothrow) AmberInstance_t{};
    if (!instance) { FreeLibrary(dll); return AMBER_INTERNAL_ERROR; }
    instance->library = dll; instance->state = State::LibraryLoaded;
    Exports& e = instance->exports;
    if (!Resolve(dll,"GetDLLVersion",e.version) || !Resolve(dll,"Initialise",e.initialise) ||
        !Resolve(dll,"Shutdown",e.shutdown) || !Resolve(dll,"Reset",e.reset) ||
        !Resolve(dll,"Run",e.run) || !Resolve(dll,"LoadROM",e.load_rom) ||
        !Resolve(dll,"LoadSoundROM",e.load_sound_rom)) {
        instance->error = g_error; FreeLibrary(dll); delete instance; return AMBER_EXPORT_MISSING;
    }
    (void)e.version();
    instance->state = State::Created; g_instance = instance; *output = instance; return AMBER_OK;
}
bool IsCurrent(AmberHandle handle) { return handle && handle == g_instance; }
AmberResult AMBER_CALL Destroy(AmberHandle handle) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!IsCurrent(handle)) { SetError(nullptr,"invalid or destroyed handle"); return AMBER_INVALID_STATE; }
    if (handle->state == State::Initialised || handle->state == State::Running) { SetError(handle,"shutdown is required before destroy"); return AMBER_INVALID_STATE; }
    FreeLibrary(handle->library); handle->state = State::Destroyed; g_instance = nullptr; delete handle; return AMBER_OK;
}
AmberResult AMBER_CALL Initialise(AmberHandle h, const AmberInitialiseParams* p) {
    if (!IsCurrent(h) || h->state != State::Created) { SetError(h,"initialise requires the Created state"); return AMBER_INVALID_STATE; }
    if (p && p->struct_size < sizeof(*p)) return AMBER_INVALID_ARGUMENT;
    if (!h->exports.initialise()) { h->state=State::Error; SetError(h,"JPM Initialise failed"); return AMBER_INITIALISE_FAILED; }
    if (p && p->program_roms[0]) {
        auto cv=[](const char* s){return reinterpret_cast<unsigned char*>(const_cast<char*>(s));};
        if (!h->exports.load_rom(cv(p->program_roms[0]),cv(p->program_roms[1]),cv(p->program_roms[2]),cv(p->program_roms[3]))) {
            h->exports.shutdown(); h->state=State::Error; SetError(h,"JPM program ROM load failed"); return AMBER_INITIALISE_FAILED;
        }
        if (p->sound_roms[0] && !h->exports.load_sound_rom(cv(p->sound_roms[0]),cv(p->sound_roms[1]),cv(p->sound_roms[2]),cv(p->sound_roms[3]))) {
            h->exports.shutdown(); h->state=State::Error; SetError(h,"JPM sound ROM load failed"); return AMBER_INITIALISE_FAILED;
        }
    }
    h->state=State::Initialised; return AMBER_OK;
}
AmberResult AMBER_CALL Reset(AmberHandle h) {
    if (!IsCurrent(h) || (h->state!=State::Initialised && h->state!=State::Running)) { SetError(h,"reset requires an initialised instance"); return AMBER_INVALID_STATE; }
    h->exports.reset(); h->state=State::Initialised; return AMBER_OK;
}
AmberResult AMBER_CALL Run(AmberHandle h,uint32_t cycles,int32_t* ran) {
    if (!IsCurrent(h) || (h->state!=State::Initialised && h->state!=State::Running)) { SetError(h,"run requires an initialised instance"); return AMBER_INVALID_STATE; }
    if (!ran) return AMBER_INVALID_ARGUMENT;
    *ran=h->exports.run(cycles); h->state=State::Running; return AMBER_OK;
}
AmberResult AMBER_CALL Shutdown(AmberHandle h) {
    if (!IsCurrent(h) || (h->state!=State::Initialised && h->state!=State::Running && h->state!=State::Error)) { SetError(h,"shutdown requires an initialised, running, or error instance"); return AMBER_INVALID_STATE; }
    if (!h->exports.shutdown()) { h->state=State::Error; SetError(h,"JPM Shutdown failed"); return AMBER_INTERNAL_ERROR; }
    h->state=State::Shutdown; return AMBER_OK;
}
AmberResult AMBER_CALL LastError(AmberHandle h,char* buffer,uint32_t capacity,uint32_t* required) {
    const std::string& value = IsCurrent(h) ? h->error : g_error;
    uint32_t need=static_cast<uint32_t>(value.size()+1); if (required) *required=need;
    if (!buffer || capacity<need) return AMBER_INVALID_ARGUMENT;
    std::memcpy(buffer,value.c_str(),need); return AMBER_OK;
}
}

extern "C" AMBER_EXPORT AmberResult AMBER_CALL AmberGetApi(uint32_t version,uint32_t size,AmberApiV1* api) {
    if (!api || size < sizeof(AmberApiV1)) return AMBER_INVALID_ARGUMENT;
    if (version != AMBER_API_VERSION_1) return AMBER_UNSUPPORTED_VERSION;
    AmberApiV1 value={sizeof(AmberApiV1),AMBER_API_VERSION_1,BridgeInfo,Enumerate,Create,Destroy,Initialise,Reset,Run,Shutdown,LastError};
    std::memcpy(api,&value,sizeof(value)); return AMBER_OK;
}
