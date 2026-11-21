#include "retroshelf/ee_gif_dmac.hpp"

namespace retroshelf {
namespace {

void encode_u32(std::uint32_t value, std::vector<std::uint8_t>& data) {
    data.resize(4);
    for (std::size_t i = 0; i < 4; ++i)
        data[i] = static_cast<std::uint8_t>((value >> (i * 8U)) & 0xFFU);
}

std::uint32_t decode_u32(const std::vector<std::uint8_t>& data) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(data[i]) << (i * 8U);
    return value;
}

bool valid_register_access(std::uint32_t offset, std::size_t size,
                           std::uint32_t register_size, std::string* error) {
    if (size != 4 || (offset & 3U) != 0U || offset >= register_size) {
        if (error) *error = "EE DMAC supports aligned 32-bit register accesses only";
        return false;
    }
    return true;
}

} // namespace

bool EeDmacGlobalRegisters::read(std::uint32_t offset, std::size_t size,
                                 std::vector<std::uint8_t>& data,
                                 std::string* error) const {
    if (!valid_register_access(offset, size, RegisterSize, error)) {
        data.clear();
        return false;
    }

    switch (offset) {
    case DmaControlOffset: encode_u32(dma_control_, data); break;
    case DmaStatusOffset: encode_u32(dma_status_, data); break;
    case DmaPriorityOffset: encode_u32(dma_priority_, data); break;
    case SkipQuadwordOffset: encode_u32(skip_quadword_, data); break;
    case RingBufferSizeOffset: encode_u32(ring_buffer_size_, data); break;
    case RingBufferOffset: encode_u32(ring_buffer_offset_, data); break;
    case StallAddressOffset: encode_u32(stall_address_, data); break;
    default:
        if (error) *error = "Read from an unimplemented EE DMAC global register";
        data.clear();
        return false;
    }
    if (error) error->clear();
    return true;
}

bool EeDmacGlobalRegisters::write(std::uint32_t offset,
                                  const std::vector<std::uint8_t>& data,
                                  std::string* error) {
    if (data.size() != 4 || (offset & 3U) != 0U || offset >= RegisterSize) {
        if (error) *error = "EE DMAC supports aligned 32-bit register accesses only";
        return false;
    }
    const std::uint32_t value = decode_u32(data);
    switch (offset) {
    case DmaControlOffset:
        // Preserve the documented control fields for correct MMIO readback.
        // Only DMAE currently affects execution; scheduling/MFIFO controls are
        // retained but not yet acted upon by the transfer scheduler.
        dma_control_ = value & 0x000007FFU;
        break;
    case DmaStatusOffset:
        // Channel, stall, MFIFO-empty, and bus-error status bits are
        // write-one-to-clear. Channel masks and the stall/MFIFO masks toggle.
        dma_status_ &= ~(value & (0x000003FFU | 0x0000E000U));
        dma_status_ ^= value & (0x03FF0000U | 0x60000000U);
        break;
    case DmaPriorityOffset: dma_priority_ = value; break;
    case SkipQuadwordOffset: skip_quadword_ = value; break;
    case RingBufferSizeOffset: ring_buffer_size_ = value; break;
    case RingBufferOffset: ring_buffer_offset_ = value & 0x7FFFFFF0U; break;
    case StallAddressOffset: stall_address_ = value & 0xFFFFFFF0U; break;
    default:
        if (error) *error = "Write to an unimplemented EE DMAC global register";
        return false;
    }
    if (error) error->clear();
    return true;
}

bool EeDmacGlobalRegisters::dma_enabled() const {
    return (dma_control_ & DmaEnable) != 0U;
}

void EeDmacGlobalRegisters::set_channel_interrupt(std::uint32_t channel) {
    if (channel < 10U) dma_status_ |= 1U << channel;
}

void EeDmacGlobalRegisters::set_stall_interrupt() {
    dma_status_ |= StallInterruptFlag;
}

void EeDmacGlobalRegisters::set_mfifo_empty_interrupt() {
    dma_status_ |= MfifoEmptyInterruptFlag;
}

