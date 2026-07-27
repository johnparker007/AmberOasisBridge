#include "amber/amber_api.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <mutex>
#include <new>
#include <string>
#include <vector>

// These aliases and prototypes exactly mirror JPMSystem6/PA2CoreInterface.h and
// JPMSystem6/Interface.h. The maintained project explicitly compiles as __cdecl.
using JpmUint8 = uint8_t;
using JpmUint32 = uint32_t;
using JpmInt32 = int32_t;
using JpmGetDllVersion = float (__cdecl *)(void);
using JpmInitialise = JpmUint8 (__cdecl *)(void);
using JpmShutdown = JpmUint8 (__cdecl *)(void);
using JpmReset = void (__cdecl *)(void);
using JpmRun = JpmInt32 (__cdecl *)(JpmUint32);
using JpmLoadRom = JpmUint32 (__cdecl *)(JpmUint8*, JpmUint8*, JpmUint8*, JpmUint8*);

enum class State { Created, Initialised, Running, Shutdown, InitialiseFailed };
struct Exports {
    JpmGetDllVersion version{};
    JpmInitialise initialise{};
    JpmShutdown shutdown{};
    JpmReset reset{};
    JpmRun run{};
    JpmLoadRom load_rom{};
    JpmLoadRom load_sound_rom{};
};

struct AmberInstance_t {
    HMODULE library{};
    Exports exports{};
    State state{State::Created};
    bool core_initialised{false};
    bool core_shutdown{false};
    char error[2048]{};
};

namespace {
std::mutex g_mutex;
AmberInstance_t* g_instance = nullptr;
thread_local char g_error[2048] = {};

void CopyError(char* destination, size_t capacity, const char* message) noexcept {
    if (!destination || capacity == 0) return;
    if (!message) message = "internal bridge error";
    std::snprintf(destination, capacity, "%s", message);
    destination[capacity - 1] = '\0';
}

bool IsLive(AmberHandle handle) noexcept { return handle != nullptr && handle == g_instance; }

void SetGlobalError(const char* message) noexcept { CopyError(g_error, sizeof(g_error), message); }
void SetLiveError(AmberHandle handle, const char* message) noexcept {
    SetGlobalError(message);
    if (IsLive(handle)) CopyError(handle->error, sizeof(handle->error), message);
}

AmberResult InvalidHandle() noexcept {
    SetGlobalError("invalid, stale, or destroyed Amber handle");
    return AMBER_INVALID_STATE;
}

void SetExceptionError(AmberHandle handle, const char* detail) noexcept {
    char message[2048]{};
    std::snprintf(message, sizeof(message), "C++ exception contained at Amber C ABI boundary: %s",
                  detail ? detail : "unknown exception");
    SetLiveError(handle, message);
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                     static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) return "<UTF-16 conversion failed>";
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
    return result;
}

std::wstring WindowsMessage(DWORD code) {
    wchar_t* allocated = nullptr;
    DWORD count = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                     FORMAT_MESSAGE_IGNORE_INSERTS,
                                 nullptr, code, 0, reinterpret_cast<wchar_t*>(&allocated), 0, nullptr);
    std::wstring result = count && allocated ? std::wstring(allocated, count) : L"No system message available.";
    if (allocated) LocalFree(allocated);
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n' || result.back() == L' ')) result.pop_back();
    return result;
}

void SetWindowsError(const char* operation, const std::wstring& path, DWORD code) {
    std::string utf8_path = WideToUtf8(path);
    std::string text = WideToUtf8(WindowsMessage(code));
    char message[2048]{};
    std::snprintf(message, sizeof(message), "%s%s%s\nWindows error %lu: %s", operation,
                  path.empty() ? "" : "\n", utf8_path.c_str(), static_cast<unsigned long>(code), text.c_str());
    SetGlobalError(message);
}

bool ModulePath(HMODULE module, std::wstring& result, const char* operation) {
    for (DWORD capacity = 256; capacity <= 32768; capacity *= 2) {
        std::vector<wchar_t> buffer(capacity);
        SetLastError(ERROR_SUCCESS);
        DWORD count = GetModuleFileNameW(module, buffer.data(), capacity);
        if (count == 0) { DWORD error = GetLastError(); SetWindowsError(operation, {}, error); return false; }
        if (count < capacity - 1 || (count < capacity && GetLastError() != ERROR_INSUFFICIENT_BUFFER)) {
            result.assign(buffer.data(), count); return true;
        }
    }
    SetGlobalError("module path exceeds the supported Windows extended path limit");
    return false;
}

