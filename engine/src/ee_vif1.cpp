#include "retroshelf/ee_vif1.hpp"
#include "retroshelf/ee_vif1_command_decoder.hpp"

#include <algorithm>

namespace retroshelf {

void EeVif1Registers::encode_u32(std::uint32_t value, std::vector<std::uint8_t>& data) {
    data.resize(4);
    for (std::size_t i = 0; i < 4; ++i)
        data[i] = static_cast<std::uint8_t>((value >> (i * 8U)) & 0xFFU);
}

std::uint32_t EeVif1Registers::decode_u32(const std::vector<std::uint8_t>& data) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(data[i]) << (i * 8U);
    return value;
}

void EeVif1Registers::reset() {
    status_ = 0;
    err_ = 0x2U;
    mark_ = cycle_ = mode_ = num_ = mask_ = code_ = 0;
    itops_ = base_ = ofst_ = tops_ = itop_ = top_ = 0;
    row_.fill(0);
    column_.fill(0);
}

bool EeVif1Registers::read(std::uint32_t offset, std::size_t size,
                           std::vector<std::uint8_t>& data, std::string* error) const {
    if (size != 4U || (offset & 3U) != 0U || offset >= RegisterSize) {
        if (error) *error = "VIF1 supports aligned 32-bit register accesses only";
        data.clear();
        return false;
    }
    std::uint32_t value = 0;
    if (offset == StatOffset) value = status_;
    else if (offset == ErrOffset) value = err_;
    else if (offset == MarkOffset) value = mark_;
    else if (offset == CycleOffset) value = cycle_;
    else if (offset == ModeOffset) value = mode_;
    else if (offset == NumOffset) value = num_;
    else if (offset == MaskOffset) value = mask_;
    else if (offset == CodeOffset) value = code_;
    else if (offset == ItopsOffset) value = itops_;
    else if (offset == BaseOffset) value = base_;
    else if (offset == OfstOffset) value = ofst_;
    else if (offset == TopsOffset) value = tops_;
    else if (offset == ItopOffset) value = itop_;
    else if (offset == TopOffset) value = top_;
    else if (offset >= RowBaseOffset && offset < RowBaseOffset + 0x40U &&
             ((offset - RowBaseOffset) % 0x10U) == 0U)
        value = row_[(offset - RowBaseOffset) / 0x10U];
    else if (offset >= ColumnBaseOffset && offset < ColumnBaseOffset + 0x40U &&
             ((offset - ColumnBaseOffset) % 0x10U) == 0U)
        value = column_[(offset - ColumnBaseOffset) / 0x10U];
    else {
        if (error) *error = "Read from an unimplemented VIF1 register";
        data.clear();
        return false;
    }
    encode_u32(value, data);
    if (error) error->clear();
    return true;
}

bool EeVif1Registers::write(std::uint32_t offset, const std::vector<std::uint8_t>& data,
                            std::string* error) {
    if (data.size() != 4U || (offset & 3U) != 0U || offset >= RegisterSize) {
        if (error) *error = "VIF1 supports aligned 32-bit register writes only";
        return false;
    }
    const std::uint32_t value = decode_u32(data);
    if (offset == FbrstOffset) {
        if ((value & 1U) != 0U) reset();
        if ((value & (1U << 1U)) != 0U) status_ |= StatVfs;
        if ((value & (1U << 2U)) != 0U) status_ |= StatVss;
        if ((value & (1U << 3U)) != 0U)
            status_ &= ~(StatVss | StatVfs | StatVis | StatInt | StatEr0 | StatEr1);
    } else if (offset == ErrOffset) err_ = value & 0x7U;
    else if (offset == MarkOffset) { mark_ = value & 0xFFFFU; status_ &= ~StatMrk; }
    else if (offset == CycleOffset) cycle_ = value & 0xFFFFU;
    else if (offset == ModeOffset) mode_ = value & 0x3U;
    else if (offset == NumOffset) num_ = value & 0xFFU;
    else if (offset == MaskOffset) mask_ = value;
    else if (offset == CodeOffset) code_ = value;
    else if (offset == ItopsOffset) itops_ = value & 0x3FFU;
    else if (offset == BaseOffset) base_ = value & 0x3FFU;
    else if (offset == OfstOffset) ofst_ = value & 0x3FFU;
    else if (offset == TopsOffset) tops_ = value & 0x3FFU;
    else if (offset == ItopOffset) itop_ = value & 0x3FFU;
    else if (offset == TopOffset) top_ = value & 0x3FFU;
    else if (offset >= RowBaseOffset && offset < RowBaseOffset + 0x40U &&
             ((offset - RowBaseOffset) % 0x10U) == 0U)
        row_[(offset - RowBaseOffset) / 0x10U] = value;
    else if (offset >= ColumnBaseOffset && offset < ColumnBaseOffset + 0x40U &&
             ((offset - ColumnBaseOffset) % 0x10U) == 0U)
        column_[(offset - ColumnBaseOffset) / 0x10U] = value;
    else {
        if (error) *error = "Write to a read-only or unimplemented VIF1 register";
        return false;
    }
    if (error) error->clear();
    return true;
}