void EeDmacGlobalRegisters::set_bus_error() {
    dma_status_ |= BusErrorFlag;
}

bool EeDmacGlobalRegisters::int1_pending() const {
    const std::uint32_t channel_status = dma_status_ & 0x3FFU;
    const std::uint32_t channel_mask = (dma_status_ >> 16U) & 0x3FFU;
    const bool masked_channel = (channel_status & channel_mask) != 0U;
    const bool stall = (dma_status_ & StallInterruptFlag) != 0U &&
                       (dma_status_ & StallInterruptMask) != 0U;
    const bool mfifo = (dma_status_ & MfifoEmptyInterruptFlag) != 0U &&
                       (dma_status_ & MfifoEmptyInterruptMask) != 0U;
    return masked_channel || stall || mfifo || (dma_status_ & BusErrorFlag) != 0U;
}

std::uint32_t EeDmacGlobalRegisters::status() const { return dma_status_; }
std::uint32_t EeDmacGlobalRegisters::mask() const {
    return (dma_status_ >> 16U) & 0x3FFU;
}

EeGifDmacChannel2::EeGifDmacChannel2(EeGifGsRuntime& runtime,
                                     EeDmacGlobalRegisters& global)
    : runtime_(runtime), global_(global) {}

bool EeGifDmacChannel2::read(std::uint32_t offset, std::size_t size,
                             std::vector<std::uint8_t>& data,
                             std::string* error) const {
    if (!valid_register_access(offset, size, RegisterSize, error)) {
        data.clear();
        return false;
    }
    const EeGifDmaRegisters& regs = runtime_.dma().registers;
    switch (offset) {
    case ChcrOffset: encode_u32(regs.chcr, data); break;
    case MadrOffset: encode_u32(regs.madr, data); break;
    case QwcOffset: encode_u32(regs.qwc, data); break;
    case TadrOffset: encode_u32(regs.tadr, data); break;
    case Asr0Offset: encode_u32(regs.asr0, data); break;
    case Asr1Offset: encode_u32(regs.asr1, data); break;
    default:
        if (error) *error = "Read from an unimplemented GIF DMA channel register";
        data.clear();
        return false;
    }
    if (error) error->clear();
    return true;
}

bool EeGifDmacChannel2::write(std::uint32_t offset,
                              const std::vector<std::uint8_t>& data,
                              std::string* error) {
    if (data.size() != 4 || (offset & 3U) != 0U || offset >= RegisterSize) {
        if (error) *error = "GIF DMA channel supports aligned 32-bit register writes only";
        return false;
    }

    EeGifDmaRegisters& regs = runtime_.dma().registers;
    const std::uint32_t value = decode_u32(data);
    switch (offset) {
    case ChcrOffset:
        regs.chcr = value;
        if ((value & StartBit) != 0U) {
            error_.clear();
            if (!global_.dma_enabled()) {
                error_ = "EE DMAC is globally disabled (D_CTRL.DMAE=0)";
                regs.chcr &= ~StartBit;
                // The register write itself succeeds; only the transfer is rejected.
                // Returning false would make the SystemBus write fail and abort callers.
                if (error) error->clear();
                return true;
            }
            // The current GIF DMA engine implements EE RAM -> GIF normal and
            // source-chain modes synchronously. Direction, interleave, MFIFO,
            // arbitration and scratchpad transfers are not silently accepted.
            if (!runtime_.run_frame()) {
                error_ = runtime_.error();
                global_.set_bus_error();
                if (error) *error = error_;
                return false;
            }
            global_.set_channel_interrupt(EeDmacGlobalRegisters::GifChannel);
        }
        break;
    case MadrOffset: regs.madr = value; break;
    case QwcOffset: regs.qwc = value & 0xFFFFU; break;
    case TadrOffset: regs.tadr = value; break;
    case Asr0Offset: regs.asr0 = value; break;
    case Asr1Offset: regs.asr1 = value; break;
    default:
        if (error) *error = "Write to an unimplemented GIF DMA channel register";
        return false;
    }
    if (error) error->clear();
    return true;
}

const std::string& EeGifDmacChannel2::error() const { return error_; }

} // namespace retroshelf