bool BridgeRelativeCorePath(std::wstring& result) {
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&g_instance), &self)) {
        DWORD error = GetLastError(); SetWindowsError("GetModuleHandleExW failed for AmberBridge.dll", {}, error); return false;
    }
    std::wstring bridge_path;
    if (!ModulePath(self, bridge_path, "GetModuleFileNameW failed for AmberBridge.dll")) return false;
    size_t separator = bridge_path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) { SetGlobalError("AmberBridge.dll module path has no containing directory"); return false; }
    result = bridge_path.substr(0, separator + 1) + L"AmberOasis.JPMSystem6.dll";
    return true;
}

template<typename T> bool Resolve(HMODULE library, const char* name, T& target) noexcept {
    target = reinterpret_cast<T>(GetProcAddress(library, name));
    if (target) return true;
    DWORD error = GetLastError();
    char message[512]{};
    std::snprintf(message, sizeof(message), "required JPM export missing: %s (Windows error %lu)",
                  name, static_cast<unsigned long>(error));
    SetGlobalError(message); return false;
}

AmberResult BridgeInfoImpl(AmberBridgeInfo* info) {
    if (!info || info->struct_size < sizeof(AmberBridgeInfo)) return AMBER_INVALID_ARGUMENT;
    info->api_version = AMBER_API_VERSION_1; info->name = "Amber Bridge"; info->bridge_version = "0.1.1"; return AMBER_OK;
}
AmberResult AMBER_CALL BridgeInfo(AmberBridgeInfo* info) noexcept {
    try { return BridgeInfoImpl(info); } catch (const std::exception& e) { SetExceptionError(nullptr,e.what()); } catch (...) { SetExceptionError(nullptr,nullptr); }
    return AMBER_INTERNAL_ERROR;
}

AmberResult EnumerateImpl(uint32_t index, AmberCoreInfo* info) {
    if (!info || info->struct_size < sizeof(AmberCoreInfo)) return AMBER_INVALID_ARGUMENT;
    info->core_id = nullptr; info->display_name = nullptr;
    if (index != 0) return AMBER_NO_MORE_ITEMS;
    info->core_id = "jpm-system6"; info->display_name = "JPM System 6"; return AMBER_OK;
}
AmberResult AMBER_CALL Enumerate(uint32_t i, AmberCoreInfo* p) noexcept {
    try { return EnumerateImpl(i,p); } catch (const std::exception& e) { SetExceptionError(nullptr,e.what()); } catch (...) { SetExceptionError(nullptr,nullptr); }
    return AMBER_INTERNAL_ERROR;
}

AmberResult CreateImpl(const char* core, AmberHandle* output) {
    if (output) *output = nullptr;
    if (!core || !output) return AMBER_INVALID_ARGUMENT;
    if (std::strcmp(core,"jpm-system6") != 0) { SetGlobalError("unknown core id"); return AMBER_INVALID_ARGUMENT; }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_instance) { SetGlobalError("one emulator instance is already active"); return AMBER_INSTANCE_LIMIT; }
    std::wstring path;
    if (!BridgeRelativeCorePath(path)) return AMBER_DLL_LOAD_FAILED;
    HMODULE library = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library) { DWORD error=GetLastError(); SetWindowsError("Failed to load JPM core:",path,error); return AMBER_DLL_LOAD_FAILED; }
    AmberInstance_t* instance = new (std::nothrow) AmberInstance_t{};
    if (!instance) { FreeLibrary(library); SetGlobalError("could not allocate Amber instance"); return AMBER_INTERNAL_ERROR; }
    instance->library=library;
    Exports& e=instance->exports;
    if (!Resolve(library,"GetDLLVersion",e.version) || !Resolve(library,"Initialise",e.initialise) ||
        !Resolve(library,"LoadROM",e.load_rom) || !Resolve(library,"LoadSoundROM",e.load_sound_rom) ||
        !Resolve(library,"Reset",e.reset) || !Resolve(library,"Run",e.run) || !Resolve(library,"Shutdown",e.shutdown)) {
        FreeLibrary(library); delete instance; return AMBER_EXPORT_MISSING;
    }
    (void)e.version(); g_instance=instance; *output=instance; return AMBER_OK;
}
AmberResult AMBER_CALL Create(const char* c, AmberHandle* h) noexcept {
    try { return CreateImpl(c,h); } catch (const std::exception& e) { if(h)*h=nullptr; SetExceptionError(nullptr,e.what()); } catch (...) { if(h)*h=nullptr; SetExceptionError(nullptr,nullptr); }
    return AMBER_INTERNAL_ERROR;
}

