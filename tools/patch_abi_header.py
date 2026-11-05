#!/usr/bin/env python3
"""Post-process cbindgen-generated nxd_abi.h.

cbindgen emits the synthetic name `Option_NxdStepIoFn` for the Rust
`Option<NxdStepIoFn>` table field and places the port types after the
`NxdAbiTable` struct.  At the ABI level the field is a nullable function
pointer, so we rewrite it to `NxdStepIoFn` and move the port-type definitions
before the table.
"""

import re
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <nxd_abi.h>", file=sys.stderr)
        return 1

    path = sys.argv[1]
    with open(path, encoding="utf-8") as f:
        text = f.read()

    # Rewrite the synthetic Option type name to the real typedef name.
    text = re.sub(r"Option_NxdStepIoFn\b", "NxdStepIoFn", text)

    # Remove the spurious forward declaration that cbindgen emitted for the
    # synthetic Option type.
    text = re.sub(r"typedef struct NxdStepIoFn NxdStepIoFn;\n", "", text)

    # Move the port slot, port bundle and step_io typedef block before the
    # NxdAbiTable struct so the table field can refer to NxdStepIoFn.
    block_pattern = re.compile(
        r"/\*\*\n \* Slot describing one typed input or output port buffer passed to `step_io`.*?\);",
        re.DOTALL,
    )
    match = block_pattern.search(text)
    if match:
        block = match.group(0)
        text = text[: match.start()] + text[match.end() :]
        insert_pos = text.find("/**\n * ABI-1 plugin table.")
        if insert_pos != -1:
            text = text[:insert_pos] + block + "\n\n" + text[insert_pos:]

    with open(path, "w", encoding="utf-8") as f:
        f.write(text)

    return 0


if __name__ == "__main__":
    sys.exit(main())
