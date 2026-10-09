#!/usr/bin/env python3
import argparse
import pathlib
import struct


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert a SPIR-V binary to a C++ uint32_t initializer.")
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("--symbol", required=True)
    args = parser.parse_args()

    data = args.input.read_bytes()
    if len(data) % 4 != 0:
        parser.error("SPIR-V size is not a multiple of four bytes")

    words = struct.unpack("<" + "I" * (len(data) // 4), data)
    if not words or words[0] != 0x07230203:
        parser.error("input does not contain SPIR-V magic")

    print(f"// {args.input.name}: {len(words)} SPIR-V words")
    print(f"static const std::vector<std::uint32_t> {args.symbol}{{")
    for offset in range(0, len(words), 6):
        chunk = words[offset:offset + 6]
        print("    " + ", ".join(f"0x{word:08X}u" for word in chunk) + ("," if offset + 6 < len(words) else ""))
    print("};")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
