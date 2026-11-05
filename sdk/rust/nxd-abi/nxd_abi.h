#include <stdint.h>

/* ABI-1 target fingerprint: "NXD_ABI_1_le64_x86_v01" padded with NULs. */
static const uint8_t NXD_ABI_1_TARGET[32] = {
    0x4e, 0x58, 0x44, 0x5f, 0x41, 0x42, 0x49, 0x5f,
    0x31, 0x5f, 0x6c, 0x65, 0x36, 0x34, 0x5f, 0x78,
    0x38, 0x36, 0x5f, 0x76, 0x30, 0x31, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


#ifndef NXD_ABI_H
#define NXD_ABI_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


/**
 * Numeric status code returned by every lifecycle function.
 */
typedef uint32_t NxdStatus;

/**
 * Version of the plugin ABI table itself.
 */
typedef struct {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} NxdAbiVersion;
/**
 * Current ABI table version (minor bumped for typed port I/O).
 */
#define NxdAbiVersion_ABI_1 (NxdAbiVersion){ .major = 1, .minor = 1, .patch = 0 }

/**
 * 32-byte ABI fingerprint used for target/layout validation.
 */
typedef struct {
    uint8_t value[32];
} NxdAbiFingerprint;

/**
 * Frame context passed to `arm` and `step`.
 *
 * The host owns this value and all buffers reachable through it.  It is
 * valid only for the duration of the callback.  The component may read the
 * scalar fields and invoke the `rng` callback but must not retain pointers
 * beyond the callback return.
 */
typedef struct {
    uint64_t frame_no;
    int64_t sim_time_ns;
    int64_t hw_time_ns;
    uint32_t seed_version;
    /**
     * Deterministic RNG callback.  May be null if the host did not arm RNG.
     */
    uint64_t (*rng)(void *state);
    /**
     * Opaque RNG state pointer passed as the first argument to `rng`.
     */
    void *rng_state;
} NxdFrameContext;

/**
 * Opaque telemetry handle owned by the host.
 */
typedef struct {
    uint8_t _private;
} NxdTelemetryHandle;

/**
 * Opaque component state handle created by the plugin and owned by the host.
 */
typedef struct {
    uint8_t _private;
} NxdComponentState;

/**
 * Creates the plugin's private state.  Returns null on failure.
 */
typedef NxdComponentState *(*NxdCreateFn)(void);

/**
 * Destroys state previously created by `create`.
 */
typedef void (*NxdDestroyFn)(NxdComponentState *state);

/**
 * Initializes the component from a UTF-8 configuration blob.
 */
typedef NxdStatus (*NxdInitializeFn)(NxdComponentState *state,
                                     const char *config,
                                     uintptr_t config_len);

/**
 * Arms the component before the first frame.
 */
typedef NxdStatus (*NxdArmFn)(NxdComponentState *state, const NxdFrameContext *ctx);

/**
 * Executes one frame.
 */
typedef NxdStatus (*NxdStepFn)(NxdComponentState *state, const NxdFrameContext *ctx);

/**
 * Stops the component and releases frame-only resources.
 */
typedef void (*NxdStopFn)(NxdComponentState *state);

/**
 * Slot describing one typed input or output port buffer passed to `step_io`.
 */
typedef struct {
    /**
     * FNV-1a hash of the port name.  The component matches this against the
     * hashes of the ports it declared in its descriptor.
     */
    uint64_t port_name_hash;
    /**
     * Pointer to the first byte of the typed buffer.
     */
    void *data;
    /**
     * Number of elements of the port's contract type in the buffer.
     */
    uintptr_t count;
    /**
     * Size of one element in bytes.
     */
    uintptr_t element_size;
    /**
     * Layout hash of the contract type (32 bytes).
     */
    NxdAbiFingerprint layout_hash;
} NxdPortSlot;

/**
 * Bundle of port slots passed to the optional `step_io` callback.
 */
typedef struct {
    /**
     * Number of slots in the bundle.
     */
    uintptr_t count;
    /**
     * Pointer to the array of `count` slots.
     */
    NxdPortSlot *slots;
} NxdPortIoBundle;

/**
 * Optional extended step callback that receives typed input/output port
 * buffers.  When null, the host falls back to the legacy `step` callback.
 */
typedef NxdStatus (*NxdStepIoFn)(NxdComponentState *state,
                                 const NxdFrameContext *ctx,
                                 const NxdPortIoBundle *inputs,
                                 NxdPortIoBundle *outputs);

/**
 * ABI-1 plugin table.  Each shared library exports exactly one symbol that
 * returns a pointer to this table.
 *
 * The table size field lets the host reject plugins built against an
 * incompatible table layout even when the major version matches.  All
 * function pointers must be non-null for a valid ABI-1 plugin; `extension`
 * must be null for ABI-1.
 */
typedef struct {
    NxdAbiVersion abi_version;
    uintptr_t table_size;
    NxdAbiFingerprint fingerprint;
    NxdAbiFingerprint sdk_layout_hash;
    /**
     * Null-terminated component name, truncated to 63 bytes plus NUL.
     */
    char component_name[64];
    NxdAbiVersion component_version;
    NxdAbiFingerprint descriptor_hash;
    NxdCreateFn create;
    NxdDestroyFn destroy;
    NxdInitializeFn initialize;
    NxdArmFn arm;
    NxdStepFn step;
    NxdStopFn stop;
    /**
     * Optional extended step callback for port-aware components.  `None` for
     * legacy port-less plugins; when `Some` the host invokes it for port I/O.
     * `Option<fn>` has the same layout as a nullable function pointer in C.
     */
    NxdStepIoFn step_io;
} NxdAbiTable;

/**
 * Entry symbol exported by every ABI-1 plugin shared library.
 *
 * The returned pointer must remain valid for the lifetime of the shared
 * library mapping.  The table itself is read-only from the host side.
 */
typedef const NxdAbiTable *(*NxdAbiEntryFn)(void);



/**
 * Operation completed successfully.
 */
#define NXD_OK 0

/**
 * Unspecified error.
 */
#define NXD_ERROR_GENERAL 1

/**
 * Invalid argument passed to a lifecycle function.
 */
#define NXD_ERROR_INVALID_ARGUMENT 2

/**
 * Operation or capability not supported by the plugin.
 */
#define NXD_ERROR_NOT_SUPPORTED 3

/**
 * Component was already initialized.
 */
#define NXD_ERROR_ALREADY_INITIALIZED 4

/**
 * Component was not initialized.
 */
#define NXD_ERROR_NOT_INITIALIZED 5

/**
 * Allocation failed (cold path only).
 */
#define NXD_ERROR_OUT_OF_MEMORY 6

/**
 * ABI, fingerprint, or descriptor mismatch detected by host or plugin.
 */
#define NXD_ERROR_ABI_MISMATCH 7

#endif  /* NXD_ABI_H */