AmberResult DestroyImpl(AmberHandle handle) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!IsLive(handle)) return InvalidHandle();
    if (handle->core_initialised && !handle->core_shutdown) { SetLiveError(handle,"shutdown is required before destroy"); return AMBER_INVALID_STATE; }
    HMODULE library=handle->library; handle->library=nullptr; g_instance=nullptr;
    if (!FreeLibrary(library)) { DWORD error=GetLastError(); delete handle; SetWindowsError("FreeLibrary failed for JPM core",{},error); return AMBER_INTERNAL_ERROR; }
    delete handle; return AMBER_OK;
}
AmberResult AMBER_CALL Destroy(AmberHandle h) noexcept {
    try { return DestroyImpl(h); } catch (const std::exception& e) { SetExceptionError(IsLive(h)?h:nullptr,e.what()); } catch (...) { SetExceptionError(IsLive(h)?h:nullptr,nullptr); }
    return AMBER_INTERNAL_ERROR;
}

using MutablePath = std::vector<JpmUint8>;
MutablePath CopyPath(const char* path) {
    if (!path) return {};
    const auto* first=reinterpret_cast<const JpmUint8*>(path);
    return MutablePath(first, first + std::strlen(path) + 1);
}
JpmUint8* Pointer(MutablePath& path) noexcept { return path.empty() ? nullptr : path.data(); }
bool LoadPaths(JpmLoadRom function, const char* const paths[4]) {
    MutablePath a=CopyPath(paths[0]), b=CopyPath(paths[1]), c=CopyPath(paths[2]), d=CopyPath(paths[3]);
    return function(Pointer(a),Pointer(b),Pointer(c),Pointer(d)) != 0;
}

AmberResult InitialiseImpl(AmberHandle h, const AmberInitialiseParams* p) {
    if (!IsLive(h)) return InvalidHandle();
    if (h->state != State::Created) { SetLiveError(h,"initialise requires the Created state"); return AMBER_INVALID_STATE; }
    if (p && p->struct_size < sizeof(AmberInitialiseParams)) return AMBER_INVALID_ARGUMENT;
    if (!h->exports.initialise()) { h->state=State::InitialiseFailed; SetLiveError(h,"JPM Initialise failed; Destroy is now permitted"); return AMBER_INITIALISE_FAILED; }
    h->core_initialised=true;
    bool loaded=true;
    if (p && p->program_roms[0]) loaded=LoadPaths(h->exports.load_rom,p->program_roms);
    if (loaded && p && p->sound_roms[0]) loaded=LoadPaths(h->exports.load_sound_rom,p->sound_roms);
    if (!loaded) {
        (void)h->exports.shutdown(); h->core_shutdown=true; h->core_initialised=false; h->state=State::InitialiseFailed;
        SetLiveError(h,"JPM ROM load failed; core was shut down and Destroy is now permitted"); return AMBER_INITIALISE_FAILED;
    }
    h->state=State::Initialised; return AMBER_OK;
}
AmberResult AMBER_CALL Initialise(AmberHandle h,const AmberInitialiseParams* p) noexcept {
    try { return InitialiseImpl(h,p); } catch (const std::exception& e) {
        if (IsLive(h) && h->core_initialised && !h->core_shutdown) { (void)h->exports.shutdown(); h->core_shutdown=true; h->core_initialised=false; h->state=State::InitialiseFailed; }
        SetExceptionError(IsLive(h)?h:nullptr,e.what());
    } catch (...) {
        if (IsLive(h) && h->core_initialised && !h->core_shutdown) { (void)h->exports.shutdown(); h->core_shutdown=true; h->core_initialised=false; h->state=State::InitialiseFailed; }
        SetExceptionError(IsLive(h)?h:nullptr,nullptr);
    }
    return AMBER_INTERNAL_ERROR;
}

