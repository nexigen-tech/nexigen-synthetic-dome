#include "nxd/runtime/transport/headless_transport.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace nxd::runtime::transport {

namespace {

struct Channel {
    std::string source_name;
    std::size_t element_size = 0;
    std::size_t max_count = 0;
    std::array<std::uint8_t, 32> layout_hash{};
    std::vector<std::byte> buffer;
    bool published = false;
    SampleMetadata last_metadata{};
};

class HeadlessPublisher final : public Publisher {
  public:
    explicit HeadlessPublisher(Channel& channel) noexcept : channel_(&channel) {}

    SampleLoan loan() noexcept override {
        SampleLoan loan{};
        loan.data = channel_->buffer.data();
        loan.element_size = channel_->element_size;
        loan.count = channel_->max_count;
        return loan;
    }

    void publish(const SampleMetadata& metadata) noexcept override {
        channel_->last_metadata = metadata;
        channel_->published = true;
    }

  private:
    Channel* channel_ = nullptr;
};

class HeadlessSubscriber final : public Subscriber {
  public:
    explicit HeadlessSubscriber(Channel& channel) noexcept : channel_(&channel) {}

    SampleView take() noexcept override {
        if (!channel_->published) {
            return SampleView{};
        }
        SampleView view{};
        view.data = channel_->buffer.data();
        view.element_size = channel_->element_size;
        view.count = channel_->max_count;
        view.metadata = channel_->last_metadata;
        return view;
    }

  private:
    Channel* channel_ = nullptr;
};

} // namespace

struct HeadlessTransport::Impl {
    std::unordered_map<std::string, std::unique_ptr<Channel>> channels;
    std::unordered_map<std::string, std::unique_ptr<HeadlessPublisher>> publishers;
    std::unordered_map<std::string, std::vector<std::unique_ptr<HeadlessSubscriber>>> subscribers;
};

HeadlessTransport::HeadlessTransport() noexcept : impl_(std::make_unique<Impl>()) {}

HeadlessTransport::~HeadlessTransport() = default;

Publisher* HeadlessTransport::create_publisher(std::string_view source_name,
                                                std::size_t element_size,
                                                std::size_t max_count,
                                                const std::array<std::uint8_t, 32>&
                                                    layout_hash) noexcept {
    auto it = impl_->channels.find(std::string(source_name));
    if (it == impl_->channels.end()) {
        auto channel = std::make_unique<Channel>();
        channel->source_name = source_name;
        channel->element_size = element_size;
        channel->max_count = max_count;
        channel->layout_hash = layout_hash;
        channel->buffer.resize(element_size * max_count);
        it = impl_->channels.emplace(channel->source_name, std::move(channel)).first;
    }
    auto pub = std::make_unique<HeadlessPublisher>(*it->second);
    auto* ptr = pub.get();
    impl_->publishers.emplace(std::string(source_name), std::move(pub));
    return ptr;
}

Subscriber* HeadlessTransport::create_subscriber(std::string_view source_name,
                                                  std::size_t element_size,
                                                  std::size_t max_count,
                                                  const std::array<std::uint8_t, 32>&
                                                      layout_hash) noexcept {
    (void)element_size;
    (void)max_count;
    (void)layout_hash;
    auto it = impl_->channels.find(std::string(source_name));
    if (it == impl_->channels.end()) {
        auto channel = std::make_unique<Channel>();
        channel->source_name = source_name;
        channel->element_size = element_size;
        channel->max_count = max_count;
        channel->layout_hash = layout_hash;
        channel->buffer.resize(element_size * max_count);
        it = impl_->channels.emplace(channel->source_name, std::move(channel)).first;
    }
    auto sub = std::make_unique<HeadlessSubscriber>(*it->second);
    auto* ptr = sub.get();
    impl_->subscribers[std::string(source_name)].push_back(std::move(sub));
    return ptr;
}

} // namespace nxd::runtime::transport
