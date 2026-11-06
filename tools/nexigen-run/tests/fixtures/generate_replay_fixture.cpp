/* Minimal MCAP fixture generator for the nexigen-run services test.
 *
 * Writes a single-channel MCAP file with one `telemetry` sample containing
 * a 32-bit float value of 50.0 at simulation time 0 ns.
 */

#include "nxd/runtime/recording/mcap.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

namespace mcap = nxd::runtime::recording::mcap;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: generate_replay_fixture <output.mcap>\n";
        return 1;
    }

    const std::string path = argv[1];

    mcap::Writer writer;
    if (!writer.open(path)) {
        std::cerr << "failed to open " << path << "\n";
        return 2;
    }

    mcap::Schema schema{};
    schema.id = 1;
    schema.name = "SignalValueF32@1.0.0";
    schema.encoding = "nxd";
    if (!writer.add_schema(schema)) {
        std::cerr << "failed to add schema\n";
        return 3;
    }

    mcap::Channel channel{};
    channel.id = 1;
    channel.schema_id = 1;
    channel.topic = "telemetry";
    channel.message_encoding = "nxd";
    channel.metadata["element_size"] = "4";
    channel.metadata["layout_hash"] =
        "0000000000000000000000000000000000000000000000000000000000000000";
    if (!writer.add_channel(channel)) {
        std::cerr << "failed to add channel\n";
        return 4;
    }

    const float value = 50.0f;
    mcap::Message message{};
    message.channel_id = 1;
    message.sequence = 1;
    message.log_time = 0;
    message.publish_time = 0;
    message.data = reinterpret_cast<const std::byte*>(&value);
    message.data_size = sizeof(value);
    if (!writer.write_message(message)) {
        std::cerr << "failed to write message\n";
        return 5;
    }

    if (!writer.close()) {
        std::cerr << "failed to close writer\n";
        return 6;
    }

    return 0;
}
