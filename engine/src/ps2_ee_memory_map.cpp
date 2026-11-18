#include "retroshelf/ps2_ee_memory_map.hpp"

#include <utility>

namespace retroshelf {
namespace {
bool is_bios_window_address(std::uint32_t address) {
    const std::uint32_t physical = address >= Ps2EeMemoryMap::Kseg0Base && address < 0xC0000000U
        ? address & Ps2EeMemoryMap::SegmentMask : address;
    return physical >= Ps2EeMemoryMap::BiosRomBase &&
           physical < Ps2EeMemoryMap::BiosRomBase + Ps2EeMemoryMap::BiosRomWindowSize;
}
} // namespace

Ps2EeMemoryMap::Ps2EeMemoryMap()
    : ram_(MainRamSize, 0), iop_ram_(IopRamSize, 0), scratchpad_(ScratchpadSize, 0) {
    ee_sio_registers_[0x10] = 0x60U; // transmitter holding/shift registers empty
    ee_sio_registers_[0x30] = 0x02U; // transmit FIFO empty
}
Ps2EeMemoryMap::Ps2EeMemoryMap(std::size_t main_ram_size)
    : ram_(main_ram_size, 0), iop_ram_(IopRamSize, 0), scratchpad_(ScratchpadSize, 0) {
    ee_sio_registers_[0x10] = 0x60U;
    ee_sio_registers_[0x30] = 0x02U;
}
std::size_t Ps2EeMemoryMap::main_ram_size() const { return ram_.size(); }
std::vector<std::uint8_t>& Ps2EeMemoryMap::main_ram() { return ram_; }
const std::vector<std::uint8_t>& Ps2EeMemoryMap::main_ram() const { return ram_; }

bool Ps2EeMemoryMap::load_bios_image(const std::vector<std::uint8_t>& image,
                                     std::string* error) {
    if (image.empty()) {
        if (error) *error = "BIOS image is empty";
        return false;
    }
    if (image.size() > BiosRomWindowSize) {
        if (error) *error = "BIOS image exceeds the 4 MiB EE BIOS ROM window";
        return false;
    }
    bios_rom_ = image;
    if (error) error->clear();
    return true;
}

std::size_t Ps2EeMemoryMap::bios_rom_size() const { return bios_rom_.size(); }
std::size_t Ps2EeMemoryMap::scratchpad_size() const { return scratchpad_.size(); }
std::uint32_t Ps2EeMemoryMap::iop_post2_last_value() const { return iop_post2_last_value_; }
std::size_t Ps2EeMemoryMap::iop_post2_write_count() const { return iop_post2_write_count_; }
std::uint32_t Ps2EeMemoryMap::iop_post3_last_value() const { return iop_post3_last_value_; }
std::size_t Ps2EeMemoryMap::iop_post3_write_count() const { return iop_post3_write_count_; }
void Ps2EeMemoryMap::raise_iop_interrupt(std::uint16_t bits) {
    iop_interrupt_status_ = static_cast<std::uint16_t>(iop_interrupt_status_ | bits);
}
std::uint16_t Ps2EeMemoryMap::iop_interrupt_status() const { return iop_interrupt_status_; }
std::uint16_t Ps2EeMemoryMap::iop_interrupt_mask() const { return iop_interrupt_mask_; }

bool Ps2EeMemoryMap::resolve_scratchpad(std::uint32_t address, std::size_t width,
                                        std::size_t& offset, std::string* error) const {
    if (address < ScratchpadBase ||
        static_cast<std::uint64_t>(address) >=
            static_cast<std::uint64_t>(ScratchpadBase) + ScratchpadSize)
        return false;
    offset = static_cast<std::size_t>(address - ScratchpadBase);
    if (width > scratchpad_.size() || offset > scratchpad_.size() - width) {
        if (error) *error = "Memory access exceeds the EE scratchpad boundary";
        return false;
    }
    if (error) error->clear();
    return true;
}

bool Ps2EeMemoryMap::resolve_bios_rom(std::uint32_t address, std::size_t width,
                                      std::size_t& offset, std::string* error) const {
    std::uint32_t physical = address;
    if (address >= Kseg0Base && address < 0xC0000000U)
        physical = address & SegmentMask;
    if (physical < BiosRomBase || physical >= BiosRomBase + BiosRomWindowSize)
        return false;
    offset = static_cast<std::size_t>(physical - BiosRomBase);
    if (bios_rom_.empty()) {
        if (error) *error = "No BIOS image is mapped";
        return false;
    }
    if (width > bios_rom_.size() || offset > bios_rom_.size() - width) {
        if (error) *error = "BIOS read exceeds the loaded ROM image";
        return false;
    }
    if (error) error->clear();
    return true;
}

bool Ps2EeMemoryMap::resolve_main_ram(std::uint32_t address, std::size_t width,
                                      std::size_t alignment, std::size_t& offset,
                                      std::string* error) const {
    const auto fail = [error](const char* message) {
        if (error) *error = message;
        return false;
    };
    if (width != 1 && width != 2 && width != 4 && width != 8 && width != 16)
        return fail("Unsupported memory access width");
    if (alignment == 0 || (alignment & (alignment - 1U)) != 0)
        return fail("Alignment must be a nonzero power of two");

    std::uint32_t physical = address;
    // KSEG0/KSEG1 are direct-mapped cached/uncached aliases in the 32-bit EE view.
    if ((address >= Kseg0Base && address < 0xC0000000U))
        physical = address & SegmentMask;

    // The accelerated uncached window starts at 0x30100000 and maps
    // physical RAM from offset 1 MiB through the end of the 32 MiB bank.
    // It is a distinct alias, not a modulo mirror of the low 1 MiB.
    if (physical >= MainRamAcceleratedBase &&
        physical < MainRamAcceleratedEnd) {
        offset = static_cast<std::size_t>(physical - 0x30000000U);
    } else {
        if (physical >= MainRamMirrorBase &&
            physical < MainRamMirrorBase + MainRamSize)
            physical -= MainRamMirrorBase;

        if (physical >= MainRamBase && physical < MainRamSize) {
            offset = static_cast<std::size_t>(physical);
        } else {
            return fail("Address is not in a supported EE main-RAM region");
        }
    }
    if ((address & static_cast<std::uint32_t>(alignment - 1U)) != 0U)
        return fail("Misaligned memory access");
    if (width > ram_.size() || offset > ram_.size() - width)
        return fail("Memory access exceeds configured main RAM");
    if (error) error->clear();
    return true;
}

bool Ps2EeMemoryMap::read(std::uint32_t address, std::size_t width,
                          std::uint64_t& value, std::string* error) const {
    if (width != 1 && width != 2 && width != 4 && width != 8) {
        if (error) *error = "Unsupported memory access width";
        return false;
    }
    if ((address & static_cast<std::uint32_t>(width - 1U)) != 0U) {
        if (error) *error = "Misaligned memory access";
        return false;
    }
    std::size_t offset = 0;
    if (address >= ScratchpadBase &&
        static_cast<std::uint64_t>(address) <
            static_cast<std::uint64_t>(ScratchpadBase) + ScratchpadSize) {
        if (!resolve_scratchpad(address, width, offset, error)) return false;
        value = 0;
        for (std::size_t i = 0; i < width; ++i)
            value |= static_cast<std::uint64_t>(scratchpad_[offset + i]) << (8U * i);
        if (error) error->clear();
        return true;
    }
    if (is_bios_window_address(address)) {
        if (!resolve_bios_rom(address, width, offset, error)) return false;
        value = 0;
        for (std::size_t i = 0; i < width; ++i)
            value |= static_cast<std::uint64_t>(bios_rom_[offset + i]) << (8U * i);
        if (error) error->clear();
        return true;
    }
    const std::uint32_t physical = address >= Kseg0Base && address < 0xC0000000U
        ? address & SegmentMask : address;
    const auto read_iop_irq_register = [&](std::uint32_t base, std::uint16_t reg) {
        if (physical == base && width == 4U) {
            value = reg;
            return true;
        }
        if (physical >= base &&
            static_cast<std::uint64_t>(physical - base) + width <= sizeof(reg)) {
            const std::size_t byte_offset = physical - base;
            value = 0;
            for (std::size_t i = 0; i < width; ++i)
                value |= static_cast<std::uint64_t>((reg >> ((byte_offset + i) * 8U)) & 0xFFU) << (i * 8U);
            return true;
        }
        return false;
    };
    if (read_iop_irq_register(IopInterruptStatusAddress, iop_interrupt_status_) ||
        read_iop_irq_register(IopInterruptMaskAddress, iop_interrupt_mask_)) {
        if (error) error->clear();
        return true;
    }
    if (physical == EeMchRicmAddress && width == 4U) {
        value = 0;
        if (error) error->clear();
        return true;
    }
    if (physical == EeMchDrdAddress && width == 4U) {
        const std::uint32_t sop = (ee_mch_ricm_ >> 6U) & 0xFU;
        const std::uint32_t sa = (ee_mch_ricm_ >> 16U) & 0xFFFU;
        value = 0;
        if (sop == 0U) {
            switch (sa) {
            case 0x21U:
                if (ee_rdram_sdevid_ < 2U) {
                    ++ee_rdram_sdevid_;
                    value = 0x1FU;
                }
                break;
            case 0x23U: value = 0x0D0DU; break;
            case 0x24U: value = 0x0090U; break;
            case 0x40U: value = ee_mch_ricm_ & 0x1FU; break;
            default: break;
            }
        }
        if (error) error->clear();
        return true;
    }
    if (physical == EeRdramControllerStatusAddress && width == 4U) {
        // F410 is undocumented. Existing PS2 hardware emulation models reads
        // as zero; do not invent a one-read busy handshake for this register.
        value = 0;
        if (error) error->clear();
        return true;
    }
    if (physical >= EeRdramControllerBase &&
        static_cast<std::uint64_t>(physical - EeRdramControllerBase) + width <= EeRdramControllerSize) {
        const std::size_t offset = physical - EeRdramControllerBase;
        value = 0;
        for (std::size_t i = 0; i < width; ++i)
            value |= static_cast<std::uint64_t>(ee_rdram_controller_registers_[offset + i]) << (8U * i);
        if (error) error->clear();
        return true;
    }
    if (physical == EeMemoryControllerControlAddress && width == 4U) {
        value = ee_memory_controller_control_;
        if (error) error->clear();
        return true;
    }
    if (physical == EeDmacEnableStatusAddress && width == 4U) {
        value = 0x00001201U | (ee_dmac_disabled_ ? 0x00010000U : 0U);
        if (error) error->clear();
        return true;
    }
    if (physical >= EeSioRegisterBase &&
        static_cast<std::uint64_t>(physical - EeSioRegisterBase) + width <= EeSioRegisterSize) {
        const std::size_t offset = physical - EeSioRegisterBase;
        value = 0;
        for (std::size_t i = 0; i < width; ++i)
            value |= static_cast<std::uint64_t>(ee_sio_registers_[offset + i]) << (8U * i);
        if (error) error->clear();
        return true;
    }
    if (physical >= IopRamBase &&
        static_cast<std::uint64_t>(physical - IopRamBase) + width <= IopRamSize) {
        const std::size_t iop_offset = physical - IopRamBase;
        value = 0;
        for (std::size_t i = 0; i < width; ++i)
            value |= static_cast<std::uint64_t>(iop_ram_[iop_offset + i]) << (8U * i);
        if (error) error->clear();
        return true;
    }
    const bool ee_sif_window = physical >= EeSifRegisterBase &&
        static_cast<std::uint64_t>(physical - EeSifRegisterBase) + width <= SifRegisterSize;
    const bool iop_sif_window = physical >= IopSifRegisterBase &&
        static_cast<std::uint64_t>(physical - IopSifRegisterBase) + width <= SifRegisterSize;
    if (ee_sif_window || iop_sif_window) {
        const std::size_t sif_offset = static_cast<std::size_t>(
            physical - (ee_sif_window ? EeSifRegisterBase : IopSifRegisterBase));
        value = 0;
        for (std::size_t i = 0; i < width; ++i)
            value |= static_cast<std::uint64_t>(sif_registers_[sif_offset + i]) << (8U * i);
        if (error) error->clear();
        return true;
    }
    if (address == EeCacheControlAddress) {
        value = ee_cache_control_;
        if (error) error->clear();
        return true;
    }
    if (physical == IopRamSizeAddress) {
        // The high half is bus-latched differently through KUSEG vs KSEG0/1.
        value = address >= Kseg0Base && address < 0xC0000000U
            ? 0x03E00B88U : 0x00000B88U;
        if (error) error->clear();
        return true;
    }
    if (physical == IopPost2Address || physical == IopPost3Address) {
        if (error) *error = "IOP POST diagnostic ports are write-only";
        return false;
    }
    if (physical >= IopLegacySpuRegisterBase &&
        static_cast<std::uint64_t>(physical - IopLegacySpuRegisterBase) + width <= IopLegacySpuRegisterSize) {
        const std::size_t register_offset = physical - IopLegacySpuRegisterBase;
        value = 0;
        for (std::size_t i = 0; i < width; ++i)
            value |= static_cast<std::uint64_t>(iop_legacy_spu_registers_[register_offset + i]) << (8U * i);
        if (error) error->clear();
        return true;
    }
    if (physical >= IopSpuRegisterBase &&
        static_cast<std::uint64_t>(physical - IopSpuRegisterBase) + width <= IopSpuRegisterSize) {
        const std::size_t register_offset = physical - IopSpuRegisterBase;
        value = 0;
        for (std::size_t i = 0; i < width; ++i)
            value |= static_cast<std::uint64_t>(iop_spu_registers_[register_offset + i]) << (8U * i);
        if (error) error->clear();
        return true;
    }
    if (physical >= IopSio2Base &&
        static_cast<std::uint64_t>(physical - IopSio2Base) + width <= IopSio2Size) {
        const std::size_t register_offset = physical - IopSio2Base;
        value = 0;
        for (std::size_t i = 0; i < width; ++i)
            value |= static_cast<std::uint64_t>(iop_sio2_registers_[register_offset + i]) << (8U * i);
        if (error) error->clear();
        return true;
    }
    if (physical >= IopDmaControlBase &&
        static_cast<std::uint64_t>(physical - IopDmaControlBase) + width <= IopDmaControlSize) {
        const std::size_t register_offset = physical - IopDmaControlBase;
        value = 0;
        for (std::size_t i = 0; i < width; ++i) {
            const std::size_t byte_offset = register_offset + i;
            const std::uint32_t word = iop_dma_control_[byte_offset / 4U];
            value |= static_cast<std::uint64_t>((word >> ((byte_offset % 4U) * 8U)) & 0xFFU) << (8U * i);
        }
        if (error) error->clear();
        return true;
    }
    if (physical >= IopDmaControl2Base &&
        static_cast<std::uint64_t>(physical - IopDmaControl2Base) + width <= IopDmaControl2Size) {
        const std::size_t register_offset = physical - IopDmaControl2Base;
        value = 0;
        for (std::size_t i = 0; i < width; ++i) {
            const std::size_t byte_offset = register_offset + i;
            const std::uint32_t word = iop_dma_control2_[byte_offset / 4U];
            value |= static_cast<std::uint64_t>((word >> ((byte_offset % 4U) * 8U)) & 0xFFU) << (8U * i);
        }
        if (error) error->clear();
        return true;
    }
    if (physical >= IopMemoryControlBase &&
        static_cast<std::uint64_t>(physical - IopMemoryControlBase) + width <= IopMemoryControlSize) {
        const std::size_t register_offset = physical - IopMemoryControlBase;
        value = 0;
        for (std::size_t i = 0; i < width; ++i) {
            const std::size_t byte_offset = register_offset + i;
            const std::uint32_t word = iop_memory_control_[byte_offset / 4U];
            value |= static_cast<std::uint64_t>((word >> ((byte_offset % 4U) * 8U)) & 0xFFU) << (8U * i);
        }
        if (error) error->clear();
        return true;
    }
    std::size_t ram_offset = 0;
    if (!resolve_main_ram(address, width, width, ram_offset, error)) return false;
    value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint64_t>(ram_[ram_offset + i]) << (8U * i);
    if (error) error->clear();
    return true;
}

bool Ps2EeMemoryMap::write(std::uint32_t address, std::size_t width,
                           std::uint64_t value, std::string* error) {
    if (width != 1 && width != 2 && width != 4 && width != 8) {
        if (error) *error = "Unsupported memory access width";
        return false;
    }
    if ((address & static_cast<std::uint32_t>(width - 1U)) != 0U) {
        if (error) *error = "Misaligned memory access";
        return false;
    }
    std::size_t scratchpad_offset = 0;
    if (address >= ScratchpadBase &&
        static_cast<std::uint64_t>(address) <
            static_cast<std::uint64_t>(ScratchpadBase) + ScratchpadSize) {
        if (!resolve_scratchpad(address, width, scratchpad_offset, error)) return false;
        for (std::size_t i = 0; i < width; ++i)
            scratchpad_[scratchpad_offset + i] =
                static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU);
        if (error) error->clear();
        return true;
    }
    std::size_t bios_offset = 0;
    if (resolve_bios_rom(address, width, bios_offset, nullptr)) {
        if (error) *error = "Writes to the EE BIOS ROM are not permitted";
        return false;
    }
    const auto canonical = [this](std::uint32_t value) {
        return value >= Kseg0Base && value < 0xC0000000U ? value & SegmentMask : value;
    };
    const std::uint32_t physical = canonical(address);
    const auto write_iop_irq_register = [&](std::uint32_t base, std::uint16_t& reg,
                                            bool write_one_to_clear) {
        std::size_t byte_offset = 0;
        std::size_t access_width = width;
        if (physical == base && width == 4U) {
            access_width = sizeof(reg);
        } else if (physical >= base &&
                   static_cast<std::uint64_t>(physical - base) + width <= sizeof(reg)) {
            byte_offset = physical - base;
        } else {
            return false;
        }
        std::uint16_t written_bits = 0;
        for (std::size_t i = 0; i < access_width; ++i)
            written_bits |= static_cast<std::uint16_t>(((value >> (i * 8U)) & 0xFFU) << ((byte_offset + i) * 8U));
        if (write_one_to_clear) {
            reg = static_cast<std::uint16_t>(reg & ~written_bits);
        } else {
            const std::uint16_t mask = static_cast<std::uint16_t>(
                ((access_width == 2U ? 0xFFFFU : 0xFFU) << (byte_offset * 8U)));
            reg = static_cast<std::uint16_t>((reg & ~mask) | (written_bits & mask));
        }
        return true;
    };
    if (write_iop_irq_register(IopInterruptStatusAddress, iop_interrupt_status_, true) ||
        write_iop_irq_register(IopInterruptMaskAddress, iop_interrupt_mask_, false)) {
        if (error) error->clear();
        return true;
    }
    if (physical == EeMchRicmAddress && width == 4U) {
        const std::uint32_t sa = (static_cast<std::uint32_t>(value) >> 16U) & 0xFFFU;
        const std::uint32_t sbc = (static_cast<std::uint32_t>(value) >> 6U) & 0xFU;
        if (sa == 0x21U && sbc == 1U && ((ee_mch_drd_ >> 7U) & 1U) == 0U)
            ee_rdram_sdevid_ = 0;
        ee_mch_ricm_ = static_cast<std::uint32_t>(value) & 0x7FFFFFFFU;
        if (error) error->clear();
        return true;
    }
    if (physical == EeMchDrdAddress && width == 4U) {
        ee_mch_drd_ = static_cast<std::uint32_t>(value);
        if (error) error->clear();
        return true;
    }
    if (physical >= EeRdramControllerBase &&
        static_cast<std::uint64_t>(physical - EeRdramControllerBase) + width <= EeRdramControllerSize) {
        const std::size_t offset = physical - EeRdramControllerBase;
        for (std::size_t i = 0; i < width; ++i)
            ee_rdram_controller_registers_[offset + i] = static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU);
        if (error) error->clear();
        return true;
    }
    if (physical == EeMemoryControllerControlAddress && width == 4U) {
        ee_memory_controller_control_ = static_cast<std::uint32_t>(value);
        if (error) error->clear();
        return true;
    }
    if (physical == EeDmacEnableControlAddress && width == 4U) {
        ee_dmac_disabled_ = (static_cast<std::uint32_t>(value) & 0x00010000U) != 0U;
        if (error) error->clear();
        return true;
    }
    if (physical >= EeSioRegisterBase &&
        static_cast<std::uint64_t>(physical - EeSioRegisterBase) + width <= EeSioRegisterSize) {
        const std::size_t offset = physical - EeSioRegisterBase;
        for (std::size_t i = 0; i < width; ++i)
            ee_sio_registers_[offset + i] = static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU);
        if (error) error->clear();
        return true;
    }
    if (physical >= BiosRomBase && physical < BiosRomBase + BiosRomWindowSize) {
        if (error) *error = bios_rom_.empty() ? "No BIOS image is mapped" : "BIOS write exceeds the loaded ROM image";
        return false;
    }
    if (physical >= IopRamBase &&
        static_cast<std::uint64_t>(physical - IopRamBase) + width <= IopRamSize) {
        const std::size_t iop_offset = physical - IopRamBase;
        for (std::size_t i = 0; i < width; ++i)
            iop_ram_[iop_offset + i] =
                static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU);
        if (error) error->clear();
        return true;
    }
    const bool ee_sif_window = physical >= EeSifRegisterBase &&
        static_cast<std::uint64_t>(physical - EeSifRegisterBase) + width <= SifRegisterSize;
    const bool iop_sif_window = physical >= IopSifRegisterBase &&
        static_cast<std::uint64_t>(physical - IopSifRegisterBase) + width <= SifRegisterSize;
    if (ee_sif_window || iop_sif_window) {
        const std::size_t sif_offset = static_cast<std::size_t>(
            physical - (ee_sif_window ? EeSifRegisterBase : IopSifRegisterBase));
        for (std::size_t i = 0; i < width; ++i)
            sif_registers_[sif_offset + i] =
                static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU);
        if (error) error->clear();
        return true;
    }
    if (address == EeCacheControlAddress) {
        ee_cache_control_ = static_cast<std::uint32_t>(value);
        if (error) error->clear();
        return true;
    }
    if (physical == IopRamSizeAddress) {
        // RAM_SIZE is read-only; writes do not alter the installed RAM size.
        if (error) error->clear();
        return true;
    }
    if (physical == IopPost2Address && (width == 1U || width == 4U)) {
        iop_post2_last_value_ = static_cast<std::uint32_t>(value);
        ++iop_post2_write_count_;
        if (error) error->clear();
        return true;
    }
    if (physical == IopPost3Address && (width == 1U || width == 4U)) {
        iop_post3_last_value_ = static_cast<std::uint32_t>(value);
        ++iop_post3_write_count_;
        if (error) error->clear();
        return true;
    }
    if (physical >= IopLegacySpuRegisterBase &&
        static_cast<std::uint64_t>(physical - IopLegacySpuRegisterBase) + width <= IopLegacySpuRegisterSize) {
        const std::size_t register_offset = physical - IopLegacySpuRegisterBase;
        for (std::size_t i = 0; i < width; ++i)
            iop_legacy_spu_registers_[register_offset + i] =
                static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU);
        if (error) error->clear();
        return true;
    }
    if (physical >= IopSpuRegisterBase &&
        static_cast<std::uint64_t>(physical - IopSpuRegisterBase) + width <= IopSpuRegisterSize) {
        const std::size_t register_offset = physical - IopSpuRegisterBase;
        for (std::size_t i = 0; i < width; ++i)
            iop_spu_registers_[register_offset + i] =
                static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU);
        if (error) error->clear();
        return true;
    }
    if (physical >= IopSio2Base &&
        static_cast<std::uint64_t>(physical - IopSio2Base) + width <= IopSio2Size) {
        const std::size_t register_offset = physical - IopSio2Base;
        for (std::size_t i = 0; i < width; ++i)
            iop_sio2_registers_[register_offset + i] =
                static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU);
        if (error) error->clear();
        return true;
    }
    if (physical >= IopDmaControlBase &&
        static_cast<std::uint64_t>(physical - IopDmaControlBase) + width <= IopDmaControlSize) {
        const std::size_t register_offset = physical - IopDmaControlBase;
        for (std::size_t i = 0; i < width; ++i) {
            const std::size_t byte_offset = register_offset + i;
            const std::size_t word_index = byte_offset / 4U;
            const unsigned shift = static_cast<unsigned>(byte_offset % 4U) * 8U;
            const std::uint32_t mask = 0xFFU << shift;
            iop_dma_control_[word_index] = (iop_dma_control_[word_index] & ~mask) |
                (static_cast<std::uint32_t>((value >> (8U * i)) & 0xFFU) << shift);
        }
        if (error) error->clear();
        return true;
    }
    if (physical >= IopDmaControl2Base &&
        static_cast<std::uint64_t>(physical - IopDmaControl2Base) + width <= IopDmaControl2Size) {
        const std::size_t register_offset = physical - IopDmaControl2Base;
        for (std::size_t i = 0; i < width; ++i) {
            const std::size_t byte_offset = register_offset + i;
            const std::size_t word_index = byte_offset / 4U;
            const unsigned shift = static_cast<unsigned>(byte_offset % 4U) * 8U;
            const std::uint32_t mask = 0xFFU << shift;
            iop_dma_control2_[word_index] = (iop_dma_control2_[word_index] & ~mask) |
                (static_cast<std::uint32_t>((value >> (8U * i)) & 0xFFU) << shift);
        }
        if (error) error->clear();
        return true;
    }
    if (physical >= IopMemoryControlBase &&
        static_cast<std::uint64_t>(physical - IopMemoryControlBase) + width <= IopMemoryControlSize) {
        const std::size_t register_offset = physical - IopMemoryControlBase;
        for (std::size_t i = 0; i < width; ++i) {
            const std::size_t byte_offset = register_offset + i;
            const std::size_t word_index = byte_offset / 4U;
            const unsigned shift = static_cast<unsigned>(byte_offset % 4U) * 8U;
            const std::uint32_t mask = 0xFFU << shift;
            iop_memory_control_[word_index] = (iop_memory_control_[word_index] & ~mask) |
                (static_cast<std::uint32_t>((value >> (8U * i)) & 0xFFU) << shift);
        }
        if (error) error->clear();
        return true;
    }
    std::size_t offset = 0;
    if (!resolve_main_ram(address, width, width, offset, error)) return false;
    for (std::size_t i = 0; i < width; ++i)
        ram_[offset + i] = static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU);
    if (error) error->clear();
    return true;
}

