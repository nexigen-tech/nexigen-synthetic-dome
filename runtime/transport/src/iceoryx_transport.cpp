#include "nxd/runtime/transport/iceoryx_transport.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "iox2/iceoryx2.h"

namespace nxd::runtime::transport {

namespace {

constexpr std::size_t payload_alignment = alignof(std::max_align_t);

std::string payload_type_name(const std::array<std::uint8_t, 32>& layout_hash,
                              std::size_t max_count) {
    char buffer[128];
    std::size_t pos = 0;
    pos += static_cast<std::size_t>(
        std::snprintf(buffer + pos, sizeof(buffer) - pos, "nxd_payload_"));
    for (std::uint8_t byte : layout_hash) {
        pos += static_cast<std::size_t>(
            std::snprintf(buffer + pos, sizeof(buffer) - pos, "%02x", byte));
    }
    std::snprintf(buffer + pos, sizeof(buffer) - pos, "_%zu", max_count);
    return std::string(buffer);
}

std::string make_service_name(std::string_view source_name) {
    return std::string("nxd/") + std::string(source_name);
}

class IceoryxPublisher final : public Publisher {
  public:
    IceoryxPublisher(iox2_publisher_h handle,
                     std::size_t element_size,
                     std::size_t max_count) noexcept
        : handle_(handle), element_size_(element_size), max_count_(max_count) {}

    ~IceoryxPublisher() override {
        if (loaned_sample_ != nullptr) {
            // There is no explicit "cancel loan" in the C API.  We send the
            // sample as-is so that the underlying resources are released.
            iox2_sample_mut_send(loaned_sample_, nullptr);
            loaned_sample_ = nullptr;
        }
        if (handle_ != nullptr) {
            iox2_publisher_drop(handle_);
        }
    }

    SampleLoan loan() noexcept override {
        if (handle_ == nullptr || loaned_sample_ != nullptr) {
            return SampleLoan{};
        }

        iox2_sample_mut_h sample = nullptr;
        if (iox2_publisher_loan_slice_uninit(&handle_, nullptr, &sample, max_count_) != IOX2_OK
            || sample == nullptr) {
            return SampleLoan{};
        }

        void* payload = nullptr;
        std::size_t count = 0;
        iox2_sample_mut_payload_mut(&sample, &payload, &count);

        void* header = nullptr;
        iox2_sample_mut_user_header_mut(&sample, &header);
        if (header != nullptr) {
            std::memset(header, 0, sizeof(SampleMetadata));
        }

        loaned_sample_ = sample;
        SampleLoan loan{};
        loan.data = static_cast<std::byte*>(payload);
        loan.element_size = element_size_;
        loan.count = count;
        return loan;
    }

    void publish(const SampleMetadata& metadata) noexcept override {
        if (loaned_sample_ == nullptr) {
            return;
        }

        void* header = nullptr;
        iox2_sample_mut_user_header_mut(&loaned_sample_, &header);
        if (header != nullptr) {
            std::memcpy(header, &metadata, sizeof(SampleMetadata));
        }

        iox2_sample_mut_send(loaned_sample_, nullptr);
        loaned_sample_ = nullptr;
    }

  private:
    iox2_publisher_h handle_ = nullptr;
    std::size_t element_size_ = 0;
    std::size_t max_count_ = 0;
    iox2_sample_mut_h loaned_sample_ = nullptr;
};

class IceoryxSubscriber final : public Subscriber {
  public:
    IceoryxSubscriber(iox2_subscriber_h handle,
                      std::size_t element_size,
                      std::size_t max_count) noexcept
        : handle_(handle), element_size_(element_size) {
        buffer_.resize(element_size * max_count);
    }

    ~IceoryxSubscriber() override {
        if (handle_ != nullptr) {
            iox2_subscriber_drop(handle_);
        }
    }

