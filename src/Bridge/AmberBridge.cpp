#include "amber/amber_api.h"
#include "AmberBridgeV2Adapter.h"
#include "../Cores/JPMSystem6/PA2CoreInterface.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <cmath>
#include <limits>
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
using JpmByteSetter = void (__cdecl *)(JpmUint8, JpmUint8);
using JpmCounterSetter = void (__cdecl *)(JpmUint8, JpmUint32);
using JpmSwitch = void (__cdecl *)(JpmUint8);
using JpmGetSnapshotSize = JpmUint32 (__cdecl *)(void);
using JpmGetSnapshot = JpmUint32 (__cdecl *)(void*, JpmUint32);
using JpmGetAudioFormat = JpmUint32 (__cdecl *)(PA2_AudioFormat*, JpmUint32);
using JpmFillAudioFrames = JpmUint32 (__cdecl *)(int16_t*, JpmUint32);

enum class State { Created, Initialised, Running, Shutdown, InitialiseFailed, ConfigurationInconsistent };
struct Exports {
    JpmGetDllVersion version{};
    JpmInitialise initialise{};
    JpmShutdown shutdown{};
    JpmReset reset{};
    JpmRun run{};
    JpmLoadRom load_rom{};
    JpmLoadRom load_sound_rom{};
    JpmSwitch switch_on{}, switch_off{};
    JpmGetSnapshotSize snapshot_size{}; JpmGetSnapshot snapshot{};
    JpmGetAudioFormat audio_format{}; JpmFillAudioFrames audio_frames{};
    JpmByteSetter steps{}, opto_start{}, opto_end{}, opto_invert{};
    JpmByteSetter coin_enable{}, coin_value{}, lockout_value{}, lockout_invert{};
    JpmByteSetter route_enable{}, port_index{}, coin{}, level{}, full_level{};
    JpmCounterSetter counter_in{}, counter_out{};
    JpmSwitch percent{};
};

struct AmberInstance_t {
    HMODULE library{};
    Exports exports{};
    State state{State::Created};
    bool core_initialised{false};
    bool core_shutdown{false};
    char error[2048]{};
    uint64_t capabilities{};
    AmberReelConfigurationV1 reel_config{}; bool has_reel_config{};
    AmberCoinConfigurationV1 coin_config{}; bool has_coin_config{};
    uint8_t percentage{}; bool has_percentage{};
    bool configuration_consistent{true};
};

static_assert(sizeof(PA2_LampState)==20, "maintained PA2 lamp layout changed");
static_assert(offsetof(PA2_OutputSnapshot, MatrixLamps)==80, "maintained PA2 snapshot header changed");
static_assert(offsetof(PA2_OutputSnapshot, Reels)>offsetof(PA2_OutputSnapshot, Leds), "maintained PA2 snapshot array order changed");
static_assert(offsetof(PA2_OutputSnapshot, AlphaSegmented)>offsetof(PA2_OutputSnapshot, Reels), "maintained PA2 alpha layout changed");
static_assert(offsetof(PA2_OutputSnapshot, LedDisplays)>offsetof(PA2_OutputSnapshot, AlphaSegmented), "maintained PA2 LED layout changed");

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