bool Ps2EeMemoryMap::read128(std::uint32_t address, std::uint64_t& low,
                             std::uint64_t& high, std::string* error) const {
    if ((address & 0xFU) != 0U) {
        if (error) *error = "Misaligned 128-bit memory access";
        return false;
    }
    std::size_t offset = 0;
    if (address >= ScratchpadBase &&
        static_cast<std::uint64_t>(address) <
            static_cast<std::uint64_t>(ScratchpadBase) + ScratchpadSize) {
        if (!resolve_scratchpad(address, 16, offset, error)) return false;
        low = 0; high = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            low |= static_cast<std::uint64_t>(scratchpad_[offset + i]) << (8U * i);
            high |= static_cast<std::uint64_t>(scratchpad_[offset + 8 + i]) << (8U * i);
        }
        if (error) error->clear();
        return true;
    }
    if (is_bios_window_address(address)) {
        if (!resolve_bios_rom(address, 16, offset, error)) return false;
        low = 0;
        high = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            low |= static_cast<std::uint64_t>(bios_rom_[offset + i]) << (8U * i);
            high |= static_cast<std::uint64_t>(bios_rom_[offset + 8 + i]) << (8U * i);
        }
        if (error) error->clear();
        return true;
    }
    const std::uint32_t physical = address >= Kseg0Base && address < 0xC0000000U
        ? address & SegmentMask : address;
    if (physical >= IopRamBase &&
        static_cast<std::uint64_t>(physical - IopRamBase) + 16U <= IopRamSize) {
        offset = physical - IopRamBase;
        low = 0;
        high = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            low |= static_cast<std::uint64_t>(iop_ram_[offset + i]) << (8U * i);
            high |= static_cast<std::uint64_t>(iop_ram_[offset + 8U + i]) << (8U * i);
        }
        if (error) error->clear();
        return true;
    }
    if (!resolve_main_ram(address, 16, 16, offset, error)) return false;
    low = 0;
    high = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        low |= static_cast<std::uint64_t>(ram_[offset + i]) << (8U * i);
        high |= static_cast<std::uint64_t>(ram_[offset + 8 + i]) << (8U * i);
    }
    if (error) error->clear();
    return true;
}