    SampleView take() noexcept override {
        if (handle_ == nullptr) {
            return SampleView{};
        }

        iox2_sample_h sample = nullptr;
        if (iox2_subscriber_receive(&handle_, nullptr, &sample) != IOX2_OK) {
            return SampleView{};
        }
        if (sample == nullptr) {
            return SampleView{};
        }

        const void* payload = nullptr;
        std::size_t count = 0;
        iox2_sample_payload(&sample, &payload, &count);

        const void* header = nullptr;
        iox2_sample_user_header(&sample, &header);
        if (header != nullptr) {
            std::memcpy(&last_metadata_, header, sizeof(SampleMetadata));
        } else {
            last_metadata_ = SampleMetadata{};
        }

        const std::size_t bytes = count * element_size_;
        if (bytes <= buffer_.size() && payload != nullptr) {
            std::memcpy(buffer_.data(), payload, bytes);
        }

        iox2_sample_drop(sample);

        SampleView view{};
        view.data = buffer_.data();
        view.element_size = element_size_;
        view.count = count;
        view.metadata = last_metadata_;
        return view;
    }

  private:
    iox2_subscriber_h handle_ = nullptr;
    std::size_t element_size_ = 0;
    std::vector<std::byte> buffer_;
    SampleMetadata last_metadata_{};
};

} // namespace

struct IceoryxTransport::Impl {
    iox2_node_h node = nullptr;
    std::unordered_map<std::string, std::unique_ptr<IceoryxPublisher>> publishers;
    std::unordered_map<std::string, std::vector<std::unique_ptr<IceoryxSubscriber>>> subscribers;
};

IceoryxTransport::IceoryxTransport() noexcept : impl_(std::make_unique<Impl>()) {
    iox2_set_log_level_from_env_or(iox2_log_level_e_ERROR);

    iox2_node_builder_h builder = iox2_node_builder_new(nullptr);
    if (builder == nullptr) {
        return;
    }

    if (iox2_node_builder_create(builder, nullptr, iox2_service_type_e_IPC, &impl_->node)
        != IOX2_OK) {
        impl_->node = nullptr;
    }
}

IceoryxTransport::~IceoryxTransport() {
    if (impl_ == nullptr) {
        return;
    }
    impl_->publishers.clear();
    impl_->subscribers.clear();
    if (impl_->node != nullptr) {
        iox2_node_drop(impl_->node);
    }
}

Publisher* IceoryxTransport::create_publisher(std::string_view source_name,
                                               std::size_t element_size,
                                               std::size_t max_count,
                                               const std::array<std::uint8_t, 32>&
                                                   layout_hash) noexcept {
    if (impl_ == nullptr || impl_->node == nullptr || element_size == 0 || max_count == 0) {
        return nullptr;
    }

    const std::string key(source_name);
    auto it = impl_->publishers.find(key);
    if (it != impl_->publishers.end()) {
        return it->second.get();
    }

    const std::string service_name = make_service_name(source_name);
    iox2_service_name_h service_name_h = nullptr;
    if (iox2_service_name_new(nullptr,
                              service_name.c_str(),
                              service_name.size(),
                              &service_name_h)
        != IOX2_OK) {
        return nullptr;
    }

    iox2_service_builder_h service_builder =
        iox2_node_service_builder(&impl_->node, nullptr, iox2_cast_service_name_ptr(service_name_h));
    iox2_service_builder_pub_sub_h ps_builder = iox2_service_builder_pub_sub(service_builder);

    const std::string payload_name = payload_type_name(layout_hash, max_count);
    const std::size_t payload_size = element_size * max_count;
    if (iox2_service_builder_pub_sub_set_payload_type_details(&ps_builder,
                                                              iox2_type_variant_e_FIXED_SIZE,
                                                              payload_name.c_str(),
                                                              payload_name.size(),
                                                              payload_size,
                                                              payload_alignment)
        != IOX2_OK) {
        iox2_service_name_drop(service_name_h);
        return nullptr;
    }

    constexpr const char* header_name = "NxdSampleMetadata";
    if (iox2_service_builder_pub_sub_set_user_header_type_details(&ps_builder,
                                                                  iox2_type_variant_e_FIXED_SIZE,
                                                                  header_name,
                                                                  std::strlen(header_name),
                                                                  sizeof(SampleMetadata),
                                                                  alignof(SampleMetadata))
        != IOX2_OK) {
        iox2_service_name_drop(service_name_h);
        return nullptr;
    }

    iox2_port_factory_pub_sub_h port_factory = nullptr;
    if (iox2_service_builder_pub_sub_open_or_create(ps_builder, nullptr, &port_factory)
        != IOX2_OK) {
        iox2_service_name_drop(service_name_h);
        return nullptr;
    }

    iox2_port_factory_publisher_builder_h pub_builder =
        iox2_port_factory_pub_sub_publisher_builder(&port_factory, nullptr);
    iox2_publisher_h publisher_h = nullptr;
    const int rc = iox2_port_factory_publisher_builder_create(pub_builder, nullptr, &publisher_h);

    iox2_port_factory_pub_sub_drop(port_factory);
    iox2_service_name_drop(service_name_h);

    if (rc != IOX2_OK || publisher_h == nullptr) {
        return nullptr;
    }

    auto publisher =
        std::make_unique<IceoryxPublisher>(publisher_h, element_size, max_count);
    Publisher* ptr = publisher.get();
    impl_->publishers.emplace(key, std::move(publisher));
    return ptr;
}

Subscriber* IceoryxTransport::create_subscriber(std::string_view source_name,
                                                 std::size_t element_size,
                                                 std::size_t max_count,
                                                 const std::array<std::uint8_t, 32>&
                                                     layout_hash) noexcept {
    if (impl_ == nullptr || impl_->node == nullptr || element_size == 0 || max_count == 0) {
        return nullptr;
    }

    const std::string key(source_name);

    const std::string service_name = make_service_name(source_name);
    iox2_service_name_h service_name_h = nullptr;
    if (iox2_service_name_new(nullptr,
                              service_name.c_str(),
                              service_name.size(),
                              &service_name_h)
        != IOX2_OK) {
        return nullptr;
    }

    iox2_service_builder_h service_builder =
        iox2_node_service_builder(&impl_->node, nullptr, iox2_cast_service_name_ptr(service_name_h));
    iox2_service_builder_pub_sub_h ps_builder = iox2_service_builder_pub_sub(service_builder);

    const std::string payload_name = payload_type_name(layout_hash, max_count);
    const std::size_t payload_size = element_size * max_count;
    if (iox2_service_builder_pub_sub_set_payload_type_details(&ps_builder,
                                                              iox2_type_variant_e_FIXED_SIZE,
                                                              payload_name.c_str(),
                                                              payload_name.size(),
                                                              payload_size,
                                                              payload_alignment)
        != IOX2_OK) {
        iox2_service_name_drop(service_name_h);
        return nullptr;
    }

    constexpr const char* header_name = "NxdSampleMetadata";
    if (iox2_service_builder_pub_sub_set_user_header_type_details(&ps_builder,
                                                                  iox2_type_variant_e_FIXED_SIZE,
                                                                  header_name,
                                                                  std::strlen(header_name),
                                                                  sizeof(SampleMetadata),
                                                                  alignof(SampleMetadata))
        != IOX2_OK) {
        iox2_service_name_drop(service_name_h);
        return nullptr;
    }

    iox2_port_factory_pub_sub_h port_factory = nullptr;
    if (iox2_service_builder_pub_sub_open_or_create(ps_builder, nullptr, &port_factory)
        != IOX2_OK) {
        iox2_service_name_drop(service_name_h);
        return nullptr;
    }

    iox2_port_factory_subscriber_builder_h sub_builder =
        iox2_port_factory_pub_sub_subscriber_builder(&port_factory, nullptr);
    iox2_subscriber_h subscriber_h = nullptr;
    const int rc = iox2_port_factory_subscriber_builder_create(sub_builder, nullptr, &subscriber_h);

    iox2_port_factory_pub_sub_drop(port_factory);
    iox2_service_name_drop(service_name_h);

    if (rc != IOX2_OK || subscriber_h == nullptr) {
        return nullptr;
    }

    auto subscriber =
        std::make_unique<IceoryxSubscriber>(subscriber_h, element_size, max_count);
    Subscriber* ptr = subscriber.get();
    impl_->subscribers[key].push_back(std::move(subscriber));
    return ptr;
}

} // namespace nxd::runtime::transport
