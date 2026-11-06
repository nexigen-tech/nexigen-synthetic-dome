#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

#define NXD_BLACKBOARD_OK 0
#define NXD_BLACKBOARD_ERROR_NOT_FOUND -1
#define NXD_BLACKBOARD_ERROR_LAYOUT_MISMATCH -2
#define NXD_BLACKBOARD_ERROR_EXISTS -3
#define NXD_BLACKBOARD_ERROR_NO_SPACE -4
#define NXD_BLACKBOARD_ERROR_INVALID_ARGUMENT -5

typedef struct NxdBlackboardEntry NxdBlackboardEntry;

typedef struct {
    char entry_name[64];
    std::uint8_t layout_hash[32];
    std::uint64_t revision;
    std::uint64_t apply_frame_no;
    std::uint64_t generation;
} NxdBlackboardUpdate;

/**
 * Initialize the global Blackboard instance.  Idempotent and safe to call from
 * any thread; actual initialization happens once before the first use.
 */
int nxd_blackboard_initialize(void);

/**
 * Create a single-writer Blackboard entry.  Returns nullptr if the entry already
 * exists, no slots are available, or the arguments are invalid.
 */
NxdBlackboardEntry* nxd_blackboard_create_entry(const char* name,
                                                std::uint64_t value_size,
                                                const std::uint8_t layout_hash[32]);

/**
 * Open an existing entry for reading.  Returns nullptr if the entry does not
 * exist or the layout hash does not match.
 */
const NxdBlackboardEntry* nxd_blackboard_open_entry(const char* name,
                                                    const std::uint8_t layout_hash[32]);

/**
 * Write a value to an entry.  The value size must match the entry size.  Returns
 * the new revision on success, or a negative error code on failure.
 */
std::int64_t nxd_blackboard_write(NxdBlackboardEntry* entry,
                                  const void* value,
                                  std::uint64_t value_size,
                                  std::uint64_t apply_frame_no);

/**
 * Read the latest value from an entry.  The caller must provide a buffer of the
 * entry's value size.  Returns NXD_BLACKBOARD_OK on success.
 */
int nxd_blackboard_read(const NxdBlackboardEntry* entry,
                        void* out_value,
                        std::uint64_t value_size,
                        std::uint64_t* out_revision,
                        std::uint64_t* out_apply_frame_no);

/**
 * Copy the last BlackboardUpdate event for the entry.  Returns NXD_BLACKBOARD_OK
 * on success.
 */
int nxd_blackboard_get_update(const NxdBlackboardEntry* entry,
                              NxdBlackboardUpdate* out_update);

#ifdef __cplusplus
} // extern "C"
#endif
