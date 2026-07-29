#include "FabricRuntimeInternal.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <new>
#include <utility>

struct FabricRuntime {
    fabric::RuntimeRegistry registry;
};

struct FabricMachineSession {
    explicit FabricMachineSession(std::unique_ptr<fabric::FabricBackendInstance> value)
        : instance(std::move(value)) {}
    ~FabricMachineSession() { if (initialised && !shutdown) instance->shutdown(); }

    std::unique_ptr<fabric::FabricBackendInstance> instance;
    std::mutex mutex;
    std::string boundary_error;
    bool initialised = false;
    bool shutdown = false;
};

namespace {
bool terminated(const char *value, size_t capacity) noexcept {
    return value && std::memchr(value, '\0', capacity) != nullptr;
}

template <typename T> bool valid(const T *value) noexcept {
    return value && value->struct_size >= sizeof(T) && value->struct_version == FABRIC_ABI_VERSION_1;
}

FabricResult remember(FabricMachineSession *session, FabricResult result) noexcept {
    if (result == FABRIC_OK) session->boundary_error.clear();
    else session->boundary_error = session->instance->last_error();
    return result;
}

template <typename Function>
FabricResult invoke(FabricMachineSession *session, Function function) noexcept {
    if (!session) return FABRIC_INVALID_ARGUMENT;
    try {
        std::lock_guard<std::mutex> lock(session->mutex);
        return remember(session, function());
    } catch (const std::exception &exception) {
        session->boundary_error = exception.what();
        return FABRIC_INTERNAL_ERROR;
    } catch (...) {
        session->boundary_error = "unknown exception at backend boundary";
        return FABRIC_INTERNAL_ERROR;
    }
}
} // namespace

namespace fabric {
FabricResult RuntimeRegistry::register_provider(std::unique_ptr<FabricBackendProvider> provider) noexcept {
    if (!provider) return FABRIC_INVALID_ARGUMENT;
    try { providers_.push_back(std::move(provider)); }
    catch (...) { return FABRIC_INTERNAL_ERROR; }
    return FABRIC_OK;
}

FabricResult RuntimeRegistry::create(const FabricLaunchRequest &request,
                                     std::unique_ptr<FabricBackendInstance> &instance,
                                     std::string &error) const noexcept {
    try {
        for (const auto &provider : providers_) {
            if (provider->supports(request.backend_kind, request.machine_identifier))
                return provider->create(request, instance, error);
        }
        error = "no backend provider supports the requested backend and machine";
        return FABRIC_NOT_FOUND;
    } catch (...) {
        error = "backend provider failed unexpectedly";
        return FABRIC_INTERNAL_ERROR;
    }
}
} // namespace fabric

FabricResult FabricRegisterBackendProvider(FabricRuntime *runtime,
    std::unique_ptr<fabric::FabricBackendProvider> provider) noexcept {
    return runtime ? runtime->registry.register_provider(std::move(provider)) : FABRIC_INVALID_ARGUMENT;
}

