#include "retroshelf/cpu.hpp"
#include "retroshelf/ee_gif_dmac.hpp"
#include "retroshelf/ps2_ee_memory_map.hpp"
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
void write_instruction(retroshelf::Ps2EeMemoryMap& memory, std::uint32_t address,
                       std::uint32_t instruction) {
    std::string error;
    if (!memory.write(address, 4, instruction, &error)) throw std::runtime_error(error);
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
std::uint32_t itype(std::uint32_t op, std::uint32_t rs,
                    std::uint32_t rt, std::uint16_t imm) {
    return (op << 26U) | (rs << 21U) | (rt << 16U) | imm;
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
    std::vector<std::uint8_t> stream(512, 0);
    const std::uint8_t addresses[] = {0x0, 0x1, 0x5, 0x5, 0x5};
    const std::uint64_t values[] = {
        0x3ULL, 0xFF0000FFULL,
        (static_cast<std::uint64_t>(1U << 4U) << 16U) | (1U << 4U),
        (static_cast<std::uint64_t>(1U << 4U) << 16U) | (6U << 4U),
        (static_cast<std::uint64_t>(6U << 4U) << 16U) | (1U << 4U)
    };
    for (std::size_t i = 0; i < 5; ++i) {
        const std::size_t offset = i * 32U;
        write_u64(stream, offset, 1ULL | (i == 4 ? (1ULL << 15U) : 0ULL) |
                                  (1ULL << 60U));
        write_u64(stream, offset + 8U, addresses[i]);
        write_u64(stream, offset + 16U, values[i]);
    }

    retroshelf::Ps2EeMemoryMap memory(512);
    memory.main_ram() = stream;
    retroshelf::EeGifGsRuntime runtime(memory, 8, 8);
    auto globals = std::make_shared<retroshelf::EeDmacGlobalRegisters>();
    auto channel = std::make_shared<retroshelf::EeGifDmacChannel2>(runtime, *globals);
    retroshelf::SystemBus bus(memory);
    std::string error;
    check(bus.map_mmio(retroshelf::EeGifDmacChannel2::RegisterBase,
                       retroshelf::EeGifDmacChannel2::RegisterSize, channel, &error),
          "map GIF channel 2 register window");
    check(bus.map_mmio(retroshelf::EeDmacGlobalRegisters::RegisterBase,
                       retroshelf::EeDmacGlobalRegisters::RegisterSize, globals, &error),
          "map DMAC global register window");

    // Execute guest MIPS instructions that configure D_CTRL, D2_MADR,
    // D2_QWC and D2_CHCR. This tests the CPU -> SystemBus -> DMAC -> GIF -> GS
    // path, rather than invoking the device's start method directly.
    const std::uint32_t program_base = 0x100U;
    const std::uint32_t program[] = {
        itype(0x0FU, 0U, 1U, 0x1000U),       // LUI r1, 0x1000
        itype(0x0DU, 1U, 1U, 0xA000U),       // ORI r1, r1, 0xA000 (GIF ch2)
        itype(0x0FU, 0U, 4U, 0x1000U),       // LUI r4, 0x1000
        itype(0x0DU, 4U, 4U, 0xE000U),       // ORI r4, r4, 0xE000 (DMAC globals)
        itype(0x0DU, 0U, 2U, 1U),            // ORI r2, zero, DMAE
        itype(0x2BU, 4U, 2U, 0x0000U),       // SW r2, D_CTRL(r4)
        itype(0x0DU, 0U, 2U, 0U),            // MADR = 0
        itype(0x2BU, 1U, 2U, 0x0010U),       // SW r2, D2_MADR(r1)
        itype(0x0DU, 0U, 2U, 10U),           // QWC = 10
        itype(0x2BU, 1U, 2U, 0x0020U),       // SW r2, D2_QWC(r1)
        itype(0x0DU, 0U, 2U, 0x0100U),       // CHCR.STR = 1
        itype(0x2BU, 1U, 2U, 0x0000U)        // SW r2, D2_CHCR(r1)
    };
    for (std::size_t i = 0; i < sizeof(program) / sizeof(program[0]); ++i)
        write_instruction(memory, program_base + static_cast<std::uint32_t>(i * 4U),
                          program[i]);

    retroshelf::Cpu cpu;
    cpu.set_pc(program_base);
    for (std::size_t i = 0; i < sizeof(program) / sizeof(program[0]); ++i) {
        if (!cpu.step_from_ee_bus(memory, bus, &error)) {
            std::cerr << "CPU MMIO program stopped: " << error << "\n";
            ++failures;
            break;
        }
    }

    check(runtime.dma().state() == retroshelf::EeGifDma::State::Completed,
          "guest CPU CHCR write completes GIF DMA");
    check((read_u32(bus, retroshelf::EeDmacGlobalRegisters::RegisterBase + 0x10U) &
           (1U << 2U)) != 0U,
          "GIF completion sets D_STAT channel 2 flag");
    retroshelf::GsFramebuffer::Rgba8 pixel{};
    check(runtime.framebuffer().get_pixel(2, 2, pixel) && pixel.r == 255 &&
              pixel.g == 0 && pixel.b == 0 && pixel.a == 255,
          "guest CPU DMA payload reaches GS framebuffer renderer");

    // Enable channel-2's D_STAT mask; INT1 becomes pending only after both
    // the channel status flag and its mask are set.
    const auto write_mmio = [&bus](std::uint32_t address, std::uint32_t value) {
        std::vector<std::uint8_t> data(4);
        for (std::size_t i = 0; i < 4; ++i)
            data[i] = static_cast<std::uint8_t>(value >> (i * 8U));
        std::string write_error;
        if (!bus.write(address, data, &write_error)) throw std::runtime_error(write_error);
    };
    write_mmio(retroshelf::EeDmacGlobalRegisters::RegisterBase + 0x10U,
               1U << (16U + 2U));
    check(globals->int1_pending(), "D_STAT channel-2 mask asserts DMAC INT1");
    write_mmio(retroshelf::EeDmacGlobalRegisters::RegisterBase + 0x10U, 1U << 2U);
    check(!globals->int1_pending(), "D_STAT write-one-to-clear deasserts channel interrupt");

    // Global DMA disable must reject a new transfer and leave STR clear.
    write_mmio(retroshelf::EeDmacGlobalRegisters::RegisterBase, 0U);
    write_mmio(retroshelf::EeGifDmacChannel2::RegisterBase, 1U << 8U);
    check((read_u32(bus, retroshelf::EeGifDmacChannel2::RegisterBase) &
           (1U << 8U)) == 0U,
          "globally disabled DMAC rejects channel start and clears STR");

    return failures == 0 ? 0 : 1;
}
