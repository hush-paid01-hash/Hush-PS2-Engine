#include "retroshelf/ee_vif1.hpp"
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
    std::string error;
    check(bus.map_mmio(retroshelf::EeTimerBlock::RegisterBase,
                       retroshelf::EeTimerBlock::RegisterSize, timers, &error),
          "map EE timer registers without consuming VIF address space");
    check(bus.map_mmio(retroshelf::EeVif1Registers::RegisterBase,
                       retroshelf::EeVif1Registers::RegisterSize, vif1, &error),
          "map VIF1 register window alongside timers");

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

    std::vector<std::uint8_t> bytes;
    check(!bus.read(base + 4U, 4, bytes, &error),
          "unimplemented VIF1 register reads fail explicitly");
    check(!bus.write(base, std::vector<std::uint8_t>{0, 0, 0, 0}, &error),
          "VIF1_STAT remains read-only");
    return failures == 0 ? 0 : 1;
}
