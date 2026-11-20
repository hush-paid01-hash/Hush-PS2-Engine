#include "retroshelf/ee_vif1_command_decoder.hpp"

#include <algorithm>

namespace retroshelf {

namespace {
std::uint32_t load_u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8U) |
           (static_cast<std::uint32_t>(p[2]) << 16U) |
           (static_cast<std::uint32_t>(p[3]) << 24U);
}
}

EeVif1CommandDecoder::EeVif1CommandDecoder(EeVif1Registers& registers,
                                           Vu1Memory& memory)
    : registers_(registers), memory_(memory) {}

void EeVif1CommandDecoder::set_error(const char* message) {
    last_error_ = message;
    error_code_ = 1U;
    registers_.set_status_flags(EeVif1Registers::StatEr1);
    registers_.set_command_status(0U);
}

const char* EeVif1CommandDecoder::last_error() const { return last_error_; }

void EeVif1CommandDecoder::reset() {
    pending_.clear();
    payload_bytes_ = 0;
    pending_opcode_ = 0;
    pending_num_ = pending_immediate_ = 0;
    unpack_address_ = unpack_remaining_ = 0;
    error_code_ = 0;
    last_error_ = "";
    registers_.set_command_status(0U);
}

bool EeVif1CommandDecoder::consume(const std::uint8_t* bytes, std::size_t size) {
    if (size != 0U && bytes == nullptr) {
        set_error("VIF1 input buffer is null");
        return false;
    }
    if (size > 1024U * 1024U || pending_.size() > 1024U * 1024U - size) {
        set_error("VIF1 command input exceeds bounded staging capacity");
        return false;
    }
    if (size != 0U) pending_.insert(pending_.end(), bytes, bytes + size);
    return process_available();
}

bool EeVif1CommandDecoder::process_available() {
    while (true) {
        if (payload_bytes_ != 0U) {
            if (pending_.size() < payload_bytes_) {
                registers_.set_command_status(1U);
                return true;
            }
            if (!consume_payload()) return false;
            pending_.erase(pending_.begin(), pending_.begin() +
                           static_cast<std::ptrdiff_t>(payload_bytes_));
            payload_bytes_ = 0;
            continue;
        }
        if (unpack_remaining_ != 0U) {
            if (pending_.size() < 4U) {
                registers_.set_command_status(3U);
                return true;
            }
            const std::uint32_t scalar = load_u32(pending_.data());
            const std::uint8_t vector[16] = {
                static_cast<std::uint8_t>(scalar), static_cast<std::uint8_t>(scalar >> 8U),
                static_cast<std::uint8_t>(scalar >> 16U), static_cast<std::uint8_t>(scalar >> 24U),
                static_cast<std::uint8_t>(scalar), static_cast<std::uint8_t>(scalar >> 8U),
                static_cast<std::uint8_t>(scalar >> 16U), static_cast<std::uint8_t>(scalar >> 24U),
                static_cast<std::uint8_t>(scalar), static_cast<std::uint8_t>(scalar >> 8U),
                static_cast<std::uint8_t>(scalar >> 16U), static_cast<std::uint8_t>(scalar >> 24U),
                static_cast<std::uint8_t>(scalar), static_cast<std::uint8_t>(scalar >> 8U),
                static_cast<std::uint8_t>(scalar >> 16U), static_cast<std::uint8_t>(scalar >> 24U)
            };
            if (!memory_.write_data(unpack_address_, vector, sizeof(vector))) {
                set_error("VIF1 S-32 UNPACK exceeds VU1 data memory");
                return false;
            }
            unpack_address_ += 16U;
            --unpack_remaining_;
            pending_.erase(pending_.begin(), pending_.begin() + 4);
            registers_.set_command_status(unpack_remaining_ == 0U ? 0U : 3U);
            continue;
        }
        if (pending_.size() < 4U) {
            registers_.set_command_status(pending_.empty() ? 0U : 1U);
            return true;
        }
        const std::uint32_t code = load_u32(pending_.data());
        pending_.erase(pending_.begin(), pending_.begin() + 4);
        if (!execute_command(code)) return false;
    }
}

