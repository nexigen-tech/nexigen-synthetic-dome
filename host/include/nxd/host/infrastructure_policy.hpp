#pragma once

#include <nexigen/app/infrastructure.hpp>
#include <nexigen/chrono/console_chrono_sink.hpp>
#include <nexigen/logging/console_logger.hpp>
#include <nexigen/metrics/console_metric_sink.hpp>
#include <nexigen/tracing/console_trace_sink.hpp>

#include <nexigen/core/result.hpp>

namespace nxd::host {

    /// Minimal production-like Infrastructure policy for `nxd-host`.
    ///
    /// All four subsystems use console sinks so the host can run headless without
    /// file configuration.  The policy itself performs no allocation on the hot
    /// path; `Infrastructure` allocates its providers during initialization.
    struct NxdInfrastructurePolicy {
        using Logger = nexigen::logging::ConsoleLogger<>;
        using MetricsSink = nexigen::metrics::ConsoleMetricSink;
        using TraceSink = nexigen::tracing::ConsoleTraceSink;
        using ChronoSink = nexigen::chrono::ConsoleChronoSink;

        static nexigen::core::Status emplace_logger(std::optional<Logger>& out,
                                                    const nexigen::logging::Settings& settings,
                                                    std::string_view) noexcept {
            out.emplace();
            return out->initialize(settings);
        }

        static nexigen::core::Status emplace_metrics_sink(std::optional<MetricsSink>& out,
                                                          const nexigen::metrics::Settings&,
                                                          std::string_view) noexcept {
            out.emplace();
            return {};
        }

        static nexigen::core::Status emplace_trace_sink(std::optional<TraceSink>& out,
                                                        const nexigen::tracing::Settings&,
                                                        std::string_view) noexcept {
            out.emplace();
            return {};
        }

        static nexigen::core::Status
        emplace_chrono_sink(std::optional<ChronoSink>& out,
                            const nexigen::chrono::Settings&) noexcept {
            out.emplace();
            return {};
        }
    };

    using NxdInfrastructure = nexigen::app::Infrastructure<NxdInfrastructurePolicy>;

} // namespace nxd::host
