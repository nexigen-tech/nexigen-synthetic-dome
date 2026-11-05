#include "nxd/abi.hpp"
#include <cstdint>
#include <cstddef>
#include <type_traits>

static_assert(sizeof(NxdStatus) == 4, "NxdStatus size");
static_assert(NXD_OK == 0, "NXD_OK");
static_assert(NXD_ERROR_GENERAL == 1, "NXD_ERROR_GENERAL");
static_assert(NXD_ERROR_INVALID_ARGUMENT == 2, "NXD_ERROR_INVALID_ARGUMENT");
static_assert(NXD_ERROR_NOT_SUPPORTED == 3, "NXD_ERROR_NOT_SUPPORTED");
static_assert(NXD_ERROR_ALREADY_INITIALIZED == 4, "NXD_ERROR_ALREADY_INITIALIZED");
static_assert(NXD_ERROR_NOT_INITIALIZED == 5, "NXD_ERROR_NOT_INITIALIZED");
static_assert(NXD_ERROR_OUT_OF_MEMORY == 6, "NXD_ERROR_OUT_OF_MEMORY");
static_assert(NXD_ERROR_ABI_MISMATCH == 7, "NXD_ERROR_ABI_MISMATCH");

static_assert(sizeof(NxdAbiVersion) == 12, "NxdAbiVersion size");
static_assert(alignof(NxdAbiVersion) == 4, "NxdAbiVersion alignment");
static_assert(sizeof(NxdAbiFingerprint) == 32, "NxdAbiFingerprint size");
static_assert(alignof(NxdAbiFingerprint) == 1, "NxdAbiFingerprint alignment");

static_assert(sizeof(NxdFrameContext) == 48, "NxdFrameContext size");
static_assert(alignof(NxdFrameContext) == 8, "NxdFrameContext alignment");
static_assert(offsetof(NxdFrameContext, frame_no) == 0, "frame_no offset");
static_assert(offsetof(NxdFrameContext, sim_time_ns) == 8, "sim_time_ns offset");
static_assert(offsetof(NxdFrameContext, hw_time_ns) == 16, "hw_time_ns offset");
static_assert(offsetof(NxdFrameContext, seed_version) == 24, "seed_version offset");
static_assert(offsetof(NxdFrameContext, rng) == 32, "rng offset");
static_assert(offsetof(NxdFrameContext, rng_state) == 40, "rng_state offset");

static_assert(sizeof(NxdAbiTable) == 256, "NxdAbiTable size");
static_assert(alignof(NxdAbiTable) == alignof(void*), "NxdAbiTable alignment");
static_assert(std::is_trivially_copyable_v<NxdAbiTable>, "NxdAbiTable trivially copyable");

static_assert(sizeof(NxdCreateFn) == sizeof(void*), "NxdCreateFn size");
static_assert(sizeof(NxdDestroyFn) == sizeof(void*), "NxdDestroyFn size");
static_assert(sizeof(NxdInitializeFn) == sizeof(void*), "NxdInitializeFn size");
static_assert(sizeof(NxdArmFn) == sizeof(void*), "NxdArmFn size");
static_assert(sizeof(NxdStepFn) == sizeof(void*), "NxdStepFn size");
static_assert(sizeof(NxdStopFn) == sizeof(void*), "NxdStopFn size");
static_assert(sizeof(NxdAbiEntryFn) == sizeof(void*), "NxdAbiEntryFn size");

// Verify that the entry symbol type matches the declared entry function-pointer
// type.  A real plugin implements exactly this signature.
extern "C" const NxdAbiTable* nxd_abi_entry();
static_assert(std::is_same_v<decltype(&nxd_abi_entry), NxdAbiEntryFn>, "entry signature mismatch");

int main() { return 0; }
