#include "nxd/runtime/chronos/c_api.hpp"
#include "nxd/runtime/chronos/state_machine.hpp"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <new>

namespace chronos = nxd::runtime::chronos;

namespace {

chronos::Config to_cpp_config(const NxdChronosConfig* config) noexcept {
    chronos::Config cfg{};
    switch (config->mode) {
        case NXD_CHRONOS_MODE_REALTIME_HIL:
            cfg.mode = chronos::Mode::RealTimeHil;
            break;
        case NXD_CHRONOS_MODE_SCALED_SIL:
            cfg.mode = chronos::Mode::ScaledSil;
            break;
        case NXD_CHRONOS_MODE_FREE_SIL:
            cfg.mode = chronos::Mode::FreeSil;
            break;
        default:
            cfg.mode = chronos::Mode::Step;
            break;
    }
    cfg.base_frame_interval_ns = config->base_frame_interval_ns;
    cfg.scale_factor = config->scale_factor;
    return cfg;
}

NxdChronosState to_c_state(chronos::State state) noexcept {
    switch (state) {
        case chronos::State::Uninitialized:
            return NXD_CHRONOS_STATE_UNINITIALIZED;
        case chronos::State::Initialized:
            return NXD_CHRONOS_STATE_INITIALIZED;
        case chronos::State::Armed:
            return NXD_CHRONOS_STATE_ARMED;
        case chronos::State::Playing:
            return NXD_CHRONOS_STATE_PLAYING;
        case chronos::State::Paused:
            return NXD_CHRONOS_STATE_PAUSED;
        case chronos::State::Stepping:
            return NXD_CHRONOS_STATE_STEPPING;
        case chronos::State::Stopped:
            return NXD_CHRONOS_STATE_STOPPED;
        case chronos::State::Disposed:
            return NXD_CHRONOS_STATE_DISPOSED;
    }
    return NXD_CHRONOS_STATE_UNINITIALIZED;
}

NxdChronosStatus to_c_status(const nexigen::core::Status& status) noexcept {
    if (status.has_value()) {
        return NXD_CHRONOS_OK;
    }
    switch (status.error()) {
        case nexigen::core::ErrorCode::InvalidArgument:
            return NXD_CHRONOS_ERROR_INVALID_ARGUMENT;
        case nexigen::core::ErrorCode::Conflict:
            return NXD_CHRONOS_ERROR_INVALID_STATE;
        default:
            return NXD_CHRONOS_ERROR_INVALID_ARGUMENT;
    }
}

} // namespace

extern "C" {

namespace {
NxdChronosInstance* chronos_create(const NxdChronosConfig* config) {
    if (config == nullptr) {
        return nullptr;
    }
    const chronos::Config cfg = to_cpp_config(config);
    void* const mem = std::malloc(sizeof(chronos::Runtime));
    if (mem == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<NxdChronosInstance*>(new (mem) chronos::Runtime(cfg));
}

void chronos_destroy(NxdChronosInstance* instance) {
    if (instance == nullptr) {
        return;
    }
    auto* rt = reinterpret_cast<chronos::Runtime*>(instance);
    rt->~Runtime();
    std::free(rt);
}

NxdChronosStatus chronos_initialize(NxdChronosInstance* instance) {
    if (instance == nullptr) {
        return NXD_CHRONOS_ERROR_INVALID_ARGUMENT;
    }
    return to_c_status(reinterpret_cast<chronos::Runtime*>(instance)->initialize());
}

NxdChronosStatus chronos_arm(NxdChronosInstance* instance) {
    if (instance == nullptr) {
        return NXD_CHRONOS_ERROR_INVALID_ARGUMENT;
    }
    return to_c_status(reinterpret_cast<chronos::Runtime*>(instance)->arm());
}

NxdChronosStatus chronos_play(NxdChronosInstance* instance) {
    if (instance == nullptr) {
        return NXD_CHRONOS_ERROR_INVALID_ARGUMENT;
    }
    return to_c_status(reinterpret_cast<chronos::Runtime*>(instance)->play());
}

NxdChronosStatus chronos_pause(NxdChronosInstance* instance) {
    if (instance == nullptr) {
        return NXD_CHRONOS_ERROR_INVALID_ARGUMENT;
    }
    return to_c_status(reinterpret_cast<chronos::Runtime*>(instance)->pause());
}

NxdChronosStatus chronos_step(NxdChronosInstance* instance) {
    if (instance == nullptr) {
        return NXD_CHRONOS_ERROR_INVALID_ARGUMENT;
    }
    return to_c_status(reinterpret_cast<chronos::Runtime*>(instance)->step());
}

NxdChronosStatus chronos_stop(NxdChronosInstance* instance) {
    if (instance == nullptr) {
        return NXD_CHRONOS_ERROR_INVALID_ARGUMENT;
    }
    return to_c_status(reinterpret_cast<chronos::Runtime*>(instance)->stop());
}

NxdChronosStatus chronos_dispose(NxdChronosInstance* instance) {
    if (instance == nullptr) {
        return NXD_CHRONOS_ERROR_INVALID_ARGUMENT;
    }
    return to_c_status(reinterpret_cast<chronos::Runtime*>(instance)->dispose());
}

NxdChronosState chronos_get_state(const NxdChronosInstance* instance) {
    if (instance == nullptr) {
        return NXD_CHRONOS_STATE_UNINITIALIZED;
    }
    return to_c_state(reinterpret_cast<const chronos::Runtime*>(instance)->state());
}

std::uint64_t chronos_get_frame_no(const NxdChronosInstance* instance) {
    if (instance == nullptr) {
        return 0;
    }
    return reinterpret_cast<const chronos::Runtime*>(instance)->frame_no();
}

std::int64_t chronos_get_sim_time_ns(const NxdChronosInstance* instance) {
    if (instance == nullptr) {
        return 0;
    }
    return reinterpret_cast<const chronos::Runtime*>(instance)->sim_time().ns;
}

const NxdChronosApiTable* chronos_api_table() noexcept {
    static const NxdChronosApiTable table = {
        .version_major = NXD_CHRONOS_API_MAJOR,
        .version_minor = NXD_CHRONOS_API_MINOR,
        .version_patch = NXD_CHRONOS_API_PATCH,
        .table_size = sizeof(NxdChronosApiTable),
        .create = chronos_create,
        .destroy = chronos_destroy,
        .initialize = chronos_initialize,
        .arm = chronos_arm,
        .play = chronos_play,
        .pause = chronos_pause,
        .step = chronos_step,
        .stop = chronos_stop,
        .dispose = chronos_dispose,
        .get_state = chronos_get_state,
        .get_frame_no = chronos_get_frame_no,
        .get_sim_time_ns = chronos_get_sim_time_ns,
    };
    return &table;
}

} // namespace

NXD_CHRONOS_API_ENTRY {
    return chronos_api_table();
}

} // extern "C"
