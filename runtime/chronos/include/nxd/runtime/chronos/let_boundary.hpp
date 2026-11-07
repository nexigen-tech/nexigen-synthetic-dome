/* NXD Chronos — Logical Execution Time deadline tracker.
 *
 * Records declared LET boundaries and detects late writes to cross-partition
 * ports.  Intra-partition writes are not tracked because they are not subject
 * to LET deadlines.
 */

#ifndef NXD_RUNTIME_CHRONOS_LET_BOUNDARY_HPP
#define NXD_RUNTIME_CHRONOS_LET_BOUNDARY_HPP

#include "nxd/runtime/chronos/time.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nxd::runtime::chronos {

/// Runtime view of a LET boundary.
struct LetBoundary final {
    std::string id;
    std::string source_instance;
    std::string source_port;
    std::string target_instance;
    std::string target_port;
    SimTime read_time{0};
    SimTime write_deadline{0};

    [[nodiscard]] bool operator==(const LetBoundary&) const noexcept = default;
};

/// Recorded LET deadline violation.
struct LetViolation final {
    std::string boundary_id;
    SimTime write_time{0};
    SimTime write_deadline{0};
    std::int64_t lateness_ns = 0;
};

/// Tracks LET boundaries and records late writes.
class LetDeadlineTracker final {
  public:
    LetDeadlineTracker() noexcept = default;

    /// Declares a LET boundary.  Must happen before arming.
    void declare_boundary(const LetBoundary& boundary) noexcept;

    /// Records a write to a cross-partition port at the current simulation
    /// time.  Returns a violation if the write missed its deadline.
    [[nodiscard]] std::optional<LetViolation>
    record_write(const std::string& source_instance,
                 const std::string& source_port,
                 const std::string& target_instance,
                 const std::string& target_port,
                 SimTime write_time) noexcept;

    /// Returns all recorded violations.
    [[nodiscard]] const std::vector<LetViolation>& violations() const noexcept;

    /// Clears all declarations and recorded violations.
    void reset() noexcept;
  private:
    std::vector<LetBoundary> boundaries_;
    std::vector<LetViolation> violations_;

    [[nodiscard]] const LetBoundary* find_boundary(const std::string& source_instance,
                                                   const std::string& source_port,
                                                   const std::string& target_instance,
                                                   const std::string& target_port) const noexcept;
};

} // namespace nxd::runtime::chronos

#endif // NXD_RUNTIME_CHRONOS_LET_BOUNDARY_HPP
