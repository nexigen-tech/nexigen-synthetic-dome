/* Implementation of CAN monitor output helpers using the generated contracts. */

#include "can_monitor_outputs.hpp"

#include "nxd/contracts/CanAggregateErrors.hpp"
#include "nxd/contracts/CanHeartbeat.hpp"

#include <cstring>

namespace can_monitor {

using nxd::contracts::CanAggregateErrors;
using nxd::contracts::CanHeartbeat;
using nxd::contracts::Direction;
using nxd::contracts::Kind;
using nxd::contracts::Status;

void fill_heartbeat(void* data,
                    std::int64_t hw_time_ns,
                    std::uint32_t can_id,
                    std::uint8_t direction,
                    std::uint32_t heartbeat_counter,
                    std::uint8_t status) noexcept {
    auto* heartbeat = static_cast<CanHeartbeat*>(data);
    std::memset(heartbeat, 0, sizeof(CanHeartbeat));
    heartbeat->hw_time_ns = hw_time_ns;
    heartbeat->can_id = can_id;
    heartbeat->direction = static_cast<Direction>(direction);
    heartbeat->kind = Kind::Heartbeat;
    heartbeat->heartbeat_counter = heartbeat_counter;
    heartbeat->status = status;
}

void fill_aggregate(void* data,
                    std::int64_t detection_time_ns,
                    std::int64_t heartbeat_time_ns,
                    std::uint32_t rx_valid,
                    std::uint32_t rx_invalid,
                    std::uint32_t self_rx_count,
                    std::uint32_t activity_filter,
                    std::uint8_t status) noexcept {
    auto* aggregate = static_cast<CanAggregateErrors*>(data);
    std::memset(aggregate, 0, sizeof(CanAggregateErrors));
    aggregate->detection_time_ns = detection_time_ns;
    aggregate->heartbeat_time_ns = heartbeat_time_ns;
    aggregate->rx_valid = rx_valid;
    aggregate->rx_invalid = rx_invalid;
    aggregate->self_rx_count = self_rx_count;
    aggregate->activity_filter = activity_filter;
    aggregate->status = static_cast<Status>(status);
}

} // namespace can_monitor
