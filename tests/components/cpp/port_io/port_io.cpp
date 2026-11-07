/* Test plugin exercising the extended step_io callback. */

#include "sample.hpp"
#include "nxd/sdk.hpp"
#include <cstdint>
#include <cstring>

namespace {

struct State {
    std::uint64_t received_sequence = 0;
    std::int64_t received_value = 0;
};

State g_state{};

NxdComponentState* create() {
    g_state = State{};
    return reinterpret_cast<NxdComponentState*>(&g_state);
}

void destroy(NxdComponentState*) {}

NxdStatus initialize(NxdComponentState*, const char*, std::uintptr_t) {
    return NXD_OK;
}

NxdStatus arm(NxdComponentState*, const NxdFrameContext*) {
    return NXD_OK;
}

void stop(NxdComponentState*) {}

NxdStatus step_io(NxdComponentState* state, const NxdFrameContext*,
                  const NxdPortIoBundle* inputs, NxdPortIoBundle* outputs) {
    auto* s = reinterpret_cast<State*>(state);

    const nxd::Port<nxd_test::PortIoSample> in(inputs->slots[0]);
    const auto in_span = in.as_span();
    if (in_span.empty()) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    s->received_sequence = in_span[0].sequence;
    s->received_value = in_span[0].value;

    nxd::OutputLoan<nxd_test::PortIoSample> out(outputs->slots[0]);
    const auto out_span = out.as_span();
    if (out_span.empty()) {
        return NXD_ERROR_INVALID_ARGUMENT;
    }
    out_span[0].sequence = in_span[0].sequence;
    out_span[0].value = in_span[0].value + 1;

    return NXD_OK;
}

NxdStatus step(NxdComponentState*, const NxdFrameContext*) {
    return NXD_OK;
}

} // namespace

extern "C" {

const NxdAbiTable* nxd_abi_entry() {
    static NxdAbiTable table{};
    table.abi_version = NxdAbiVersion_ABI_1;
    table.table_size = sizeof(NxdAbiTable);
    table.fingerprint = *reinterpret_cast<const NxdAbiFingerprint*>(NXD_ABI_1_TARGET);
    table.sdk_layout_hash = NxdAbiFingerprint{};
    const char name[] = "PortIoTest";
    std::strncpy(table.component_name, name, sizeof(table.component_name) - 1);
    table.component_name[sizeof(table.component_name) - 1] = '\0';
    table.component_version = NxdAbiVersion{1, 0, 0};
    table.descriptor_hash = NxdAbiFingerprint{};
    table.create = create;
    table.destroy = destroy;
    table.initialize = initialize;
    table.arm = arm;
    table.step = step;
    table.stop = stop;
    table.step_io = step_io;
    return &table;
}

} // extern "C"
