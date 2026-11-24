#include "retroshelf/vu1_memory.hpp"

#include <algorithm>

namespace retroshelf {

Vu1Memory::Vu1Memory()
    : micro_(MicroMemorySize, 0), data_(DataMemorySize, 0) {}

bool Vu1Memory::read_range(const std::vector<std::uint8_t>& memory,
                           std::size_t address, std::uint8_t* destination,
                           std::size_t size) {
    if (address > memory.size() || size > memory.size() - address ||
        (size != 0U && destination == nullptr)) return false;
    if (size != 0U)
        std::copy_n(memory.data() + address, size, destination);
    return true;
}

bool Vu1Memory::write_range(std::vector<std::uint8_t>& memory,
                            std::size_t address, const std::uint8_t* source,
                            std::size_t size) {
    if (address > memory.size() || size > memory.size() - address ||
        (size != 0U && source == nullptr)) return false;
    if (size != 0U)
        std::copy_n(source, size, memory.data() + address);
    return true;
}

bool Vu1Memory::read_micro(std::size_t address, std::uint8_t* destination,
                           std::size_t size) const {
    return read_range(micro_, address, destination, size);
}
bool Vu1Memory::write_micro(std::size_t address, const std::uint8_t* source,
                            std::size_t size) {
    return write_range(micro_, address, source, size);
}
bool Vu1Memory::read_data(std::size_t address, std::uint8_t* destination,
                          std::size_t size) const {
    return read_range(data_, address, destination, size);
}
bool Vu1Memory::write_data(std::size_t address, const std::uint8_t* source,
                           std::size_t size) {
    return write_range(data_, address, source, size);
}

void Vu1Memory::reset() {
    std::fill(micro_.begin(), micro_.end(), 0);
    std::fill(data_.begin(), data_.end(), 0);
}
const std::vector<std::uint8_t>& Vu1Memory::micro_bytes() const { return micro_; }
const std::vector<std::uint8_t>& Vu1Memory::data_bytes() const { return data_; }

} // namespace retroshelf
