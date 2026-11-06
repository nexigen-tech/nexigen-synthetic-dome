#pragma once

#include "nxd/runtime/recording/mcap.hpp"
#include "nxd/runtime/transport/transport.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nxd::runtime::recording {

/// Records transport samples and Blackboard updates to a single MCAP file.
class Recorder {
  public:
    Recorder(transport::Transport& transport, std::string_view output_path) noexcept;
    ~Recorder();

    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;

    /// Subscribe to a transport topic and declare its schema for MCAP.
    [[nodiscard]] bool register_topic(std::string_view source_name,
                                       std::size_t element_size,
                                       std::size_t max_count,
                                       const std::array<std::uint8_t, 32>& layout_hash,
                                       std::string_view schema_name) noexcept;

    [[nodiscard]] bool start() noexcept;

    /// Poll all subscribers and write any new samples.
    void poll() noexcept;

    /// Record the current Blackboard state as the reserved `_blackboard` channel.
    void record_blackboard() noexcept;

    /// Finalize the recording and write the RecordingMetadata attachment.
    [[nodiscard]] bool stop() noexcept;

    [[nodiscard]] bool is_active() const noexcept;

  private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace nxd::runtime::recording
