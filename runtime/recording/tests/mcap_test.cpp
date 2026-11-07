#include "nxd/runtime/recording/mcap.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace mcap = nxd::runtime::recording::mcap;

struct TestSample {
    double value = 0.0;
};

int main() {
    const char* path = "/tmp/opencode/mcap_roundtrip_test.mcap";
    {
        mcap::Writer writer;
        if (!writer.open(path)) return 1;

        mcap::Schema schema{};
        schema.id = 1;
        schema.name = "TestSample@1.0.0";
        schema.encoding = "nxd";
        if (!writer.add_schema(schema)) return 2;

        mcap::Channel channel{};
        channel.id = 1;
        channel.schema_id = 1;
        channel.topic = "signal";
        channel.message_encoding = "nxd";
        channel.metadata["element_size"] = "8";
        channel.metadata["layout_hash"] = std::string(64, '0');
        if (!writer.add_channel(channel)) return 3;

        for (int i = 0; i < 5; ++i) {
            TestSample sample{static_cast<double>(i * 10)};
            mcap::Message msg{};
            msg.channel_id = 1;
            msg.sequence = static_cast<std::uint32_t>(i);
            msg.log_time = static_cast<std::uint64_t>(i + 1) * 1000;
            msg.publish_time = msg.log_time;
            msg.data = reinterpret_cast<const std::byte*>(&sample);
            msg.data_size = sizeof(sample);
            if (!writer.write_message(msg)) return 4;
        }

        mcap::Attachment attachment{};
        attachment.name = "RecordingMetadata";
        attachment.media_type = "application/x-nxd.recording-metadata";
        attachment.log_time = 5000;
        attachment.create_time = 5000;
        const char* meta = "meta";
        attachment.data.assign(reinterpret_cast<const std::byte*>(meta),
                               reinterpret_cast<const std::byte*>(meta) + 4);
        if (!writer.add_attachment(attachment)) return 5;

        if (!writer.close()) return 6;
    }

    mcap::Reader reader;
    if (!reader.open(path)) return 10;

    if (reader.channels().size() != 1) return 11;
    if (reader.channels()[0].topic != "signal") return 12;
    if (reader.attachments().size() != 1) return 13;

    const auto& messages = reader.messages();
    if (messages.size() != 5) return 20;
    for (std::size_t i = 0; i < messages.size(); ++i) {
        if (messages[i].log_time != static_cast<std::uint64_t>((i + 1) * 1000)) return 21;
        if (messages[i].data.size() != sizeof(TestSample)) return 22;
        double value = 0.0;
        std::memcpy(&value, messages[i].data.data(), sizeof(value));
        if (value != static_cast<double>(i * 10)) return 23;
    }

    const auto stats = reader.statistics();
    if (!stats || stats->message_count != 5) return 30;
    if (stats->attachment_count != 1) return 31;

    return 0;
}
