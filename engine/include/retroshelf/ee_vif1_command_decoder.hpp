#pragma once

#include "retroshelf/ee_vif1.hpp"
#include "retroshelf/vu1_memory.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace retroshelf {

// Incremental VIF1 command-stream decoder. This first execution slice supports
// state-setting commands and S-32 UNPACK only; unsupported commands fail closed.
class EeVif1CommandDecoder {
public:
    EeVif1CommandDecoder(EeVif1Registers& registers, Vu1Memory& memory);

    bool consume(const std::uint8_t* bytes, std::size_t size);
    void reset();
    const char* last_error() const;

private:
    bool process_available();
    bool execute_command(std::uint32_t code);
    bool consume_payload();
    void set_error(const char* message);

    EeVif1Registers& registers_;
    Vu1Memory& memory_;
    std::vector<std::uint8_t> pending_;
    std::size_t payload_bytes_{0};
    std::uint8_t pending_opcode_{0};
    std::uint32_t pending_num_{0};
    std::uint32_t pending_immediate_{0};
    std::uint32_t unpack_address_{0};
    std::uint32_t unpack_remaining_{0};
    std::uint32_t unpack_format_{0};
    std::uint32_t unpack_payload_bytes_{0};
    std::uint32_t error_code_{0};
    const char* last_error_{""};
};

} // namespace retroshelf
