#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace retroshelf {

// VU1 has independent 16 KiB instruction and data memories. This class is
// storage only: VU instruction execution, address wrapping, and bus timing
// are deliberately handled by later VU/VIF components.
class Vu1Memory {
public:
    static constexpr std::size_t MicroMemorySize = 16U * 1024U;
    static constexpr std::size_t DataMemorySize = 16U * 1024U;

    Vu1Memory();

    bool read_micro(std::size_t address, std::uint8_t* destination,
                    std::size_t size) const;
    bool write_micro(std::size_t address, const std::uint8_t* source,
                     std::size_t size);
    bool read_data(std::size_t address, std::uint8_t* destination,
                   std::size_t size) const;
    bool write_data(std::size_t address, const std::uint8_t* source,
                    std::size_t size);

    void reset();
    const std::vector<std::uint8_t>& micro_bytes() const;
    const std::vector<std::uint8_t>& data_bytes() const;

private:
    static bool read_range(const std::vector<std::uint8_t>& memory,
                           std::size_t address, std::uint8_t* destination,
                           std::size_t size);
    static bool write_range(std::vector<std::uint8_t>& memory,
                            std::size_t address, const std::uint8_t* source,
                            std::size_t size);

    std::vector<std::uint8_t> micro_;
    std::vector<std::uint8_t> data_;
};

} // namespace retroshelf
