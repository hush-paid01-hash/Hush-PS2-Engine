#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace retroshelf {

// Bounded EE memory foundation with main RAM aliases, BIOS ROM, scratchpad,
// and the IOP memory-control register window. Other MMIO and TLB behavior
// remain outside this memory-map model.
class Ps2EeMemoryMap {
public:
    static constexpr std::size_t MainRamSize = 32U * 1024U * 1024U;
    static constexpr std::uint32_t MainRamBase = 0x00000000U;
    static constexpr std::uint32_t MainRamMirrorBase = 0x20000000U;
    static constexpr std::uint32_t MainRamAcceleratedBase = 0x30100000U;
    static constexpr std::uint32_t MainRamAcceleratedEnd = 0x32000000U;
    static constexpr std::uint32_t Kseg0Base = 0x80000000U;
    static constexpr std::uint32_t Kseg1Base = 0xA0000000U;
    static constexpr std::uint32_t SegmentMask = 0x1FFFFFFFU;
    static constexpr std::uint32_t BiosRomBase = 0x1FC00000U;
    static constexpr std::size_t BiosRomWindowSize = 4U * 1024U * 1024U;
    static constexpr std::uint32_t ScratchpadBase = 0x70000000U;
    static constexpr std::size_t ScratchpadSize = 16U * 1024U;
    static constexpr std::uint32_t IopMemoryControlBase = 0x1F801000U;
    static constexpr std::size_t IopMemoryControlSize = 9U * sizeof(std::uint32_t);
    static constexpr std::uint32_t IopPost2Address = 0x1F802070U;
    static constexpr std::uint32_t IopPost3Address = 0x1FA00000U;
    static constexpr std::uint32_t IopDmaControlBase = 0x1F8010F0U;
    static constexpr std::size_t IopDmaControlSize = 2U * sizeof(std::uint32_t);
    static constexpr std::uint32_t IopDmaControl2Base = 0x1F801570U;
    static constexpr std::size_t IopDmaControl2Size = 4U * sizeof(std::uint32_t);
    static constexpr std::uint32_t IopSpuRegisterBase = 0x1F801C00U;
    static constexpr std::size_t IopSpuRegisterSize = 0x200U;
    static constexpr std::uint32_t IopSio2Base = 0x1F808200U;
    static constexpr std::size_t IopSio2Size = 0x78U;
    static constexpr std::uint32_t EeCacheControlAddress = 0xFFFE0130U;
    static constexpr std::uint32_t IopRamSizeAddress = 0x1F801060U;

    Ps2EeMemoryMap();
    explicit Ps2EeMemoryMap(std::size_t main_ram_size);

    std::size_t main_ram_size() const;
    std::vector<std::uint8_t>& main_ram();
    const std::vector<std::uint8_t>& main_ram() const;

    // Install a read-only BIOS image in the EE physical BIOS window. This only
    // maps bytes; it does not authenticate or execute firmware.
    bool load_bios_image(const std::vector<std::uint8_t>& image,
                         std::string* error = nullptr);
    std::size_t bios_rom_size() const;
    std::size_t scratchpad_size() const;
    std::uint32_t iop_post2_last_value() const;
    std::size_t iop_post2_write_count() const;
    std::uint32_t iop_post3_last_value() const;
    std::size_t iop_post3_write_count() const;

    // Resolve a virtual EE address to a main-RAM offset. False means the
    // address is not in one of the supported RAM aliases.
    bool resolve_main_ram(std::uint32_t address, std::size_t width,
                          std::size_t alignment, std::size_t& offset,
                          std::string* error = nullptr) const;

    bool read(std::uint32_t address, std::size_t width, std::uint64_t& value,
              std::string* error = nullptr) const;
    bool write(std::uint32_t address, std::size_t width, std::uint64_t value,
               std::string* error = nullptr);
    bool read128(std::uint32_t address, std::uint64_t& low, std::uint64_t& high,
                 std::string* error = nullptr) const;
    bool write128(std::uint32_t address, std::uint64_t low, std::uint64_t high,
                  std::string* error = nullptr);

private:
    bool resolve_bios_rom(std::uint32_t address, std::size_t width,
                          std::size_t& offset, std::string* error) const;
    bool resolve_scratchpad(std::uint32_t address, std::size_t width,
                            std::size_t& offset, std::string* error) const;

    std::vector<std::uint8_t> ram_;
    std::vector<std::uint8_t> bios_rom_;
    std::vector<std::uint8_t> scratchpad_;
    // IOP memory-control register reset values measured on PS2 hardware.
    std::array<std::uint32_t, 9> iop_memory_control_{{
        0x1F000000U, 0x1F802000U, 0x0013243FU, 0x00003022U,
        0x0013243FU, 0x200931E1U, 0x00020843U, 0x00070777U, 0x00031125U
    }};
    std::uint32_t iop_post2_last_value_{0};
    std::size_t iop_post2_write_count_{0};
    std::uint32_t iop_post3_last_value_{0};
    std::size_t iop_post3_write_count_{0};
    std::array<std::uint32_t, 2> iop_dma_control_{};
    std::array<std::uint32_t, 4> iop_dma_control2_{};
    std::array<std::uint8_t, IopSpuRegisterSize> iop_spu_registers_{};
    std::array<std::uint8_t, IopSio2Size> iop_sio2_registers_{};
    std::uint32_t ee_cache_control_{0x0001E988U};
};

} // namespace retroshelf
