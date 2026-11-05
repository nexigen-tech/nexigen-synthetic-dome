#include "nxd/runtime/chronos/let_boundary.hpp"

namespace nxd::runtime::chronos {

void LetDeadlineTracker::declare_boundary(const LetBoundary& boundary) noexcept {
    boundaries_.push_back(boundary);
}

std::optional<LetViolation>
LetDeadlineTracker::record_write(const std::string& source_instance,
                                  const std::string& source_port,
                                  const std::string& target_instance,
                                  const std::string& target_port,
                                  SimTime write_time) noexcept {
    const LetBoundary* const boundary =
        find_boundary(source_instance, source_port, target_instance, target_port);
    if (boundary == nullptr) {
        return std::nullopt;
    }

    if (write_time.ns > boundary->write_deadline.ns) {
        const LetViolation violation{
            .boundary_id = boundary->id,
            .write_time = write_time,
            .write_deadline = boundary->write_deadline,
            .lateness_ns = write_time.ns - boundary->write_deadline.ns,
        };
        violations_.push_back(violation);
        return violation;
    }

    return std::nullopt;
}

const std::vector<LetViolation>& LetDeadlineTracker::violations() const noexcept {
    return violations_;
}

void LetDeadlineTracker::reset() noexcept {
    boundaries_.clear();
    violations_.clear();
}

const LetBoundary* LetDeadlineTracker::find_boundary(const std::string& source_instance,
                                                      const std::string& source_port,
                                                      const std::string& target_instance,
                                                      const std::string& target_port) const noexcept {
    for (const auto& boundary : boundaries_) {
        if (boundary.source_instance == source_instance &&
            boundary.source_port == source_port &&
            boundary.target_instance == target_instance &&
            boundary.target_port == target_port) {
            return &boundary;
        }
    }
    return nullptr;
}

} // namespace nxd::runtime::chronos
