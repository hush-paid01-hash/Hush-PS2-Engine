#pragma once

#include "retroshelf/system_bus.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace retroshelf {

class EeVif1CommandDecoder;

// VIF1 control/status register bank. This models register semantics only;
// command execution, FIFO flow control, VU1 memory and PATH2 are separate work.
class EeVif1Registers final : public MmioDevice {
public:
    static constexpr std::uint32_t RegisterBase = 0x10003C00U;
    static constexpr std::uint32_t RegisterSize = 0x180U;

    static constexpr std::uint32_t StatOffset = 0x00U;
    static constexpr std::uint32_t FbrstOffset = 0x10U;
    static constexpr std::uint32_t ErrOffset = 0x20U;
    static constexpr std::uint32_t MarkOffset = 0x30U;
    static constexpr std::uint32_t CycleOffset = 0x40U;
    static constexpr std::uint32_t ModeOffset = 0x50U;
    static constexpr std::uint32_t NumOffset = 0x60U;
    static constexpr std::uint32_t MaskOffset = 0x70U;
    static constexpr std::uint32_t CodeOffset = 0x80U;
    static constexpr std::uint32_t ItopsOffset = 0x90U;
    static constexpr std::uint32_t BaseOffset = 0xA0U;
    static constexpr std::uint32_t OfstOffset = 0xB0U;
    static constexpr std::uint32_t TopsOffset = 0xC0U;
    static constexpr std::uint32_t ItopOffset = 0xD0U;
    static constexpr std::uint32_t TopOffset = 0xE0U;
    static constexpr std::uint32_t RowBaseOffset = 0x100U;
    static constexpr std::uint32_t ColumnBaseOffset = 0x140U;

    static constexpr std::uint32_t StatVpsMask = 0x3U;
    static constexpr std::uint32_t StatVgw = 1U << 3U;
    static constexpr std::uint32_t StatMrk = 1U << 6U;
    static constexpr std::uint32_t StatDbf = 1U << 7U;
    static constexpr std::uint32_t StatVss = 1U << 8U;
    static constexpr std::uint32_t StatVfs = 1U << 9U;
    static constexpr std::uint32_t StatVis = 1U << 10U;
    static constexpr std::uint32_t StatInt = 1U << 11U;
    static constexpr std::uint32_t StatEr0 = 1U << 12U;
    static constexpr std::uint32_t StatEr1 = 1U << 13U;
    static constexpr std::uint32_t StatFdr = 1U << 23U;
    static constexpr std::uint32_t StatFqcShift = 24U;
    static constexpr std::uint32_t StatFqcMask = 0x1FU << StatFqcShift;

    bool read(std::uint32_t offset, std::size_t size,
              std::vector<std::uint8_t>& data,
              std::string* error = nullptr) const override;
    bool write(std::uint32_t offset, const std::vector<std::uint8_t>& data,
               std::string* error = nullptr) override;

    std::uint32_t status() const;
    std::uint32_t tops() const;
    std::uint64_t reset_generation() const;
    void set_status_flags(std::uint32_t flags);
    void clear_status_flags(std::uint32_t flags);
    void set_command_status(std::uint32_t vps);
    void set_fifo_count(std::size_t count);
    void set_fifo_direction(bool read_from_vif);

private:
    static void encode_u32(std::uint32_t value, std::vector<std::uint8_t>& data);
    static std::uint32_t decode_u32(const std::vector<std::uint8_t>& data);
    void reset();

    std::uint64_t reset_generation_{0};
    std::uint32_t status_{0};
    std::uint32_t err_{0x2U}; // ME0: suppress DMAtag mismatch stalls by default.
    std::uint32_t mark_{0};
    std::uint32_t cycle_{0};
    std::uint32_t mode_{0};
    std::uint32_t num_{0};
    std::uint32_t mask_{0};
    std::uint32_t code_{0};
    std::uint32_t itops_{0};
    std::uint32_t base_{0};
    std::uint32_t ofst_{0};
    std::uint32_t tops_{0};
    std::uint32_t itop_{0};
    std::uint32_t top_{0};
    std::array<std::uint32_t, 4> row_{};
    std::array<std::uint32_t, 4> column_{};
};

// VIF1 FIFO data port. The hardware FIFO holds sixteen 128-bit quadwords.
// Transfers are deliberately strict: only one aligned quadword access is
// accepted, and empty/full accesses fail instead of silently losing data.
class EeVif1Fifo final : public MmioDevice {
public:
    static constexpr std::uint32_t RegisterBase = 0x10005000U;
    static constexpr std::uint32_t RegisterSize = 0x10U;
    static constexpr std::size_t Capacity = 16U;
    static constexpr std::size_t QuadwordSize = 16U;

    explicit EeVif1Fifo(EeVif1Registers& registers);
    bool read(std::uint32_t offset, std::size_t size,
              std::vector<std::uint8_t>& data,
              std::string* error = nullptr) const override;
    bool write(std::uint32_t offset, const std::vector<std::uint8_t>& data,
               std::string* error = nullptr) override;
    void reset();
    std::size_t count() const;
    // Move queued quadwords into the incremental VIF1 command decoder.
    // Stops on the first decoder error and reports how many were consumed.
    bool drain_to(EeVif1CommandDecoder& decoder,
                  std::size_t* drained = nullptr,
                  std::size_t max_quadwords = Capacity);

private:
    void update_status() const;
    void synchronize_reset() const;
    EeVif1Registers& registers_;
    mutable std::array<std::array<std::uint8_t, QuadwordSize>, Capacity> entries_{};
    mutable std::size_t head_{0};
    mutable std::size_t count_{0};
    mutable std::uint64_t observed_reset_generation_{0};
    mutable bool decoder_reset_pending_{false};
};

} // namespace retroshelf
