#include "retroshelf/ee_gif_dmac.hpp"
#include "retroshelf/ee_vif1.hpp"
#include "retroshelf/ee_vif1_command_decoder.hpp"
#include "retroshelf/ee_vif1_dmac.hpp"
#include "retroshelf/ps2_ee_memory_map.hpp"
#include "retroshelf/system_bus.hpp"
#include "retroshelf/vu1_memory.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void write32(retroshelf::SystemBus& bus, std::uint32_t address, std::uint32_t value) {
    std::vector<std::uint8_t> data(4U);
    for (std::size_t i = 0; i < 4U; ++i)
        data[i] = static_cast<std::uint8_t>(value >> (i * 8U));
    std::string error;
    if (!bus.write(address, data, &error)) throw std::runtime_error(error);
}
std::uint32_t read32(retroshelf::SystemBus& bus, std::uint32_t address) {
    std::vector<std::uint8_t> data;
    std::string error;
    if (!bus.read(address, 4U, data, &error)) throw std::runtime_error(error);
    std::uint32_t value = 0U;
    for (std::size_t i = 0; i < 4U; ++i)
        value |= static_cast<std::uint32_t>(data[i]) << (i * 8U);
    return value;
}
}

int main() {
    int failures = 0;
    const auto check = [&failures](bool ok, const char* message) {
        if (ok) std::cout << "PASS: " << message << '\n';
        else { std::cerr << "FAIL: " << message << '\n'; ++failures; }
    };
    retroshelf::Ps2EeMemoryMap memory(256U);
    // A single VIF NOP command followed by padding, stored at EE RAM address 0.
    memory.main_ram().assign(256U, 0U);
    retroshelf::SystemBus bus(memory);
    auto vif = std::make_shared<retroshelf::EeVif1Registers>();
    auto fifo = std::make_shared<retroshelf::EeVif1Fifo>(*vif);
    auto vu1 = std::make_shared<retroshelf::Vu1Memory>();
    auto decoder = std::make_shared<retroshelf::EeVif1CommandDecoder>(*vif, *vu1);
    auto global = std::make_shared<retroshelf::EeDmacGlobalRegisters>();
    auto dma = std::make_shared<retroshelf::EeVif1DmacChannel1>(bus, *fifo, *decoder, *global);
    std::string error;
    check(bus.map_mmio(retroshelf::EeVif1Registers::RegisterBase,
                       retroshelf::EeVif1Registers::RegisterSize, vif, &error),
          "map VIF1 registers");
    check(bus.map_mmio(retroshelf::EeVif1Fifo::RegisterBase,
                       retroshelf::EeVif1Fifo::RegisterSize, fifo, &error),
          "map VIF1 FIFO");
    check(bus.map_mmio(retroshelf::EeDmacGlobalRegisters::RegisterBase,
                       retroshelf::EeDmacGlobalRegisters::RegisterSize, global, &error),
          "map DMAC global registers");
    check(bus.map_mmio(retroshelf::EeVif1DmacChannel1::RegisterBase,
                       retroshelf::EeVif1DmacChannel1::RegisterSize, dma, &error),
          "map VIF1 DMA channel 1");

    constexpr auto dmac = retroshelf::EeDmacGlobalRegisters::RegisterBase;
    constexpr auto channel = retroshelf::EeVif1DmacChannel1::RegisterBase;
    write32(bus, dmac + retroshelf::EeDmacGlobalRegisters::DmaControlOffset,
            retroshelf::EeDmacGlobalRegisters::DmaEnable);
    write32(bus, channel + retroshelf::EeVif1DmacChannel1::MadrOffset, 0U);
    write32(bus, channel + retroshelf::EeVif1DmacChannel1::QwcOffset, 1U);
    write32(bus, channel + retroshelf::EeVif1DmacChannel1::ChcrOffset,
            retroshelf::EeVif1DmacChannel1::StartBit);
    check((read32(bus, channel + retroshelf::EeVif1DmacChannel1::ChcrOffset) &
           retroshelf::EeVif1DmacChannel1::StartBit) == 0U,
          "normal VIF1 DMA clears STR after completion");
    check(read32(bus, channel + retroshelf::EeVif1DmacChannel1::MadrOffset) == 16U &&
          read32(bus, channel + retroshelf::EeVif1DmacChannel1::QwcOffset) == 0U,
          "normal VIF1 DMA advances MADR and consumes QWC");
    check((global->status() & (1U << 1U)) != 0U,
          "VIF1 DMA completion sets D_STAT channel 1 flag");
    check(fifo->count() == 0U &&
          (vif->status() & retroshelf::EeVif1Registers::StatEr1) == 0U,
          "DMA quadword reaches decoder and drains FIFO without VIF error");

    // A source-chain request must fail explicitly rather than be mistaken for
    // a normal transfer; the global bus-error source is asserted.
    write32(bus, channel + retroshelf::EeVif1DmacChannel1::MadrOffset, 0U);
    write32(bus, channel + retroshelf::EeVif1DmacChannel1::QwcOffset, 1U);
    std::vector<std::uint8_t> chain_chcr(4U);
    const std::uint32_t chain_value = retroshelf::EeVif1DmacChannel1::StartBit | (1U << 2U);
    for (std::size_t i = 0; i < 4U; ++i)
        chain_chcr[i] = static_cast<std::uint8_t>(chain_value >> (i * 8U));
    const bool chain_write_ok = bus.write(channel + retroshelf::EeVif1DmacChannel1::ChcrOffset,
                                          chain_chcr, &error);
    check(!chain_write_ok && (global->status() & retroshelf::EeDmacGlobalRegisters::BusErrorFlag) != 0U &&
          (read32(bus, channel + retroshelf::EeVif1DmacChannel1::ChcrOffset) &
           retroshelf::EeVif1DmacChannel1::StartBit) == 0U,
          "unsupported VIF1 chain mode fails closed and raises DMAC bus error");
    return failures == 0 ? 0 : 1;
}
