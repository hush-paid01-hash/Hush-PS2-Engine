#include "retroshelf/ee_vif1.hpp"
#include "retroshelf/ee_vif1_command_decoder.hpp"
#include "retroshelf/vu1_memory.hpp"
#include "retroshelf/ee_timer.hpp"
#include "retroshelf/ps2_ee_memory_map.hpp"
#include "retroshelf/system_bus.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void write32(retroshelf::SystemBus& bus, std::uint32_t address, std::uint32_t value) {
    std::vector<std::uint8_t> bytes(4);
    for (std::size_t i = 0; i < 4; ++i) bytes[i] = static_cast<std::uint8_t>(value >> (8U * i));
    std::string error;
    if (!bus.write(address, bytes, &error)) throw std::runtime_error(error);
}
std::uint32_t read32(retroshelf::SystemBus& bus, std::uint32_t address) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!bus.read(address, 4, bytes, &error)) throw std::runtime_error(error);
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(bytes[i]) << (8U * i);
    return value;
}
}

int main() {
    int failures = 0;
    const auto check = [&failures](bool condition, const char* message) {
        if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
        else std::cout << "PASS: " << message << '\n';
    };
    retroshelf::Ps2EeMemoryMap memory(1024);
    retroshelf::SystemBus bus(memory);
    auto timers = std::make_shared<retroshelf::EeTimerBlock>();
    auto vif1 = std::make_shared<retroshelf::EeVif1Registers>();
    auto vif1_fifo = std::make_shared<retroshelf::EeVif1Fifo>(*vif1);
    std::string error;
    check(bus.map_mmio(retroshelf::EeTimerBlock::RegisterBase,
                       retroshelf::EeTimerBlock::RegisterSize, timers, &error),
          "map EE timer registers without consuming VIF address space");
    check(bus.map_mmio(retroshelf::EeVif1Registers::RegisterBase,
                       retroshelf::EeVif1Registers::RegisterSize, vif1, &error),
          "map VIF1 register window alongside timers");
    check(bus.map_mmio(retroshelf::EeVif1Fifo::RegisterBase,
                       retroshelf::EeVif1Fifo::RegisterSize, vif1_fifo, &error),
          "map VIF1 128-bit FIFO data port");

    constexpr auto base = retroshelf::EeVif1Registers::RegisterBase;
    write32(bus, base + retroshelf::EeVif1Registers::CycleOffset, 0x00050302U);
    write32(bus, base + retroshelf::EeVif1Registers::ModeOffset, 0x7U);
    write32(bus, base + retroshelf::EeVif1Registers::NumOffset, 0x123U);
    write32(bus, base + retroshelf::EeVif1Registers::MaskOffset, 0xA5A55A5AU);
    write32(bus, base + retroshelf::EeVif1Registers::BaseOffset, 0xFFFFFC12U);
    write32(bus, base + retroshelf::EeVif1Registers::RowBaseOffset + 0x20U, 0x11223344U);
    write32(bus, base + retroshelf::EeVif1Registers::ColumnBaseOffset + 0x10U, 0x55667788U);
    check(read32(bus, base + retroshelf::EeVif1Registers::CycleOffset) == 0x00000302U,
          "VIF1_CYCLE stores CL and WL");
    check(read32(bus, base + retroshelf::EeVif1Registers::ModeOffset) == 3U,
          "VIF1_MODE masks to two mode bits");
    check(read32(bus, base + retroshelf::EeVif1Registers::NumOffset) == 0x23U,
          "VIF1_NUM masks to eight bits");
    check(read32(bus, base + retroshelf::EeVif1Registers::MaskOffset) == 0xA5A55A5AU,
          "VIF1_MASK round-trips its 32-bit matrix");
    check(read32(bus, base + retroshelf::EeVif1Registers::BaseOffset) == 0x12U,
          "VIF1_BASE masks to ten bits");
    check(read32(bus, base + retroshelf::EeVif1Registers::RowBaseOffset + 0x20U) == 0x11223344U &&
          read32(bus, base + retroshelf::EeVif1Registers::ColumnBaseOffset + 0x10U) == 0x55667788U,
          "VIF1 row and column registers retain independent values");

    vif1->set_status_flags(retroshelf::EeVif1Registers::StatInt |
                           retroshelf::EeVif1Registers::StatMrk |
                           retroshelf::EeVif1Registers::StatEr1);
    vif1->set_command_status(2U);
    check((read32(bus, base) & (retroshelf::EeVif1Registers::StatInt |
                                retroshelf::EeVif1Registers::StatMrk |
                                retroshelf::EeVif1Registers::StatEr1 |
                                retroshelf::EeVif1Registers::StatVpsMask)) ==
          (retroshelf::EeVif1Registers::StatInt | retroshelf::EeVif1Registers::StatMrk |
           retroshelf::EeVif1Registers::StatEr1 | 2U),
          "VIF1_STAT exposes command and status flags");
    write32(bus, base + retroshelf::EeVif1Registers::MarkOffset, 0x12345678U);
    check(read32(bus, base + retroshelf::EeVif1Registers::MarkOffset) == 0x5678U &&
          (read32(bus, base) & retroshelf::EeVif1Registers::StatMrk) == 0U,
          "writing VIF1_MARK masks value and clears MRK");

    write32(bus, base + retroshelf::EeVif1Registers::FbrstOffset, 0x6U);
    check((read32(bus, base) & (retroshelf::EeVif1Registers::StatVfs |
                                retroshelf::EeVif1Registers::StatVss)) ==
          (retroshelf::EeVif1Registers::StatVfs | retroshelf::EeVif1Registers::StatVss),
          "VIF1_FBRST force-break and stop set their status flags");
    write32(bus, base + retroshelf::EeVif1Registers::FbrstOffset, 0x8U);
    check((read32(bus, base) & (retroshelf::EeVif1Registers::StatVfs |
                                retroshelf::EeVif1Registers::StatVss |
                                retroshelf::EeVif1Registers::StatInt |
                                retroshelf::EeVif1Registers::StatEr1)) == 0U,
          "VIF1_FBRST stall-cancel clears documented stall and error flags");
    write32(bus, base + retroshelf::EeVif1Registers::FbrstOffset, 0x1U);
    check(read32(bus, base) == 0U &&
          read32(bus, base + retroshelf::EeVif1Registers::ErrOffset) == 0x2U,
          "VIF1_FBRST reset restores register defaults");

    // Exercise the 16-quadword FIFO through the guest-visible MMIO port.
    constexpr auto fifo_base = retroshelf::EeVif1Fifo::RegisterBase;
    std::vector<std::uint8_t> quadword(16U);
    std::vector<std::uint8_t> empty_read;
    check(!bus.read(fifo_base, 16U, empty_read, &error),
          "empty VIF1 FIFO reports underflow");
    bool pushed_all = true;
    for (std::size_t q = 0; q < retroshelf::EeVif1Fifo::Capacity; ++q) {
        for (std::size_t b = 0; b < quadword.size(); ++b)
            quadword[b] = static_cast<std::uint8_t>(q * 16U + b);
        if (!bus.write(fifo_base, quadword, &error)) pushed_all = false;
    }
    check(pushed_all && vif1_fifo->count() == 16U &&
          ((read32(bus, base) & retroshelf::EeVif1Registers::StatFqcMask) >>
           retroshelf::EeVif1Registers::StatFqcShift) == 16U,
          "VIF1 FIFO accepts sixteen quadwords and reports FQC");
    check(!bus.write(fifo_base, quadword, &error),
          "full VIF1 FIFO reports overflow without overwriting data");
    bool popped_all = true;
    for (std::size_t q = 0; q < retroshelf::EeVif1Fifo::Capacity; ++q) {
        std::vector<std::uint8_t> received;
        if (!bus.read(fifo_base, 16U, received, &error) || received.size() != 16U ||
            received[0] != static_cast<std::uint8_t>(q * 16U) ||
            received[15] != static_cast<std::uint8_t>(q * 16U + 15U))
            popped_all = false;
    }
    check(popped_all && vif1_fifo->count() == 0U &&
          (read32(bus, base) & retroshelf::EeVif1Registers::StatFqcMask) == 0U,
          "VIF1 FIFO preserves order and clears FQC when drained");
    quadword.assign(16U, 0xA5U);
    check(!bus.write(fifo_base + 1U, quadword, &error),
          "VIF1 FIFO rejects nonzero data-port offsets");
    check(!bus.write(fifo_base, std::vector<std::uint8_t>(8U), &error),
          "VIF1 FIFO rejects non-128-bit writes");
    check(bus.write(fifo_base, quadword, &error),
          "VIF1 FIFO accepts a quadword after draining");
    vif1_fifo->reset();
    check(vif1_fifo->count() == 0U &&
          (read32(bus, base) & retroshelf::EeVif1Registers::StatFqcMask) == 0U,
          "VIF1 FIFO reset empties queue and status count");
    // The FIFO can now feed the incremental command decoder, rather than
    // remaining an isolated MMIO queue. A pair of NOP-only quadwords should
    // be consumed in order and update FQC as the queue drains.
    retroshelf::Vu1Memory vu1_memory;
    retroshelf::EeVif1CommandDecoder decoder(*vif1, vu1_memory);
    std::vector<std::uint8_t> nop_quadword(16U, 0U);
    check(bus.write(fifo_base, nop_quadword, &error) &&
          bus.write(fifo_base, nop_quadword, &error),
          "queue VIF1 command quadwords for decoder integration");
    std::size_t drained = 0U;
    check(vif1_fifo->drain_to(decoder, &drained) && drained == 2U &&
          vif1_fifo->count() == 0U &&
          (read32(bus, base) & retroshelf::EeVif1Registers::StatFqcMask) == 0U,
          "VIF1 FIFO drains quadwords into command decoder and updates FQC");

    // Unsupported opcodes fail closed and set ER1; the FIFO reports exactly
    // how many quadwords were consumed before the decoder stopped.
    std::vector<std::uint8_t> invalid_command(16U, 0U);
    invalid_command[3] = 0xFFU;
    check(bus.write(fifo_base, invalid_command, &error),
          "queue unsupported VIF1 opcode for error-path test");
    drained = 0U;
    check(!vif1_fifo->drain_to(decoder, &drained) && drained == 1U &&
          (read32(bus, base) & retroshelf::EeVif1Registers::StatEr1) != 0U,
          "VIF1 decoder errors stop FIFO drain and expose ER1");
    decoder.reset();
    vif1_fifo->reset();

    // Leave an incomplete STMASK payload in the decoder, queue data, then
    // issue the guest-visible FBRST reset. FIFO observation must clear both
    // queued data and decoder staging before the next drain.
    const std::uint32_t stmask = 0x20000000U;
    std::vector<std::uint8_t> stmask_header(4U);
    for (std::size_t i = 0; i < 4U; ++i)
        stmask_header[i] = static_cast<std::uint8_t>(stmask >> (8U * i));
    check(decoder.consume(stmask_header.data(), stmask_header.size()),
          "decoder stages an incomplete VIF1 payload before reset");
    check(bus.write(fifo_base, nop_quadword, &error),
          "queue VIF1 data before hardware reset");
    write32(bus, base + retroshelf::EeVif1Registers::FbrstOffset, 0x1U);
    check(vif1_fifo->count() == 0U &&
          (read32(bus, base) & retroshelf::EeVif1Registers::StatFqcMask) == 0U,
          "FBRST reset lazily clears FIFO and FQC");
    check(bus.write(fifo_base, nop_quadword, &error),
          "queue fresh VIF1 data after hardware reset");
    drained = 0U;
    check(vif1_fifo->drain_to(decoder, &drained) && drained == 1U &&
          (read32(bus, base) & retroshelf::EeVif1Registers::StatEr1) == 0U,
          "FBRST reset also clears decoder's incomplete command state");
    vif1_fifo->reset();

    vif1->set_fifo_direction(true);
    check((read32(bus, base) & retroshelf::EeVif1Registers::StatFdr) != 0U,
          "VIF1_STAT exposes FIFO direction flag");
    vif1->set_fifo_direction(false);
    check((read32(bus, base) & retroshelf::EeVif1Registers::StatFdr) == 0U,
          "VIF1_STAT clears FIFO direction flag");

    std::vector<std::uint8_t> bytes;
    check(!bus.read(base + 4U, 4, bytes, &error),
          "unimplemented VIF1 register reads fail explicitly");
    check(!bus.write(base, std::vector<std::uint8_t>{0, 0, 0, 0}, &error),
          "VIF1_STAT remains read-only");
    return failures == 0 ? 0 : 1;
}
