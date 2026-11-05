/* NXD Chronos — versioned C state-control API.
 *
 * This header declares the C ABI table implemented by the Chronos runtime.
 * It is designed to be consumed from C, C++, and Rust via identical layout.
 */

#ifndef NXD_RUNTIME_CHRONOS_C_API_HPP
#define NXD_RUNTIME_CHRONOS_C_API_HPP

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/// Major version of the Chronos C API.
#define NXD_CHRONOS_API_MAJOR 1
/// Minor version of the Chronos C API.
#define NXD_CHRONOS_API_MINOR 0
/// Patch version of the Chronos C API.
#define NXD_CHRONOS_API_PATCH 0

/// Numeric status returned by every state-control function.
typedef std::uint32_t NxdChronosStatus;

/// Operation completed successfully.
static constexpr NxdChronosStatus NXD_CHRONOS_OK = 0;
/// Invalid argument or configuration.
static constexpr NxdChronosStatus NXD_CHRONOS_ERROR_INVALID_ARGUMENT = 2;
/// Invalid state transition.
static constexpr NxdChronosStatus NXD_CHRONOS_ERROR_INVALID_STATE = 4;

/// Execution mode selected before arming.
typedef std::uint8_t NxdChronosMode;
static constexpr NxdChronosMode NXD_CHRONOS_MODE_REALTIME_HIL = 0;
static constexpr NxdChronosMode NXD_CHRONOS_MODE_SCALED_SIL = 1;
static constexpr NxdChronosMode NXD_CHRONOS_MODE_FREE_SIL = 2;
static constexpr NxdChronosMode NXD_CHRONOS_MODE_STEP = 3;

/// Lifecycle state of a Chronos instance.
typedef std::uint8_t NxdChronosState;
static constexpr NxdChronosState NXD_CHRONOS_STATE_UNINITIALIZED = 0;
static constexpr NxdChronosState NXD_CHRONOS_STATE_INITIALIZED = 1;
static constexpr NxdChronosState NXD_CHRONOS_STATE_ARMED = 2;
static constexpr NxdChronosState NXD_CHRONOS_STATE_PLAYING = 3;
static constexpr NxdChronosState NXD_CHRONOS_STATE_PAUSED = 4;
static constexpr NxdChronosState NXD_CHRONOS_STATE_STEPPING = 5;
static constexpr NxdChronosState NXD_CHRONOS_STATE_STOPPED = 6;
static constexpr NxdChronosState NXD_CHRONOS_STATE_DISPOSED = 7;

/// Runtime configuration validated during initialization.
struct NxdChronosConfig {
    NxdChronosMode mode;
    std::int64_t base_frame_interval_ns;
    std::uint32_t scale_factor;
};

/// Opaque handle to a Chronos runtime instance.
struct NxdChronosInstance;

struct NxdChronosApiTable;

typedef NxdChronosInstance* (*NxdChronosCreateFn)(const NxdChronosConfig* config);
typedef void (*NxdChronosDestroyFn)(NxdChronosInstance* instance);
typedef NxdChronosStatus (*NxdChronosInitializeFn)(NxdChronosInstance* instance);
typedef NxdChronosStatus (*NxdChronosArmFn)(NxdChronosInstance* instance);
typedef NxdChronosStatus (*NxdChronosPlayFn)(NxdChronosInstance* instance);
typedef NxdChronosStatus (*NxdChronosPauseFn)(NxdChronosInstance* instance);
typedef NxdChronosStatus (*NxdChronosStepFn)(NxdChronosInstance* instance);
typedef NxdChronosStatus (*NxdChronosStopFn)(NxdChronosInstance* instance);
typedef NxdChronosStatus (*NxdChronosDisposeFn)(NxdChronosInstance* instance);
typedef NxdChronosState (*NxdChronosGetStateFn)(const NxdChronosInstance* instance);
typedef std::uint64_t (*NxdChronosGetFrameNoFn)(const NxdChronosInstance* instance);
typedef std::int64_t (*NxdChronosGetSimTimeNsFn)(const NxdChronosInstance* instance);

/// Versioned table exported by the Chronos runtime.
struct NxdChronosApiTable {
    std::uint32_t version_major;
    std::uint32_t version_minor;
    std::uint32_t version_patch;
    std::size_t table_size;
    NxdChronosCreateFn create;
    NxdChronosDestroyFn destroy;
    NxdChronosInitializeFn initialize;
    NxdChronosArmFn arm;
    NxdChronosPlayFn play;
    NxdChronosPauseFn pause;
    NxdChronosStepFn step;
    NxdChronosStopFn stop;
    NxdChronosDisposeFn dispose;
    NxdChronosGetStateFn get_state;
    NxdChronosGetFrameNoFn get_frame_no;
    NxdChronosGetSimTimeNsFn get_sim_time_ns;
};

/// Entry symbol exported by the Chronos runtime library.
#ifdef __cplusplus
#define NXD_CHRONOS_API_ENTRY extern "C" __attribute__((visibility("default"))) const NxdChronosApiTable* nxd_chronos_api()
#else
#define NXD_CHRONOS_API_ENTRY __attribute__((visibility("default"))) const struct NxdChronosApiTable* nxd_chronos_api()
#endif

/// Declares the entry symbol for callers that include this header.
NXD_CHRONOS_API_ENTRY;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NXD_RUNTIME_CHRONOS_C_API_HPP