bool Ps2EeMemoryMap::write128(std::uint32_t address, std::uint64_t low,
                              std::uint64_t high, std::string* error) {
    if ((address & 0xFU) != 0U) {
        if (error) *error = "Misaligned 128-bit memory access";
        return false;
    }
    std::size_t scratchpad_offset = 0;
    if (address >= ScratchpadBase &&
        static_cast<std::uint64_t>(address) <
            static_cast<std::uint64_t>(ScratchpadBase) + ScratchpadSize) {
        if (!resolve_scratchpad(address, 16, scratchpad_offset, error)) return false;
        for (std::size_t i = 0; i < 8; ++i) {
            scratchpad_[scratchpad_offset + i] =
                static_cast<std::uint8_t>((low >> (8U * i)) & 0xFFU);
            scratchpad_[scratchpad_offset + 8 + i] =
                static_cast<std::uint8_t>((high >> (8U * i)) & 0xFFU);
        }
        if (error) error->clear();
        return true;
    }
    std::size_t bios_offset = 0;
    if (resolve_bios_rom(address, 16, bios_offset, nullptr)) {
        if (error) *error = "Writes to the EE BIOS ROM are not permitted";
        return false;
    }
    const std::uint32_t physical = address >= Kseg0Base && address < 0xC0000000U
        ? address & SegmentMask : address;
    if (physical >= BiosRomBase && physical < BiosRomBase + BiosRomWindowSize) {
        if (error) *error = bios_rom_.empty() ? "No BIOS image is mapped" : "BIOS write exceeds the loaded ROM image";
        return false;
    }
    if (physical >= IopRamBase &&
        static_cast<std::uint64_t>(physical - IopRamBase) + 16U <= IopRamSize) {
        const std::size_t iop_offset = physical - IopRamBase;
        for (std::size_t i = 0; i < 8; ++i) {
            iop_ram_[iop_offset + i] = static_cast<std::uint8_t>((low >> (8U * i)) & 0xFFU);
            iop_ram_[iop_offset + 8U + i] = static_cast<std::uint8_t>((high >> (8U * i)) & 0xFFU);
        }
        if (error) error->clear();
        return true;
    }
    std::size_t offset = 0;
    if (!resolve_main_ram(address, 16, 16, offset, error)) return false;
    for (std::size_t i = 0; i < 8; ++i) {
        ram_[offset + i] = static_cast<std::uint8_t>((low >> (8U * i)) & 0xFFU);
        ram_[offset + 8 + i] = static_cast<std::uint8_t>((high >> (8U * i)) & 0xFFU);
    }
    if (error) error->clear();
    return true;
}

} // namespace retroshelf
