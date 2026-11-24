#include "retroshelf/vu1_memory.hpp"

#include <array>
#include <cstdint>
#include <iostream>

int main() {
    int failures = 0;
    const auto check = [&failures](bool condition, const char* message) {
        if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
        else std::cout << "PASS: " << message << '\n';
    };

    retroshelf::Vu1Memory memory;
    check(memory.micro_bytes().size() == 16U * 1024U &&
          memory.data_bytes().size() == 16U * 1024U,
          "allocates separate 16 KiB VU1 micro and data memories");

    const std::array<std::uint8_t, 16> program{{
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
        0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE
    }};
    std::array<std::uint8_t, 16> readback{};
    check(memory.write_micro(0, program.data(), program.size()),
          "writes a VU1 microprogram byte range");
    check(memory.read_micro(0, readback.data(), readback.size()) &&
          readback == program, "reads microprogram bytes back unchanged");
    check(memory.read_data(0, readback.data(), readback.size()) &&
          readback != program, "micro and data memories remain independent");

    check(memory.write_data(retroshelf::Vu1Memory::DataMemorySize - 16U,
                            program.data(), program.size()),
          "writes the final VU1 data-memory quadword");
    check(!memory.write_data(retroshelf::Vu1Memory::DataMemorySize - 8U,
                             program.data(), program.size()),
          "rejects a data write crossing the memory boundary");
    check(!memory.read_micro(retroshelf::Vu1Memory::MicroMemorySize,
                             readback.data(), 1U),
          "rejects a micro-memory read beyond its boundary");
    check(!memory.write_micro(0, nullptr, 1U),
          "rejects a null source for a non-empty write");
    check(memory.read_data(retroshelf::Vu1Memory::DataMemorySize,
                           nullptr, 0U),
          "allows an empty range at the end of memory");

    memory.reset();
    check(memory.micro_bytes()[0] == 0 &&
          memory.data_bytes().back() == 0,
          "reset clears both VU1 memory regions");
    return failures == 0 ? 0 : 1;
}
