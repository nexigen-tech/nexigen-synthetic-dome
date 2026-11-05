/* Helper declarations for writing CanHeartbeat and CanAggregateErrors output
 * buffers from the CAN monitor plugin.
 *
 * The implementations live in a separate translation unit so that the
 * RawCanBatchClassical header (which defines Direction/Kind) and the
 * heartbeat/aggregate headers (which define the same enum names) are not
 * included together.
 */

#pragma once

#include <cstdint>

namespace can_monitor {

void fill_heartbeat(void* data,
                    std::int64_t hw_time_ns,
                    std::uint32_t can_id,
                    std::uint8_t direction,
                    std::uint32_t heartbeat_counter,
                    std::uint8_t status) noexcept;

void fill_aggregate(void* data,
                    std::int64_t detection_time_ns,
                    std::int64_t heartbeat_time_ns,
                    std::uint32_t rx_valid,
                    std::uint32_t rx_invalid,
                    std::uint32_t self_rx_count,
                    std::uint32_t activity_filter,
                    std::uint8_t status) noexcept;

} // namespace can_monitor
