#pragma once

namespace retroshelf {

// Separate EE DMAC INT1 input. This is not an INTC source.
class EeDmacInterruptSource {
public:
    virtual ~EeDmacInterruptSource() = default;
    virtual bool int1_pending() const = 0;
};

} // namespace retroshelf