AmberResult ResetImpl(AmberHandle h) {
    if (!IsLive(h)) return InvalidHandle();
    if (h->state!=State::Initialised && h->state!=State::Running) { SetLiveError(h,"reset requires an initialised or running instance"); return AMBER_INVALID_STATE; }
    h->exports.reset(); h->state=State::Initialised; return AMBER_OK;
}
AmberResult AMBER_CALL Reset(AmberHandle h) noexcept { try{return ResetImpl(h);}catch(const std::exception& e){SetExceptionError(IsLive(h)?h:nullptr,e.what());}catch(...){SetExceptionError(IsLive(h)?h:nullptr,nullptr);}return AMBER_INTERNAL_ERROR; }

AmberResult RunImpl(AmberHandle h,uint32_t cycles,int32_t* ran) {
    if (ran) *ran=0;
    if (!IsLive(h)) return InvalidHandle();
    if (!ran) return AMBER_INVALID_ARGUMENT;
    if (h->state!=State::Initialised && h->state!=State::Running) { SetLiveError(h,"run requires an initialised or running instance"); return AMBER_INVALID_STATE; }
    *ran=h->exports.run(cycles); h->state=State::Running; return AMBER_OK;
}
AmberResult AMBER_CALL Run(AmberHandle h,uint32_t c,int32_t* r) noexcept { try{return RunImpl(h,c,r);}catch(const std::exception& e){SetExceptionError(IsLive(h)?h:nullptr,e.what());}catch(...){SetExceptionError(IsLive(h)?h:nullptr,nullptr);}return AMBER_INTERNAL_ERROR; }

AmberResult ShutdownImpl(AmberHandle h) {
    if (!IsLive(h)) return InvalidHandle();
    if (!h->core_initialised || h->core_shutdown || (h->state!=State::Initialised && h->state!=State::Running)) { SetLiveError(h,"shutdown requires an active initialised or running core"); return AMBER_INVALID_STATE; }
    JpmUint8 result=h->exports.shutdown(); h->core_shutdown=true; h->core_initialised=false;
    if (!result) { h->state=State::InitialiseFailed; SetLiveError(h,"JPM Shutdown reported failure; Destroy is permitted"); return AMBER_INTERNAL_ERROR; }
    h->state=State::Shutdown; return AMBER_OK;
}
AmberResult AMBER_CALL Shutdown(AmberHandle h) noexcept { try{return ShutdownImpl(h);}catch(const std::exception& e){SetExceptionError(IsLive(h)?h:nullptr,e.what());}catch(...){SetExceptionError(IsLive(h)?h:nullptr,nullptr);}return AMBER_INTERNAL_ERROR; }

AmberResult LastErrorImpl(AmberHandle h,char* buffer,uint32_t capacity,uint32_t* required) {
    if (h && !IsLive(h)) return InvalidHandle();
    const char* value=h ? h->error : g_error;
    size_t length=std::strlen(value)+1;
    if (required) *required=static_cast<uint32_t>(length);
    if (!buffer && capacity==0 && required) return AMBER_OK;
    if (!buffer) return AMBER_INVALID_ARGUMENT;
    if (capacity<length) return AMBER_BUFFER_TOO_SMALL;
    std::memcpy(buffer,value,length); return AMBER_OK;
}
AmberResult AMBER_CALL LastError(AmberHandle h,char* b,uint32_t c,uint32_t* r) noexcept { try{return LastErrorImpl(h,b,c,r);}catch(const std::exception& e){SetExceptionError(IsLive(h)?h:nullptr,e.what());}catch(...){SetExceptionError(IsLive(h)?h:nullptr,nullptr);}return AMBER_INTERNAL_ERROR; }
}

extern "C" AMBER_EXPORT AmberResult AMBER_CALL AmberGetApi(uint32_t version,uint32_t size,AmberApiV1* api) {
    try {
        if (!api) return AMBER_INVALID_ARGUMENT;
        if (version!=AMBER_API_VERSION_1) return AMBER_UNSUPPORTED_VERSION;
        if (size<sizeof(AmberApiV1)) return AMBER_BUFFER_TOO_SMALL;
        const AmberApiV1 value={sizeof(AmberApiV1),AMBER_API_VERSION_1,BridgeInfo,Enumerate,Create,Destroy,Initialise,Reset,Run,Shutdown,LastError};
        std::memcpy(api,&value,sizeof(value)); return AMBER_OK;
    } catch (const std::exception& e) { SetExceptionError(nullptr,e.what()); }
      catch (...) { SetExceptionError(nullptr,nullptr); }
    return AMBER_INTERNAL_ERROR;
}
