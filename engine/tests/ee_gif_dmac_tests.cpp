#include "retroshelf/ee_gif_dmac.hpp"
#include "retroshelf/system_bus.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void write_u64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8U));
}
void write_u32(retroshelf::SystemBus& bus, std::uint32_t address, std::uint32_t value) {
    std::vector<std::uint8_t> bytes(4);
    for (std::size_t i = 0; i < 4; ++i)
        bytes[i] = static_cast<std::uint8_t>(value >> (i * 8U));
    std::string error;
    if (!bus.write(address, bytes, &error)) throw std::runtime_error(error);
}
std::uint32_t read_u32(retroshelf::SystemBus& bus, std::uint32_t address) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!bus.read(address, 4, bytes, &error)) throw std::runtime_error(error);
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(bytes[i]) << (i * 8U);
    return value;
}
}

int main() {
    int failures = 0;
    const auto check = [&failures](bool ok, const char* label) {
        if (!ok) {
            std::cerr << "FAIL: " << label << "\n";
            ++failures;
        } else {
            std::cout << "PASS: " << label << "\n";
        }
    };

    // Five packed GIF writes draw a red triangle through the actual runtime.
    std::vector<std::uint8_t> ram(512, 0);
    const std::uint8_t addresses[] = {0x0, 0x1, 0x5, 0x5, 0x5};
    const std::uint64_t values[] = {
        0x3ULL, 0xFF0000FFULL,
        (static_cast<std::uint64_t>(1U << 4U) << 16U) | (1U << 4U),
        (static_cast<std::uint64_t>(1U << 4U) << 16U) | (6U << 4U),
        (static_cast<std::uint64_t>(6U << 4U) << 16U) | (1U << 4U)
    };
    for (std::size_t i = 0; i < 5; ++i) {
        const std::size_t offset = i * 32U;
        write_u64(ram, offset, 1ULL | (i == 4 ? (1ULL << 15U) : 0ULL) |
                                  (1ULL << 60U));
        write_u64(ram, offset + 8U, addresses[i]);
        write_u64(ram, offset + 16U, values[i]);
    }

    retroshelf::EeGifGsRuntime runtime(ram, 8, 8);
    auto globals = std::make_shared<retroshelf::EeDmacGlobalRegisters>();
    auto channel = std::make_shared<retroshelf::EeGifDmacChannel2>(runtime, *globals);
    retroshelf::SystemBus execution_bus(ram);
    std::string error;
    check(execution_bus.map_mmio(retroshelf::EeGifDmacChannel2::RegisterBase,
                                 retroshelf::EeGifDmacChannel2::RegisterSize,
                                 channel, &error),
          "map GIF channel 2 register window");
    check(execution_bus.map_mmio(retroshelf::EeDmacGlobalRegisters::RegisterBase,
                                 retroshelf::EeDmacGlobalRegisters::RegisterSize,
                                 globals, &error),
          "map DMAC global register window");

    write_u32(execution_bus, retroshelf::EeDmacGlobalRegisters::RegisterBase, 1U);
    write_u32(execution_bus, retroshelf::EeGifDmacChannel2::RegisterBase + 0x10U, 0U);
    write_u32(execution_bus, retroshelf::EeGifDmacChannel2::RegisterBase + 0x20U, 10U);
    write_u32(execution_bus, retroshelf::EeGifDmacChannel2::RegisterBase, 1U << 8U);

    check(runtime.dma().state() == retroshelf::EeGifDma::State::Completed,
          "guest CHCR start completes the GIF DMA transfer");
    check(read_u32(execution_bus, retroshelf::EeDmacGlobalRegisters::RegisterBase + 0x10U) & (1U << 2U),
          "GIF channel completion sets D_STAT channel 2 flag");
    retroshelf::GsFramebuffer::Rgba8 pixel{};
    check(runtime.framebuffer().get_pixel(2, 2, pixel) && pixel.r == 255 &&
              pixel.g == 0 && pixel.b == 0 && pixel.a == 255,
          "GIF DMA data reaches GS framebuffer renderer");

    // Enable channel-2's D_STAT mask; INT1 becomes pending only after both
    // the channel status flag and its mask are set.
    write_u32(execution_bus, retroshelf::EeDmacGlobalRegisters::RegisterBase + 0x10U,
              1U << (16U + 2U));
    check(globals->int1_pending(), "D_STAT channel-2 mask asserts DMAC INT1");
    write_u32(execution_bus, retroshelf::EeDmacGlobalRegisters::RegisterBase + 0x10U,
              1U << 2U);
    check(!globals->int1_pending(), "D_STAT write-one-to-clear deasserts channel interrupt");

    // Global DMA disable must reject a new transfer and leave STR clear.
    write_u32(execution_bus, retroshelf::EeDmacGlobalRegisters::RegisterBase, 0U);
    write_u32(execution_bus, retroshelf::EeGifDmacChannel2::RegisterBase, 1U << 8U);
    check((read_u32(execution_bus, retroshelf::EeGifDmacChannel2::RegisterBase) & (1U << 8U)) == 0U,
          "globally disabled DMAC rejects channel start and clears STR");

    return failures == 0 ? 0 : 1;
}