extern "C" {
FabricResult FabricCreateRuntime(uint32_t requested_version, FabricRuntime **out_runtime) {
    if (!out_runtime) return FABRIC_INVALID_ARGUMENT;
    *out_runtime = nullptr;
    if (requested_version != FABRIC_ABI_VERSION_1) return FABRIC_UNSUPPORTED_VERSION;
    try { *out_runtime = new FabricRuntime(); return FABRIC_OK; }
    catch (...) { return FABRIC_INTERNAL_ERROR; }
}

void FabricDestroyRuntime(FabricRuntime *runtime) { delete runtime; }

FabricResult FabricCreateSession(FabricRuntime *runtime, const FabricLaunchRequest *request,
                                 FabricMachineSession **out_session) {
    if (!runtime || !out_session || !valid(request)) return FABRIC_INVALID_ARGUMENT;
    *out_session = nullptr;
    if (!terminated(request->backend_kind, sizeof(request->backend_kind)) ||
        !terminated(request->machine_identifier, sizeof(request->machine_identifier)) ||
        !terminated(request->backend_path, sizeof(request->backend_path)) ||
        (request->rom_path_count && !request->rom_paths) ||
        (request->machine_configuration_size && !request->machine_configuration)) return FABRIC_INVALID_ARGUMENT;
    try {
        std::unique_ptr<fabric::FabricBackendInstance> instance;
        std::string error;
        FabricResult result = runtime->registry.create(*request, instance, error);
        if (result != FABRIC_OK) return result;
        if (!instance) return FABRIC_INTERNAL_ERROR;
        *out_session = new FabricMachineSession(std::move(instance));
        return FABRIC_OK;
    } catch (...) { return FABRIC_INTERNAL_ERROR; }
}

void FabricDestroySession(FabricMachineSession *session) { delete session; }

FabricResult FabricSessionInitialise(FabricMachineSession *session) {
    return invoke(session, [session] {
        if (session->initialised || session->shutdown) return FABRIC_INVALID_STATE;
        FabricResult result = session->instance->initialise();
        if (result == FABRIC_OK) session->initialised = true;
        return result;
    });
}
FabricResult FabricSessionReset(FabricMachineSession *session) {
    return invoke(session, [session] { return session->initialised && !session->shutdown
        ? session->instance->reset() : FABRIC_INVALID_STATE; });
}
FabricResult FabricSessionAdvance(FabricMachineSession *session, uint64_t elapsed_nanoseconds) {
    return invoke(session, [session, elapsed_nanoseconds] { return session->initialised && !session->shutdown
        ? session->instance->advance(elapsed_nanoseconds) : FABRIC_INVALID_STATE; });
}
FabricResult FabricSessionShutdown(FabricMachineSession *session) {
    return invoke(session, [session] {
        if (!session->initialised || session->shutdown) return FABRIC_INVALID_STATE;
        FabricResult result = session->instance->shutdown();
        if (result == FABRIC_OK) session->shutdown = true;
        return result;
    });
}
FabricResult FabricSessionSubmitInput(FabricMachineSession *session, const FabricInput *input) {
    if (!valid(input) || !terminated(input->identifier, sizeof(input->identifier))) return FABRIC_INVALID_ARGUMENT;
    return invoke(session, [session, input] { return session->initialised && !session->shutdown
        ? session->instance->submit_input(*input) : FABRIC_INVALID_STATE; });
}
FabricResult FabricSessionGetCapabilities(FabricMachineSession *session, FabricCapabilities *capabilities) {
    if (!valid(capabilities)) return FABRIC_INVALID_ARGUMENT;
    return invoke(session, [session, capabilities] { return session->instance->capabilities(*capabilities); });
}
FabricResult FabricSessionGetSnapshot(FabricMachineSession *session, FabricMachineSnapshot *snapshot) {
    if (!valid(snapshot)) return FABRIC_INVALID_ARGUMENT;
    return invoke(session, [session, snapshot] { return session->initialised && !session->shutdown
        ? session->instance->snapshot(*snapshot) : FABRIC_INVALID_STATE; });
}
FabricResult FabricSessionGetAudioFormat(FabricMachineSession *session, FabricAudioFormat *format) {
    if (!valid(format)) return FABRIC_INVALID_ARGUMENT;
    return invoke(session, [session, format] { return session->instance->audio_format(*format); });
}
FabricResult FabricSessionReadAudio(FabricMachineSession *session, int16_t *samples,
                                    uint32_t capacity, uint32_t *written) {
    if (!written || (capacity && !samples)) return FABRIC_INVALID_ARGUMENT;
    *written = 0;
    return invoke(session, [session, samples, capacity, written] { return session->initialised && !session->shutdown
        ? session->instance->read_audio(samples, capacity, *written) : FABRIC_INVALID_STATE; });
}
FabricResult FabricSessionGetLastError(FabricMachineSession *session, char *buffer,
                                       uint32_t buffer_size, uint32_t *required_size) {
    if (!session || !required_size) return FABRIC_INVALID_ARGUMENT;
    try {
        std::lock_guard<std::mutex> lock(session->mutex);
        const std::string message = session->boundary_error.empty() ? session->instance->last_error() : session->boundary_error;
        *required_size = static_cast<uint32_t>(message.size() + 1);
        if (!buffer || buffer_size < *required_size) return FABRIC_BUFFER_TOO_SMALL;
        std::memcpy(buffer, message.c_str(), *required_size);
        return FABRIC_OK;
    } catch (...) { return FABRIC_INTERNAL_ERROR; }
}
} // extern "C"