AmberResult LiveFailure(AmberHandle handle, AmberResult result, const char* message) noexcept {
    SetLiveError(handle,message); return result;
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
template<typename T> bool ResolveOptional(HMODULE library, const char* name, T& target) noexcept {
    target = reinterpret_cast<T>(GetProcAddress(library, name)); return target != nullptr;
}

AmberResult BridgeInfoImpl(AmberBridgeInfo* info) {
    if (!info || info->struct_size < sizeof(AmberBridgeInfo)) return AMBER_INVALID_ARGUMENT;
    // This record is the immutable v0.1.1 information contract used by the
    // released Oasis wrapper.  The returned table's api_version identifies
    // whether v1 or v2 was negotiated.
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
    if (ResolveOptional(library,"TurnSwitchOn",e.switch_on) && ResolveOptional(library,"TurnSwitchOff",e.switch_off))
        instance->capabilities |= AMBER_CAP_SWITCH_INPUT;
    if (ResolveOptional(library,"GetOutputSnapshotSize",e.snapshot_size) && ResolveOptional(library,"GetOutputSnapshot",e.snapshot))
        instance->capabilities |= AMBER_CAP_OUTPUT_SNAPSHOT;
    if (ResolveOptional(library,"GetAudioFormat",e.audio_format) && ResolveOptional(library,"FillAudioFrames",e.audio_frames))
        instance->capabilities |= AMBER_CAP_AUDIO;
    if (ResolveOptional(library,"SetSteps",e.steps) && ResolveOptional(library,"SetOptoStart",e.opto_start) &&
        ResolveOptional(library,"SetOptoEnd",e.opto_end) && ResolveOptional(library,"SetOptoInvert",e.opto_invert))
        instance->capabilities |= AMBER_CAP_REEL_CONFIGURATION;
    if (ResolveOptional(library,"SetCoinEnable",e.coin_enable) && ResolveOptional(library,"SetCoinValue",e.coin_value) &&
        ResolveOptional(library,"SetLockoutVal",e.lockout_value) && ResolveOptional(library,"SetLockoutInvert",e.lockout_invert) &&
        ResolveOptional(library,"SetEnable",e.route_enable) && ResolveOptional(library,"SetCounterIn",e.counter_in) &&
        ResolveOptional(library,"SetCounterOut",e.counter_out) && ResolveOptional(library,"SetPortIndex",e.port_index) &&
        ResolveOptional(library,"SetCoin",e.coin) && ResolveOptional(library,"SetLevel",e.level) && ResolveOptional(library,"SetFullLevel",e.full_level))
        instance->capabilities |= AMBER_CAP_COIN_CONFIGURATION;
    if (ResolveOptional(library,"SetPercent",e.percent)) instance->capabilities |= AMBER_CAP_PERCENT_SWITCH;
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

void ApplyReels(AmberInstance_t* h, const AmberReelConfigurationV1& c) {
    for (uint32_t i=0;i<AMBER_MAX_REELS;i++) if (c.apply_mask & (1u<<i)) {
        const auto& r=c.reels[i];
        if (r.enabled) { h->exports.steps((uint8_t)i,(uint8_t)r.steps); h->exports.opto_start((uint8_t)i,(uint8_t)r.opto_start);
            h->exports.opto_end((uint8_t)i,(uint8_t)r.opto_end); h->exports.opto_invert((uint8_t)i,(uint8_t)r.opto_invert); }
    }
}
void ApplyCoins(AmberInstance_t* h, const AmberCoinConfigurationV1& c) {
    for (uint32_t i=0;i<AMBER_MAX_COIN_CHANNELS;i++) if(c.channel_apply_mask&(1u<<i)) { const auto& x=c.channels[i];
        h->exports.coin_enable((uint8_t)i,(uint8_t)x.enabled); h->exports.coin_value((uint8_t)i,(uint8_t)x.value); h->exports.lockout_invert((uint8_t)i,(uint8_t)x.lockout_invert); }
    if(c.configuration_flags&AMBER_COIN_CONFIG_APPLY_LOCKOUT_PORT) h->exports.lockout_value((uint8_t)c.lockout_port_base,(uint8_t)c.lockout_port_value);
    for(uint32_t i=0;i<AMBER_MAX_COIN_ROUTES;i++) if(c.route_apply_mask&(1u<<i)) { const auto& x=c.routes[i];
        h->exports.route_enable((uint8_t)i,(uint8_t)x.enabled); h->exports.counter_in((uint8_t)i,x.counter_in); h->exports.counter_out((uint8_t)i,x.counter_out);
        h->exports.port_index((uint8_t)i,(uint8_t)x.port_index); h->exports.coin((uint8_t)i,(uint8_t)x.coin_code); h->exports.level((uint8_t)i,(uint8_t)x.level); h->exports.full_level((uint8_t)i,(uint8_t)x.full_level); }
}

AmberResult ResetImpl(AmberHandle h) {
    if (!IsLive(h)) return InvalidHandle();
    if (h->state!=State::Initialised && h->state!=State::Running && h->state!=State::ConfigurationInconsistent) { SetLiveError(h,"reset requires an initialised, running, or configuration-inconsistent instance"); return AMBER_INVALID_STATE; }
    h->exports.reset();
    try { if(h->has_reel_config) ApplyReels(h,h->reel_config); if(h->has_coin_config) ApplyCoins(h,h->coin_config); if(h->has_percentage) h->exports.percent(h->percentage); }
    catch (...) { h->configuration_consistent=false; h->state=State::ConfigurationInconsistent; SetLiveError(h,"reset configuration reapplication failed"); return AMBER_INTERNAL_ERROR; }
    h->configuration_consistent=true; h->state=State::Initialised; return AMBER_OK;
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
    if (!h->core_initialised || h->core_shutdown || (h->state!=State::Initialised && h->state!=State::Running && h->state!=State::ConfigurationInconsistent)) { SetLiveError(h,"shutdown requires an active initialised, running, or configuration-inconsistent core"); return AMBER_INVALID_STATE; }
    JpmUint8 result=h->exports.shutdown(); h->core_shutdown=true; h->core_initialised=false;
    h->has_reel_config=false; h->has_coin_config=false; h->has_percentage=false; h->configuration_consistent=true;
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

bool Operational(AmberHandle h, const char* operation) {
    if (!IsLive(h)) return false;
    if (!h->configuration_consistent || (h->state!=State::Initialised && h->state!=State::Running)) { char m[160]{}; std::snprintf(m,sizeof(m),"%s requires a consistent initialised instance",operation); SetLiveError(h,m); return false; }
    return true;
}
AmberResult Unsupported(AmberHandle h,const char* feature) { char m[180]{}; std::snprintf(m,sizeof(m),"JPM System 6 does not provide the optional %s exports",feature); SetLiveError(h,m); return AMBER_NOT_SUPPORTED; }
bool ReservedZero(const uint32_t* p,size_t n) { for(size_t i=0;i<n;i++) if(p[i]) return false; return true; }

AmberResult CapabilitiesImpl(AmberHandle h,AmberCapabilitiesV1* c) {
    if(!IsLive(h)) return InvalidHandle(); if(!c) { SetLiveError(h,"capabilities pointer is null"); return AMBER_INVALID_ARGUMENT; }
    if(c->struct_size<sizeof(*c)) { SetLiveError(h,"capabilities structure is too small"); return AMBER_BUFFER_TOO_SMALL; }
    if(c->version!=AMBER_CAPABILITIES_VERSION_1) return LiveFailure(h,AMBER_INVALID_ARGUMENT,"unsupported capabilities structure version");
    if(!ReservedZero(c->reserved,3)) return LiveFailure(h,AMBER_MALFORMED_CONFIGURATION,"capabilities reserved input fields must be zero");
    std::memset(c,0,sizeof(*c)); c->struct_size=sizeof(*c); c->version=AMBER_CAPABILITIES_VERSION_1; c->feature_bits=h->capabilities; c->max_switches=AMBER_MAX_SWITCHES; return AMBER_OK;
}
#define AMBER_WRAP2(name,args,call) AmberResult AMBER_CALL name args noexcept { try{return call;}catch(const std::exception& e){SetExceptionError(IsLive(h)?h:nullptr,e.what());}catch(...){SetExceptionError(IsLive(h)?h:nullptr,nullptr);}return AMBER_INTERNAL_ERROR; }
AMBER_WRAP2(GetCapabilities,(AmberHandle h,AmberCapabilitiesV1* c),CapabilitiesImpl(h,c))

AmberResult SwitchImpl(AmberHandle h,uint32_t index,uint32_t on) { if(!IsLive(h)) return InvalidHandle(); if(!Operational(h,"SetSwitchState")) return AMBER_INVALID_STATE;
    if(!(h->capabilities&AMBER_CAP_SWITCH_INPUT)) return Unsupported(h,"switch input"); if(index>=AMBER_MAX_SWITCHES || on>1) { SetLiveError(h,"switch index must be 0..255 and state exactly 0 or 1"); return AMBER_INVALID_RANGE; }
    (on?h->exports.switch_on:h->exports.switch_off)((uint8_t)index); return AMBER_OK; }
AMBER_WRAP2(SetSwitchState,(AmberHandle h,uint32_t i,uint32_t on),SwitchImpl(h,i,on))

AmberResult SnapshotImpl(AmberHandle h,AmberOutputSnapshotV1* out) { if(!IsLive(h)) return InvalidHandle(); if(!Operational(h,"GetOutputSnapshot")) return AMBER_INVALID_STATE;
    if(!(h->capabilities&AMBER_CAP_OUTPUT_SNAPSHOT)) return Unsupported(h,"output snapshot"); if(!out) { SetLiveError(h,"snapshot pointer is null"); return AMBER_INVALID_ARGUMENT; }
    if(out->struct_size<sizeof(*out)) { SetLiveError(h,"snapshot structure is too small"); return AMBER_BUFFER_TOO_SMALL; }
    if(out->version!=AMBER_OUTPUT_SNAPSHOT_VERSION_1) return LiveFailure(h,AMBER_INVALID_ARGUMENT,"unsupported output snapshot structure version");
    if(!ReservedZero(out->reserved,4)) return LiveFailure(h,AMBER_MALFORMED_CONFIGURATION,"output snapshot reserved input fields must be zero");
    std::memset(out,0,sizeof(*out)); out->struct_size=sizeof(*out); out->version=AMBER_OUTPUT_SNAPSHOT_VERSION_1;
    const uint32_t native_size=h->exports.snapshot_size(); if(native_size!=sizeof(PA2_OutputSnapshot)) { SetLiveError(h,"maintained output snapshot size is incompatible"); return AMBER_INTERNAL_ERROR; }
    PA2_OutputSnapshot native{}; uint32_t got=h->exports.snapshot(&native,sizeof(native)); const auto* n=&native;
    if(got!=native_size || n->SizeBytes!=native_size || n->Version!=PA2_OUTPUT_SNAPSHOT_VERSION || n->MatrixLampCount<512 || n->ReelCount<8 || n->AlphaSegmentedDisplayCount<1 || n->LedCount<256) { SetLiveError(h,"maintained output snapshot returned malformed size, version, or counts"); return AMBER_INTERNAL_ERROR; }
    out->matrix_lamp_count=512; out->reel_count=8; out->alpha_display_count=1; out->seven_segment_display_count=16;
    for(uint32_t i=0;i<512;i++) { out->matrix_lamps[i].is_on=n->MatrixLamps[i].OnOff?1u:0u; if(!amber_v2::ToQ16_16(n->MatrixLamps[i].Brightness,out->matrix_lamps[i].brightness_q16_16)) goto nonfinite; }
    for(uint32_t i=0;i<8;i++) out->reel_positions[i]=n->Reels[i].Position;
    if(!amber_v2::ConvertAlpha(n->AlphaSegmented[0].Segments,n->AlphaSegmented[0].DotComma,n->AlphaSegmented[0].Brightness,out->alpha_displays[0])) goto nonfinite;
    { uint32_t on[256]{}; double brightness[256]{}; for(uint32_t i=0;i<256;i++) { on[i]=n->Leds[i].OnOff; brightness[i]=n->Leds[i].Brightness; } if(!amber_v2::ConvertSevenSegmentPlane(on,brightness,out->seven_segment_displays)) goto nonfinite; }
    return AMBER_OK;
nonfinite: std::memset(out,0,sizeof(*out)); out->struct_size=sizeof(*out); out->version=AMBER_OUTPUT_SNAPSHOT_VERSION_1; SetLiveError(h,"maintained snapshot contains non-finite brightness"); return AMBER_INTERNAL_ERROR;
}
AMBER_WRAP2(GetOutputSnapshot,(AmberHandle h,AmberOutputSnapshotV1* s),SnapshotImpl(h,s))

AmberResult AudioFormatImpl(AmberHandle h,AmberAudioFormatV1* out) { if(!IsLive(h)) return InvalidHandle(); if(!Operational(h,"GetAudioFormat")) return AMBER_INVALID_STATE; if(!(h->capabilities&AMBER_CAP_AUDIO)) return Unsupported(h,"audio");
    if(!out) return LiveFailure(h,AMBER_INVALID_ARGUMENT,"audio format pointer is null");
    if(out->struct_size<sizeof(*out)) return LiveFailure(h,AMBER_BUFFER_TOO_SMALL,"audio format structure is too small");
    if(out->version!=AMBER_AUDIO_FORMAT_VERSION_1) return LiveFailure(h,AMBER_INVALID_ARGUMENT,"unsupported audio format structure version");
    if(!ReservedZero(out->reserved,2)) return LiveFailure(h,AMBER_MALFORMED_CONFIGURATION,"audio format reserved input fields must be zero");
    std::memset(out,0,sizeof(*out)); out->struct_size=sizeof(*out); out->version=AMBER_AUDIO_FORMAT_VERSION_1; PA2_AudioFormat n{}; uint32_t got=h->exports.audio_format(&n,sizeof(n));
    if(got!=sizeof(n)||n.SizeBytes!=sizeof(n)||n.Version!=PA2_AUDIO_FORMAT_VERSION||n.SampleRate!=48000||n.Channels!=2||n.BitsPerSample!=16||n.Format!=PA2_AUDIO_FORMAT_PCM_S16) { SetLiveError(h,"maintained audio format is incompatible with PCM S16 stereo 48 kHz"); return AMBER_INTERNAL_ERROR; }
    out->sample_rate=48000; out->channels=2; out->sample_format=AMBER_AUDIO_SAMPLE_PCM_S16; out->interleaving=AMBER_AUDIO_INTERLEAVED; return AMBER_OK; }
AMBER_WRAP2(GetAudioFormat,(AmberHandle h,AmberAudioFormatV1* f),AudioFormatImpl(h,f))
AmberResult FramesImpl(AmberHandle h,int16_t* samples,uint32_t capacity,uint32_t* written) { if(!written) { if(IsLive(h)) SetLiveError(h,"frames_written pointer is null"); return AMBER_INVALID_ARGUMENT; } *written=0; if(!IsLive(h)) return InvalidHandle(); if(!Operational(h,"FillAudioFrames")) return AMBER_INVALID_STATE; if(!(h->capabilities&AMBER_CAP_AUDIO)) return Unsupported(h,"audio");
    if(capacity==0) return AMBER_OK; if(!samples) return LiveFailure(h,AMBER_INVALID_ARGUMENT,"audio sample buffer is null for nonzero capacity");
    if((reinterpret_cast<uintptr_t>(samples)%alignof(int16_t))!=0) return LiveFailure(h,AMBER_INVALID_ARGUMENT,"audio sample buffer is not aligned for int16_t");
    uint32_t sample_count=0, byte_count=0;
    if(!amber_v2::AudioExtent(capacity,2,sample_count,byte_count)) return LiveFailure(h,AMBER_INVALID_RANGE,"audio capacity overflows the contract's 32-bit sample or byte count");
    uint32_t got=h->exports.audio_frames(samples,capacity); if(got>capacity) { SetLiveError(h,"maintained audio export returned more frames than requested"); return AMBER_INTERNAL_ERROR; } *written=got; return AMBER_OK; }
AMBER_WRAP2(FillAudioFrames,(AmberHandle h,int16_t* s,uint32_t c,uint32_t* w),FramesImpl(h,s,c,w))

AmberResult ReelsImpl(AmberHandle h,const AmberReelConfigurationV1* c) { if(!IsLive(h)) return InvalidHandle(); if(!Operational(h,"ConfigureReels")) return AMBER_INVALID_STATE; if(!(h->capabilities&AMBER_CAP_REEL_CONFIGURATION)) return Unsupported(h,"reel configuration");
    if(!c) return LiveFailure(h,AMBER_INVALID_ARGUMENT,"reel configuration pointer is null");
    if(c->struct_size<sizeof(*c)) return LiveFailure(h,AMBER_BUFFER_TOO_SMALL,"reel configuration structure is too small");
    if(c->version!=AMBER_REEL_CONFIGURATION_VERSION_1) return LiveFailure(h,AMBER_INVALID_ARGUMENT,"unsupported reel configuration structure version");
    bool valid=c->reel_count==AMBER_MAX_REELS&&(c->apply_mask&~0xffu)==0;
    for(uint32_t i=0;i<8&&valid;i++) if(c->apply_mask&(1u<<i)) { const auto&r=c->reels[i]; valid=r.reel_index==i&&r.enabled<=1&&r.steps>=1&&r.steps<=255&&r.opto_start<=255&&r.opto_end<=255&&r.opto_invert<=1&&r.opto_start<=r.opto_end; }
    if(!valid) return LiveFailure(h,AMBER_MALFORMED_CONFIGURATION,"malformed reel aggregate: check count, mask, indexes, booleans, steps, and opto ordering");
    ApplyReels(h,*c);
    amber_v2::MergeReels(h->reel_config,h->has_reel_config,*c); return AMBER_OK; }
AMBER_WRAP2(ConfigureReels,(AmberHandle h,const AmberReelConfigurationV1* c),ReelsImpl(h,c))

AmberResult CoinsImpl(AmberHandle h,const AmberCoinConfigurationV1* c) { if(!IsLive(h)) return InvalidHandle(); if(!Operational(h,"ConfigureCoins")) return AMBER_INVALID_STATE; if(!(h->capabilities&AMBER_CAP_COIN_CONFIGURATION)) return Unsupported(h,"coin configuration");
    if(!c) return LiveFailure(h,AMBER_INVALID_ARGUMENT,"coin configuration pointer is null");
    if(c->struct_size<sizeof(*c)) return LiveFailure(h,AMBER_BUFFER_TOO_SMALL,"coin configuration structure is too small");
    if(c->version!=AMBER_COIN_CONFIGURATION_VERSION_1) return LiveFailure(h,AMBER_INVALID_ARGUMENT,"unsupported coin configuration structure version");
    bool valid=(c->channel_apply_mask&~0x3fu)==0&&(c->route_apply_mask&~0xffu)==0&&(c->configuration_flags&~AMBER_COIN_CONFIG_APPLY_LOCKOUT_PORT)==0&&c->reserved==0;
    if(valid&&(c->configuration_flags&AMBER_COIN_CONFIG_APPLY_LOCKOUT_PORT)) valid=c->lockout_port_base<=255&&c->lockout_port_value<=255;
    for(uint32_t i=0;i<6&&valid;i++) if(c->channel_apply_mask&(1u<<i)) { const auto&x=c->channels[i]; valid=x.channel_index==i&&x.enabled<=1&&x.value<=255&&x.lockout_invert<=1&&x.reserved==0; }
    for(uint32_t i=0;i<8&&valid;i++) if(c->route_apply_mask&(1u<<i)) { const auto&x=c->routes[i]; valid=x.route_index==i&&x.enabled<=1&&x.port_index<=7&&x.coin_code<=255&&x.level<=255&&x.full_level<=255; }
    if(!valid) return LiveFailure(h,AMBER_MALFORMED_CONFIGURATION,"malformed coin aggregate: check masks, flags, indexes, booleans, byte ranges, port index, and reserved fields");
    ApplyCoins(h,*c);
    amber_v2::MergeCoins(h->coin_config,h->has_coin_config,*c); return AMBER_OK; }
AMBER_WRAP2(ConfigureCoins,(AmberHandle h,const AmberCoinConfigurationV1* c),CoinsImpl(h,c))
AmberResult PercentImpl(AmberHandle h,uint32_t value) { if(!IsLive(h)) return InvalidHandle(); if(!Operational(h,"SetPercentageSwitch")) return AMBER_INVALID_STATE; if(!(h->capabilities&AMBER_CAP_PERCENT_SWITCH)) return Unsupported(h,"percentage switch"); if(value>15) { SetLiveError(h,"percentage switch raw value must be 0..15"); return AMBER_INVALID_RANGE; } h->exports.percent((uint8_t)value); h->percentage=(uint8_t)value; h->has_percentage=true; return AMBER_OK; }
AMBER_WRAP2(SetPercentageSwitch,(AmberHandle h,uint32_t v),PercentImpl(h,v))
#undef AMBER_WRAP2
}

extern "C" AMBER_EXPORT AmberResult AMBER_CALL AmberGetApi(uint32_t version,uint32_t size,void* api) {
    try {
        if (!api) return AMBER_INVALID_ARGUMENT;
        if (version==AMBER_API_VERSION_1) { if(size<sizeof(AmberApiV1)) return AMBER_BUFFER_TOO_SMALL; const AmberApiV1 value={sizeof(AmberApiV1),AMBER_API_VERSION_1,BridgeInfo,Enumerate,Create,Destroy,Initialise,Reset,Run,Shutdown,LastError}; std::memcpy(api,&value,sizeof(value)); return AMBER_OK; }
        if (version==AMBER_API_VERSION_2) { if(size<sizeof(AmberApiV2)) return AMBER_BUFFER_TOO_SMALL; AmberApiV2 value{}; value.struct_size=sizeof(value); value.api_version=AMBER_API_VERSION_2; value.GetBridgeInfo=BridgeInfo; value.EnumerateCore=Enumerate; value.Create=Create; value.Destroy=Destroy; value.Initialise=Initialise; value.Reset=Reset; value.Run=Run; value.Shutdown=Shutdown; value.GetLastError=LastError; value.GetCapabilities=GetCapabilities; value.SetSwitchState=SetSwitchState; value.GetOutputSnapshot=GetOutputSnapshot; value.GetAudioFormat=GetAudioFormat; value.FillAudioFrames=FillAudioFrames; value.ConfigureReels=ConfigureReels; value.ConfigureCoins=ConfigureCoins; value.SetPercentageSwitch=SetPercentageSwitch; std::memcpy(api,&value,sizeof(value)); return AMBER_OK; }
        return AMBER_UNSUPPORTED_VERSION;
    } catch (const std::exception& e) { SetExceptionError(nullptr,e.what()); }
      catch (...) { SetExceptionError(nullptr,nullptr); }
    return AMBER_INTERNAL_ERROR;
}
