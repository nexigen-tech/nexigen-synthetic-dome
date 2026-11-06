# NexDome

[![CI](https://github.com/nexigen-tech/nexigen-synthetic-dome/actions/workflows/ci.yml/badge.svg)](https://github.com/nexigen-tech/nexigen-synthetic-dome/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

> **"The Holodeck for your Hardware."**

**NexDome** is a high-fidelity **Synthetic Environment** and **Hardware-in-the-Loop (HIL)** platform designed to immerse Units Under Test (UUT) in a physically accurate, strictly deterministic digital reality.

Engineered in **Modern C++23**, NexDome leverages `std::span`, modules, and the **iceoryx2** zero-copy transport to simulate heterogeneous data streams (Vetronics, Avionics, Automotive, Vision) with sub-microsecond latency.

## 🚀 Key Features

- **Bleeding Edge C++23:** Utilizes `std::expected` for exception-free error handling and `std::mdspan` for multidimensional sensor data processing.
- **Zero-Copy Data Plane:** Transfers gigabytes of video/lidar data via shared memory using safe span views, eliminating serialization overhead.
- **Multi-Domain Fusion:** Synchronizes **MIL-STD-1553** transactions, **EBR-1553**, **CAN**, **CAN-FD** **CAN XLD**, and **RTP Video** under a single global simulation tick.
- **Deterministic:** Designed for Hard Real-Time execution on PREEMPT_RT Linux.

## 🏗 C++23 Architecture

NexDome uses a Service-Oriented Architecture (SOA) where services exchange strict POD contracts via shared memory.

### Code Example (C++23 Style)

We use `std::expected` for robust RT-safe APIs and `std::span` for memory safety:

```cpp
import std; // C++23 Modules support

namespace nxd::avionics {

    // Strictly aligned POD for Shared Memory
    struct alignas(8) MilStd1553Word {
        uint64_t timestamp_ns;
        uint16_t value;
        uint8_t  sync_type; 
    };

    // Zero-copy publisher interface
    class BusController {
    public:
        // C++23: No exceptions, returns payload reference or error enum
        auto publish_command(uint16_t cmd) -> std::expected<void, BusError> {
            // Acquiring zero-copy loan from iceoryx2
            auto loan = transport_.loan(); 
            if (!loan) return std::unexpected(BusError::ShmExhausted);
            
            loan->value = cmd;
            loan->timestamp_ns = nxd::chronos::now();
            return loan.send();
        }
    };
}