std::uint32_t EeVif1Registers::status() const { return status_; }
std::uint32_t EeVif1Registers::tops() const { return tops_; }
void EeVif1Registers::set_status_flags(std::uint32_t flags) {
    status_ |= flags & (StatVgw | StatMrk | StatDbf | StatVss | StatVfs |
                        StatVis | StatInt | StatEr0 | StatEr1);
}
void EeVif1Registers::clear_status_flags(std::uint32_t flags) {
    status_ &= ~flags;
}
void EeVif1Registers::set_command_status(std::uint32_t vps) {
    status_ = (status_ & ~StatVpsMask) | (vps & StatVpsMask);
}
void EeVif1Registers::set_fifo_count(std::size_t count) {
    const auto bounded = static_cast<std::uint32_t>(count > 31U ? 31U : count);
    status_ = (status_ & ~StatFqcMask) | (bounded << StatFqcShift);
}
void EeVif1Registers::set_fifo_direction(bool read_from_vif) {
    if (read_from_vif) status_ |= StatFdr;
    else status_ &= ~StatFdr;
}

EeVif1Fifo::EeVif1Fifo(EeVif1Registers& registers) : registers_(registers) {
    update_status();
}

bool EeVif1Fifo::read(std::uint32_t offset, std::size_t size,
                      std::vector<std::uint8_t>& data, std::string* error) const {
    if (offset != 0U || size != QuadwordSize) {
        if (error) *error = "VIF1 FIFO requires a 128-bit access at offset zero";
        data.clear();
        return false;
    }
    if (count_ == 0U) {
        if (error) *error = "VIF1 FIFO underflow";
        data.clear();
        return false;
    }
    data.assign(entries_[head_].begin(), entries_[head_].end());
    // MmioDevice::read is const; FIFO reads mutate hardware state. The ring
    // state is mutable only for this consuming data-port operation.
    head_ = (head_ + 1U) % Capacity;
    --count_;
    update_status();
    if (error) error->clear();
    return true;
}

bool EeVif1Fifo::write(std::uint32_t offset, const std::vector<std::uint8_t>& data,
                       std::string* error) {
    if (offset != 0U || data.size() != QuadwordSize) {
        if (error) *error = "VIF1 FIFO requires a 128-bit access at offset zero";
        return false;
    }
    if (count_ == Capacity) {
        if (error) *error = "VIF1 FIFO overflow";
        return false;
    }
    const std::size_t tail = (head_ + count_) % Capacity;
    std::copy(data.begin(), data.end(), entries_[tail].begin());
    ++count_;
    update_status();
    if (error) error->clear();
    return true;
}

void EeVif1Fifo::reset() {
    head_ = 0U;
    count_ = 0U;
    for (auto& entry : entries_) entry.fill(0U);
    update_status();
}
std::size_t EeVif1Fifo::count() const { return count_; }

bool EeVif1Fifo::drain_to(EeVif1CommandDecoder& decoder,
                          std::size_t* drained,
                          std::size_t max_quadwords) {
    std::size_t consumed = 0U;
    while (count_ != 0U && consumed < max_quadwords) {
        std::vector<std::uint8_t> quadword;
        std::string error;
        if (!read(0U, QuadwordSize, quadword, &error)) {
            if (drained) *drained = consumed;
            return false;
        }
        ++consumed;
        if (!decoder.consume(quadword.data(), quadword.size())) {
            if (drained) *drained = consumed;
            return false;
        }
    }
    if (drained) *drained = consumed;
    return true;
}

void EeVif1Fifo::update_status() const { registers_.set_fifo_count(count_); }

} // namespace retroshelf
