#include "retroshelf/ee_vif1_dmac.hpp"
#include "retroshelf/ee_gif_dmac.hpp"

#include <limits>

namespace retroshelf {
namespace {
void encode_u32(std::uint32_t value, std::vector<std::uint8_t>& data) {
    data.resize(4U);
    for (std::size_t i = 0; i < 4U; ++i)
        data[i] = static_cast<std::uint8_t>(value >> (i * 8U));
}
std::uint32_t decode_u32(const std::vector<std::uint8_t>& data) {
    std::uint32_t value = 0U;
    for (std::size_t i = 0; i < 4U; ++i)
        value |= static_cast<std::uint32_t>(data[i]) << (i * 8U);
    return value;
}
bool valid_access(std::uint32_t offset, std::size_t size, std::string* error) {
    if (size != 4U || (offset & 3U) != 0U || offset >= EeVif1DmacChannel1::RegisterSize) {
        if (error) *error = "VIF1 DMA supports aligned 32-bit register accesses only";
        return false;
    }
    return true;
}
}

EeVif1DmacChannel1::EeVif1DmacChannel1(SystemBus& bus, EeVif1Fifo& fifo,
                                     EeVif1CommandDecoder& decoder,
                                     EeDmacGlobalRegisters& global)
    : bus_(bus), fifo_(fifo), decoder_(decoder), global_(global) {}

bool EeVif1DmacChannel1::read(std::uint32_t offset, std::size_t size,
                             std::vector<std::uint8_t>& data, std::string* error) const {
    if (!valid_access(offset, size, error)) { data.clear(); return false; }
    switch (offset) {
    case ChcrOffset: encode_u32(chcr_, data); break;
    case MadrOffset: encode_u32(madr_, data); break;
    case QwcOffset: encode_u32(qwc_, data); break;
    case TadrOffset: encode_u32(tadr_, data); break;
    case Asr0Offset: encode_u32(asr0_, data); break;
    case Asr1Offset: encode_u32(asr1_, data); break;
    default:
        if (error) *error = "Read from an unimplemented VIF1 DMA register";
        data.clear(); return false;
    }
    if (error) error->clear();
    return true;
}

bool EeVif1DmacChannel1::write(std::uint32_t offset,
                              const std::vector<std::uint8_t>& data,
                              std::string* error) {
    if (data.size() != 4U || (offset & 3U) != 0U || offset >= RegisterSize) {
        if (error) *error = "VIF1 DMA supports aligned 32-bit register writes only";
        return false;
    }
    const std::uint32_t value = decode_u32(data);
    switch (offset) {
    case ChcrOffset:
        chcr_ = value;
        if ((chcr_ & StartBit) != 0U) {
            error_.clear();
            if (!global_.dma_enabled()) {
                error_ = "EE DMAC is globally disabled (D_CTRL.DMAE=0)";
                chcr_ &= ~StartBit;
                break;
            }
            if (!start_transfer()) {
                if (error) *error = error_;
                return false;
            }
        }
        break;
    case MadrOffset: madr_ = value; break;
    case QwcOffset: qwc_ = value & 0xFFFFU; break;
    case TadrOffset: tadr_ = value; break;
    case Asr0Offset: asr0_ = value; break;
    case Asr1Offset: asr1_ = value; break;
    default:
        if (error) *error = "Write to an unimplemented VIF1 DMA register";
        return false;
    }
    if (error) error->clear();
    return true;
}

bool EeVif1DmacChannel1::fail(const std::string& message) {
    error_ = message;
    chcr_ &= ~StartBit;
    global_.set_bus_error();
    return false;
}

bool EeVif1DmacChannel1::start_transfer() {
    constexpr std::uint32_t direction_bit = 1U;
    constexpr std::uint32_t mode_mask = 3U << 2U;
    if ((chcr_ & direction_bit) != 0U)
        return fail("VIF1 DMA direction must be EE RAM to VIF1");
    if ((chcr_ & mode_mask) != 0U)
        return fail("VIF1 DMA currently supports normal mode only; source-chain is not implemented");
    if (qwc_ > MaxTransferQuadwords)
        return fail("VIF1 DMA transfer exceeds the bounded quadword limit");
    if ((madr_ & 0xFU) != 0U)
        return fail("VIF1 DMA source address is not quadword-aligned");
    const std::uint64_t bytes = static_cast<std::uint64_t>(qwc_) * EeVif1Fifo::QuadwordSize;
    if (bytes > std::numeric_limits<std::uint32_t>::max() - madr_)
        return fail("VIF1 DMA source address overflows");

    for (std::uint32_t index = 0U; index < qwc_; ++index) {
        std::vector<std::uint8_t> quadword;
        std::string bus_error;
        const std::uint32_t source = madr_ + index * 16U;
        if (!bus_.read(source, EeVif1Fifo::QuadwordSize, quadword, &bus_error))
            return fail("VIF1 DMA source read failed: " + bus_error);
        if (!fifo_.write(0U, quadword, &bus_error))
            return fail("VIF1 DMA FIFO write failed: " + bus_error);
        std::size_t drained = 0U;
        if (!fifo_.drain_to(decoder_, &drained, 1U) || drained != 1U)
            return fail(std::string("VIF1 DMA command execution failed: ") + decoder_.last_error());
        madr_ = source + 16U;
        qwc_ = qwc_ - 1U;
    }
    chcr_ &= ~StartBit;
    global_.set_channel_interrupt(1U);
    return true;
}

const std::string& EeVif1DmacChannel1::error() const { return error_; }

} // namespace retroshelf
