#include "retroshelf/ps2_ee_memory_map.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main() {
    retroshelf::Ps2EeMemoryMap memory(1024U * 1024U);
    int failures = 0;
    const auto check = [&failures](bool condition, const char* label) {
        if (!condition) { std::cerr << "FAIL: " << label << '\n'; ++failures; }
        else std::cout << "PASS: " << label << '\n';
    };
    std::string error;
    std::uint64_t value = 0;
    std::uint64_t low = 0, high = 0;

    check(memory.write(0x100, 4, 0x12345678, &error), "write little-endian word to RAM");
    check(memory.read(0x100, 4, value, &error) && value == 0x12345678,
          "read word from physical RAM");
    check(memory.read(0x20000100, 4, value, &error) && value == 0x12345678,
          "main RAM mirror resolves to same bytes");
    check(memory.read(0x80000100, 4, value, &error) && value == 0x12345678,
          "KSEG0 cached alias resolves to RAM");
    check(memory.read(0xA0000100, 4, value, &error) && value == 0x12345678,
          "KSEG1 uncached alias resolves to RAM");

    check(memory.read(0x1F801000U, 4, value, &error) && value == 0x1F000000U,
          "IOP expansion-1 memory-control register exposes its fixed base mapping");
    check(memory.read(0xBF801004U, 4, value, &error) && value == 0x1F802000U,
          "IOP expansion-2 memory-control register is readable through KSEG1");
    check(memory.read(0xBF801010U, 4, value, &error) && value == 0x0013243FU,
          "IOP BIOS timing register exposes its measured power-on value");
    check(memory.write(0xBF801010U, 4, 0x12345678U, &error) &&
              memory.read(0x1F801010U, 4, value, &error) && value == 0x12345678U,
          "IOP memory-latency register writes persist across virtual aliases");
    check(memory.read(retroshelf::Ps2EeMemoryMap::EeMchRicmAddress, 4, value, &error) && value == 0,
          "EE MCH_RICM read returns the documented zero value");
    check(memory.write(retroshelf::Ps2EeMemoryMap::EeMchRicmAddress, 4, 0x00210000U, &error) &&
              memory.read(retroshelf::Ps2EeMemoryMap::EeMchDrdAddress, 4, value, &error) && value == 0x1FU,
          "EE RDRAM device-ID command returns the first device signature");
    check(memory.read(retroshelf::Ps2EeMemoryMap::EeMchDrdAddress, 4, value, &error) && value == 0x1FU,
          "EE RDRAM device-ID command returns the second device signature");
    check(memory.read(retroshelf::Ps2EeMemoryMap::EeMchDrdAddress, 4, value, &error) && value == 0,
          "EE RDRAM device-ID command returns zero after two devices");
    check(memory.write(retroshelf::Ps2EeMemoryMap::EeMchRicmAddress, 4, 0x00230000U, &error) &&
              memory.read(retroshelf::Ps2EeMemoryMap::EeMchDrdAddress, 4, value, &error) && value == 0x0D0DU,
          "EE RDRAM configuration command returns the documented 0x0D0D value");
    check(memory.write(retroshelf::Ps2EeMemoryMap::EeMchRicmAddress, 4, 0x00240000U, &error) &&
              memory.read(retroshelf::Ps2EeMemoryMap::EeMchDrdAddress, 4, value, &error) && value == 0x0090U,
          "EE RDRAM configuration command returns the documented 0x0090 value");
    check(memory.write(retroshelf::Ps2EeMemoryMap::EeMchRicmAddress, 4, 0x00400015U, &error) &&
              memory.read(retroshelf::Ps2EeMemoryMap::EeMchDrdAddress, 4, value, &error) && value == 0x15U,
          "EE RDRAM command 0x40 returns the low five command bits");
    check(memory.write(retroshelf::Ps2EeMemoryMap::EeMchDrdAddress, 4, 0x80U, &error) &&
              memory.read(retroshelf::Ps2EeMemoryMap::EeMchDrdAddress, 4, value, &error) && value == 0x15U,
          "EE MCH_DRD write latches without changing the active RDRAM command response");
    check(memory.write(retroshelf::Ps2EeMemoryMap::EeMchDrdAddress, 4, 0U, &error) &&
              memory.write(0xB000F430U, 4, 0x00210040U, &error) &&
              memory.write(0xB000F430U, 4, 0x00210000U, &error) &&
              memory.read(0xB000F440U, 4, value, &error) && value == 0x1FU,
          "EE RDRAM command registers are accessible through KSEG1 aliases");
    check(memory.write(retroshelf::Ps2EeMemoryMap::EeMemoryControllerControlAddress,
                       4, 1U, &error) &&
              memory.read(retroshelf::Ps2EeMemoryMap::EeMemoryControllerControlAddress,
                          4, value, &error) && value == 1U,
          "EE BIOS memory-controller control register accepts and retains its reset write");
    check(memory.write(0xB000F500U, 4, 0xFFFFFFFFU, &error) &&
              memory.read(retroshelf::Ps2EeMemoryMap::EeMemoryControllerControlAddress,
                          4, value, &error) && value == 0xFFFFFFFFU,
          "EE memory-controller control register is accessible through KSEG1 alias");
    check(memory.read(0x1000F110U, 4, value, &error) && value == 0x60U,
          "EE SIO line status reports an idle transmitter");
    check(memory.read(0x1000F130U, 4, value, &error) && value == 0x02U,
          "EE SIO interrupt status reports an empty transmit FIFO");
    check(memory.write(0xB000F100U, 4, 0x20U, &error) &&
              memory.read(0x1000F100U, 4, value, &error) && value == 0x20U,
          "EE SIO line-control register is accessible through KSEG1 alias");
    check(memory.write(0xB000F410U, 4, 0U, &error) &&
              memory.read(0x1000F410U, 4, value, &error) && value == 0U,
          "undocumented EE RDRAM-controller register F410 accepts BIOS initialization writes");
    check(memory.write(0xB000F410U, 4, 0x80000000U, &error) &&
              memory.read(0x1000F410U, 4, value, &error) && value == 0U &&
              memory.read(0x1000F410U, 4, value, &error) && value == 0U,
          "undocumented EE RDRAM F410 reads as zero without a fabricated busy handshake");
    check(memory.read(0xB000F520U, 4, value, &error) && value == 0x1201U,
          "EE D_ENABLER exposes the documented BIOS startup status");
    check(memory.write(0xB000F590U, 4, 0x10000U, &error) &&
              memory.read(0x1000F520U, 4, value, &error) && value == 0x11201U,
          "EE D_ENABLEW disables DMA and updates the D_ENABLER status bit");

    check(memory.read(0x1F801060U, 4, value, &error) && value == 0x00000B88U,
          "IOP RAM_SIZE exposes its KUSEG low-half hardware value");
    check(memory.read(0xBF801060U, 4, value, &error) && value == 0x03E00B88U,
          "IOP RAM_SIZE exposes its KSEG bus-latched hardware value");
    check(memory.write(0xBF801060U, 4, 0U, &error) &&
              memory.read(0xBF801060U, 4, value, &error) && value == 0x03E00B88U,
          "IOP RAM_SIZE ignores writes");
    memory.raise_iop_interrupt(0x0025U);
    check(memory.read(retroshelf::Ps2EeMemoryMap::IopInterruptStatusAddress, 4, value, &error) && value == 0x0025U,
          "IOP I_STAT reports raised interrupt bits through a word read");
    check(memory.write(retroshelf::Ps2EeMemoryMap::IopInterruptStatusAddress, 2, 0x0004U, &error) &&
              memory.iop_interrupt_status() == 0x0021U,
          "IOP I_STAT clears only bits written as one");
    check(memory.write(0xBF801070U, 4, 0U, &error) &&
              memory.read(0x1F801070U, 2, value, &error) && value == 0x0021U,
          "IOP BIOS zero-write to I_STAT is harmless through KSEG1");
    check(memory.write(0xBF801074U, 4, 0x1234U, &error) &&
              memory.read(0x1F801074U, 2, value, &error) && value == 0x1234U,
          "IOP I_MASK is readable and writable through KSEG1 aliases");
    check(memory.write(0x1F801075U, 1, 0xABU, &error) &&
              memory.read(0xBF801075U, 1, value, &error) && value == 0xABU &&
              memory.iop_interrupt_mask() == 0xAB34U,
          "IOP I_MASK supports byte-lane access");
    check(memory.write(0xBF802070U, 4, 0xFEU, &error) &&
              memory.iop_post2_last_value() == 0xFEU && memory.iop_post2_write_count() == 1U,
          "IOP POST2 diagnostic port accepts BIOS boot-status writes through KSEG1");
    check(!memory.read(0x1F802070U, 4, value, &error) &&
              error.find("write-only") != std::string::npos,
          "IOP POST2 diagnostic port is write-only");
    check(memory.write(0x1FA00000U, 1, 0x7EU, &error) &&
              memory.iop_post3_last_value() == 0x7EU && memory.iop_post3_write_count() == 1U,
          "IOP POST3 accepts BIOS boot-status byte writes");
    check(!memory.read(0x1FA00000U, 1, value, &error) &&
              error.find("write-only") != std::string::npos,
          "IOP POST3 diagnostic port is write-only");
    check(memory.write(0xBF8010F0U, 4, 0x07777777U, &error) &&
              memory.read(0x1F8010F0U, 4, value, &error) && value == 0x07777777U,
          "IOP DPCR priority-control register supports BIOS writes through KSEG1");
    check(memory.write(0x1F801574U, 4, 0x80000000U, &error) &&
              memory.read(0xBF801574U, 4, value, &error) && value == 0x80000000U,
          "IOP DICR2 interrupt-control register is accessible through both aliases");
    check(memory.write(0xBF808268U, 4, 0x00000001U, &error) &&
              memory.read(0x1F808268U, 4, value, &error) && value == 0x00000001U,
          "IOP SIO2 control register supports BIOS initialization through KSEG1");
    check(memory.write(0xBF801D86U, 2, 0x5A3CU, &error) &&
              memory.read(0x1F801D86U, 2, value, &error) && value == 0x5A3CU,
          "IOP legacy SPU compatibility window supports BIOS halfword writes through KSEG1");
    check(memory.write(0xBF900186U, 2, 0xA53CU, &error) &&
              memory.read(0x1F900186U, 2, value, &error) && value == 0xA53CU,
          "IOP SPU2 register bank is independently mapped at 0x1F900000");
    check(memory.write(0x1000F200U, 4, 0x12345678U, &error) &&
              memory.read(0x1D000000U, 4, value, &error) && value == 0x12345678U,
          "EE-to-IOP SIF communication register is shared across EE and IOP windows");
    check(memory.write(0x9D000010U, 4, 0x87654321U, &error) &&
              memory.read(0x1000F210U, 4, value, &error) && value == 0x87654321U,
          "IOP-to-EE SIF communication register is shared across KSEG0 aliases");
    check(memory.write(0x1000F220U, 4, 0x00000005U, &error) &&
              memory.read(0x1D000020U, 4, value, &error) && value == 0x00000005U,
          "SIF EE-to-IOP flag register is visible from the IOP window");
    check(memory.write(0x1D000040U, 4, 0x00080000U, &error) &&
              memory.read(0x1000F240U, 4, value, &error) && value == 0x00080000U,
          "SIF control register is shared across EE and IOP windows");
    check(memory.write(0x1C000100U, 4, 0x1234ABCDU, &error) &&
              memory.read(0x9C000100U, 4, value, &error) && value == 0x1234ABCDU,
          "EE IOP RAM window is shared across physical and KSEG0 aliases");
    check(memory.write(0xBC000104U, 4, 0xA55A6996U, &error) &&
              memory.read(0x1C000104U, 4, value, &error) && value == 0xA55A6996U,
          "EE IOP RAM window is shared across KSEG1 and physical aliases");
    check(memory.write128(0x1C000110U, 0x0123456789ABCDEFULL,
                          0xFEDCBA9876543210ULL, &error) &&
              memory.read128(0x9C000110U, low, high, &error) &&
              low == 0x0123456789ABCDEFULL && high == 0xFEDCBA9876543210ULL,
          "EE IOP RAM supports aligned 128-bit accesses through aliases");
    check(!memory.read(0x1C200000U, 4, value, &error) && !error.empty(),
          "EE IOP RAM rejects accesses immediately beyond its 2 MiB window");
    check(memory.read(0xFFFE0130U, 4, value, &error) && value == 0x0001E988U,
          "EE cache-control register exposes its modeled reset value");
    check(memory.write(0xFFFE0130U, 4, 0x12345678U, &error) &&
              memory.read(0xFFFE0130U, 4, value, &error) && value == 0x12345678U,
          "EE cache-control register accepts word writes");

    // The accelerated uncached window maps physical RAM starting at offset 1 MiB.
    retroshelf::Ps2EeMemoryMap full_ram;
    check(full_ram.write(0x00100000U, 4, 0xCAFEBABEU, &error),
          "seed RAM at the accelerated alias's 1 MiB offset");
    check(full_ram.read(retroshelf::Ps2EeMemoryMap::MainRamAcceleratedBase,
                        4, value, &error) && value == 0xCAFEBABEU,
          "accelerated uncached alias resolves to physical RAM offset 1 MiB");
    check(full_ram.write(retroshelf::Ps2EeMemoryMap::MainRamAcceleratedBase + 4U,
                         4, 0x1234ABCDU, &error) &&
              full_ram.read(0x00100004U, 4, value, &error) && value == 0x1234ABCDU,
          "writes through accelerated alias update the same physical RAM");
    check(!full_ram.read(retroshelf::Ps2EeMemoryMap::MainRamAcceleratedEnd,
                         4, value, &error) && !error.empty(),
          "address immediately beyond accelerated RAM alias is rejected");

    check(memory.write(0x104, 1, 0xFE, &error), "byte write succeeds");
    check(memory.read(0x104, 1, value, &error) && value == 0xFE,
          "byte read returns unsigned byte value");
    check(!memory.read(0x101, 4, value, &error) && !error.empty(),
          "unaligned word access is rejected");
    check(!memory.write(0x40000000, 4, 1, &error) && !error.empty(),
          "unmapped address is rejected");
    check(!memory.write(0x100000, 8, 1, &error) && !error.empty(),
          "out-of-range access is rejected for small configured RAM");

    check(memory.scratchpad_size() == retroshelf::Ps2EeMemoryMap::ScratchpadSize,
          "EE scratchpad has the expected 16 KiB capacity");
    check(memory.write(retroshelf::Ps2EeMemoryMap::ScratchpadBase, 4, 0x78563412U, &error),
          "write word to EE scratchpad");
    check(memory.read(retroshelf::Ps2EeMemoryMap::ScratchpadBase, 4, value, &error) &&
              value == 0x78563412U,
          "read word from EE scratchpad");
    check(memory.read(0x70000000U, 4, value, &error) && value == 0x78563412U,
          "scratchpad bytes remain accessible at the architectural base");
    check(memory.write128(0x70000010U, 0x0123456789ABCDEFULL,
                          0xFEDCBA9876543210ULL, &error),
          "write aligned 128-bit value to EE scratchpad");
    check(memory.read128(0x70000010U, low, high, &error) &&
              low == 0x0123456789ABCDEFULL && high == 0xFEDCBA9876543210ULL,
          "read aligned 128-bit value from EE scratchpad");
    check(!memory.write(0x70003FFEU, 4, 1, &error),
          "scratchpad access crossing the 16 KiB boundary is rejected");
    check(!memory.write(0x70000001U, 4, 1, &error) && !error.empty(),
          "misaligned scratchpad access is rejected");

    retroshelf::Ps2EeMemoryMap small(256);
    check(!small.read(0x100, 8, value, &error), "access beyond configured RAM is rejected");
    check(!small.read(0xBFC00000U, 4, value, &error) &&
              error.find("No BIOS image is mapped") != std::string::npos,
          "BIOS-window read without a loaded ROM reports a BIOS-specific error");
    // BIOS bytes are mapped read-only through physical, KSEG0 and KSEG1 aliases.
    std::vector<std::uint8_t> bios(16);
    for (std::size_t i = 0; i < bios.size(); ++i)
        bios[i] = static_cast<std::uint8_t>(0x10U + i);
    check(memory.load_bios_image(bios, &error), "load synthetic BIOS ROM image");
    check(memory.bios_rom_size() == bios.size(), "BIOS ROM reports mapped image size");
    check(memory.read(retroshelf::Ps2EeMemoryMap::BiosRomBase, 4, value, &error) &&
          value == 0x13121110U, "read BIOS bytes from physical ROM window");
    check(memory.read(0x9FC00000U, 4, value, &error) && value == 0x13121110U,
          "read BIOS bytes through KSEG0 alias");
    check(memory.read(0xBFC00000U, 4, value, &error) && value == 0x13121110U,
          "read BIOS bytes through KSEG1 reset-vector alias");
    check(!memory.read(0xBFC00010U, 4, value, &error) &&
              error.find("BIOS read exceeds the loaded ROM image") != std::string::npos,
          "read beyond loaded BIOS image reports a ROM-boundary error");
    check(!memory.read128(0xBFC00000U + 0x10U, low, high, &error) &&
              error.find("BIOS read exceeds the loaded ROM image") != std::string::npos,
          "128-bit read beyond loaded BIOS image reports a ROM-boundary error");
    check(!memory.write(0xBFC00000U, 4, 0xDEADBEEFU, &error) && !error.empty(),
          "writes to BIOS ROM are rejected");
    check(memory.read(0x1FC00000U, 4, value, &error) && value == 0x13121110U,
          "rejected BIOS write leaves ROM contents unchanged");
    check(memory.read128(0xBFC00000U, low, high, &error) &&
          low == 0x1716151413121110ULL && high == 0x1F1E1D1C1B1A1918ULL,
          "read aligned 128-bit BIOS data");
    check(!memory.write128(0xBFC00000U, 0, 0, &error) && !error.empty(),
          "128-bit BIOS writes are rejected");
    check(!memory.load_bios_image({}, &error), "empty BIOS image is rejected");
    check(!memory.load_bios_image(std::vector<std::uint8_t>(
              retroshelf::Ps2EeMemoryMap::BiosRomWindowSize + 1U, 0), &error),
          "BIOS image larger than ROM window is rejected");

    return failures == 0 ? 0 : 1;
}