bool EeVif1CommandDecoder::consume_payload() {
    if (pending_opcode_ == 0x20U) {
        std::vector<std::uint8_t> data(pending_.begin(),
                                       pending_.begin() + 4);
        return registers_.write(EeVif1Registers::MaskOffset, data);
    }
    if (pending_opcode_ == 0x30U || pending_opcode_ == 0x31U) {
        const std::uint32_t base = pending_opcode_ == 0x30U
            ? EeVif1Registers::RowBaseOffset : EeVif1Registers::ColumnBaseOffset;
        for (std::size_t i = 0; i < 4U; ++i) {
            std::vector<std::uint8_t> word(pending_.begin() +
                static_cast<std::ptrdiff_t>(i * 4U), pending_.begin() +
                static_cast<std::ptrdiff_t>((i + 1U) * 4U));
            if (!registers_.write(base + static_cast<std::uint32_t>(i * 0x10U), word))
                return false;
        }
        return true;
    }
    set_error("VIF1 payload opcode is unsupported");
    return false;
}

bool EeVif1CommandDecoder::execute_command(std::uint32_t code) {
    const auto opcode = static_cast<std::uint8_t>(code >> 24U);
    const std::uint32_t num = (code >> 16U) & 0xFFU;
    const std::uint32_t immediate = code & 0xFFFFU;
    const std::uint32_t command = code & 0x7FFFFFFFU;
    std::vector<std::uint8_t> word(4);
    for (std::size_t i = 0; i < 4U; ++i)
        word[i] = static_cast<std::uint8_t>(immediate >> (i * 8U));

    registers_.set_command_status(2U);
    if ((code & 0x80000000U) != 0U) {
        set_error("VIF1 interrupt commands require interrupt-controller integration");
        return false;
    }
    switch (opcode) {
    case 0x00: // NOP
        return true;
    case 0x01: // STCYCL
        if ((immediate & 0xFFFFU) != 0x0101U) {
            set_error("VIF1 STCYCL currently supports only CL=1, WL=1");
            return false;
        }
        return registers_.write(EeVif1Registers::CycleOffset, word);
    case 0x02: { // OFFSET (VIF1)
        if (!registers_.write(EeVif1Registers::OfstOffset, word)) return false;
        std::vector<std::uint8_t> base_word(4);
        const std::uint32_t tops = registers_.tops();
        for (std::size_t i = 0; i < 4U; ++i)
            base_word[i] = static_cast<std::uint8_t>(tops >> (i * 8U));
        return registers_.write(EeVif1Registers::BaseOffset, base_word);
    }
    case 0x03: return registers_.write(EeVif1Registers::BaseOffset, word);
    case 0x04: return registers_.write(EeVif1Registers::ItopOffset, word);
    case 0x05: return registers_.write(EeVif1Registers::ModeOffset, word);
    case 0x07:
        if (!registers_.write(EeVif1Registers::MarkOffset, word)) return false;
        registers_.set_status_flags(EeVif1Registers::StatMrk);
        return true;
    case 0x20: case 0x30: case 0x31:
        pending_opcode_ = opcode;
        payload_bytes_ = opcode == 0x20U ? 4U : 16U;
        registers_.set_command_status(1U);
        return true;
    default:
        break;
    }

    if (opcode >= 0x60U && opcode <= 0x7FU) {
        if ((opcode & 0x0FU) != 0U || (opcode & 0x10U) != 0U) {
            set_error("VIF1 UNPACK currently implements S-32 format only");
            return false;
        }
        const std::uint32_t count = num == 0U ? 256U : num;
        if ((immediate & 0x8000U) != 0U) {
            const std::uint32_t address =
                ((immediate & 0x3FFU) + registers_.tops()) * 16U;
            if (address >= Vu1Memory::DataMemorySize) {
                set_error("VIF1 S-32 UNPACK TOPS address is outside VU1 data memory");
                return false;
            }
            unpack_address_ = address;
        } else {
            unpack_address_ = (immediate & 0x3FFU) * 16U;
        }
        unpack_remaining_ = count;
        registers_.set_command_status(3U);
        return true;
    }

    (void)command;
    set_error("VIF1 command is not implemented by this decoder");
    return false;
}

} // namespace retroshelf
