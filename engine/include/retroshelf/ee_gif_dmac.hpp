#pragma once

#include "retroshelf/ee_gif_gs_runtime.hpp"
#include "retroshelf/system_bus.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace retroshelf {

// DMAC global status/control subset. Only the GIF channel completion source
// is currently driven by EeGifDmacChannel2; other channels are not emulated.
class EeDmacGlobalRegisters final : public MmioDevice {
public:
    static constexpr std::uint32_t RegisterBase = 0x1000E000U;
    static constexpr std::uint32_t RegisterSize = 0x70U;
    static constexpr std::uint32_t DmaControlOffset = 0x00U;
    static constexpr std::uint32_t DmaStatusOffset = 0x10U;
    static constexpr std::uint32_t DmaPriorityOffset = 0x20U;
    static constexpr std::uint32_t SkipQuadwordOffset = 0x30U;
    static constexpr std::uint32_t RingBufferSizeOffset = 0x40U;
    static constexpr std::uint32_t RingBufferOffset = 0x50U;
    static constexpr std::uint32_t StallAddressOffset = 0x60U;

    static constexpr std::uint32_t DmaEnable = 1U;
    static constexpr std::uint32_t GifChannel = 2U;
    static constexpr std::uint32_t BusErrorFlag = 1U << 15U;

    bool read(std::uint32_t offset, std::size_t size,
              std::vector<std::uint8_t>& data,
              std::string* error = nullptr) const override;
    bool write(std::uint32_t offset, const std::vector<std::uint8_t>& data,
               std::string* error = nullptr) override;

    bool dma_enabled() const;
    void set_channel_interrupt(std::uint32_t channel);
    void set_bus_error();
    bool int1_pending() const;
    std::uint32_t status() const;
    std::uint32_t mask() const;

private:
    std::uint32_t dma_control_{0};
    std::uint32_t dma_status_{0};
    std::uint32_t dma_priority_{0};
    std::uint32_t skip_quadword_{0};
    std::uint32_t ring_buffer_size_{0};
    std::uint32_t ring_buffer_offset_{0};
    std::uint32_t stall_address_{0};
};

// EE GIF DMA channel 2 register window. A CHCR.STR write starts the currently
// implemented synchronous EE RAM -> GIF -> GS path. It is not a full DMAC:
// arbitration, MFIFO, interleave, scratchpad selection and other channels are
// deliberately unsupported and reported rather than silently simulated.
class EeGifDmacChannel2 final : public MmioDevice {
public:
    static constexpr std::uint32_t RegisterBase = 0x1000A000U;
    static constexpr std::uint32_t RegisterSize = 0x60U;
    static constexpr std::uint32_t ChcrOffset = 0x00U;
    static constexpr std::uint32_t MadrOffset = 0x10U;
    static constexpr std::uint32_t QwcOffset = 0x20U;
    static constexpr std::uint32_t TadrOffset = 0x30U;
    static constexpr std::uint32_t Asr0Offset = 0x40U;
    static constexpr std::uint32_t Asr1Offset = 0x50U;
    static constexpr std::uint32_t StartBit = 1U << 8U;

    EeGifDmacChannel2(EeGifGsRuntime& runtime, EeDmacGlobalRegisters& global);

    bool read(std::uint32_t offset, std::size_t size,
              std::vector<std::uint8_t>& data,
              std::string* error = nullptr) const override;
    bool write(std::uint32_t offset, const std::vector<std::uint8_t>& data,
               std::string* error = nullptr) override;

    const std::string& error() const;

private:
    EeGifGsRuntime& runtime_;
    EeDmacGlobalRegisters& global_;
    std::string error_;
};

} // namespace retroshelf
