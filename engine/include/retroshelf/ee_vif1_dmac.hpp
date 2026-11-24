#pragma once

#include "retroshelf/ee_dmac_interrupt_source.hpp"
#include "retroshelf/ee_vif1.hpp"
#include "retroshelf/ee_vif1_command_decoder.hpp"
#include "retroshelf/system_bus.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace retroshelf {

class EeDmacGlobalRegisters;

// First VIF1 DMA channel slice: normal EE-RAM-to-VIF1 transfers. Source-chain,
// TTE/TIE, stall control and arbitration are intentionally rejected for now.
class EeVif1DmacChannel1 final : public MmioDevice {
public:
    static constexpr std::uint32_t RegisterBase = 0x10009000U;
    static constexpr std::uint32_t RegisterSize = 0x60U;
    static constexpr std::uint32_t ChcrOffset = 0x00U;
    static constexpr std::uint32_t MadrOffset = 0x10U;
    static constexpr std::uint32_t QwcOffset = 0x20U;
    static constexpr std::uint32_t TadrOffset = 0x30U;
    static constexpr std::uint32_t Asr0Offset = 0x40U;
    static constexpr std::uint32_t Asr1Offset = 0x50U;
    static constexpr std::uint32_t StartBit = 1U << 8U;
    static constexpr std::size_t MaxTransferQuadwords = 1U << 20U;

    EeVif1DmacChannel1(SystemBus& bus, EeVif1Fifo& fifo,
                       EeVif1CommandDecoder& decoder,
                       EeDmacGlobalRegisters& global);
    bool read(std::uint32_t offset, std::size_t size,
              std::vector<std::uint8_t>& data,
              std::string* error = nullptr) const override;
    bool write(std::uint32_t offset, const std::vector<std::uint8_t>& data,
               std::string* error = nullptr) override;
    const std::string& error() const;

private:
    bool start_transfer();
    bool fail(const std::string& message);
    SystemBus& bus_;
    EeVif1Fifo& fifo_;
    EeVif1CommandDecoder& decoder_;
    EeDmacGlobalRegisters& global_;
    std::uint32_t chcr_{0}, madr_{0}, qwc_{0}, tadr_{0}, asr0_{0}, asr1_{0};
    std::string error_;
};

} // namespace retroshelf
