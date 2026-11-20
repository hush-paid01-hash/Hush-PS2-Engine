#include "retroshelf/ps2_disc_execution.hpp"
#include "retroshelf/ee_interrupt_controller.hpp"
#include "retroshelf/ee_timer.hpp"
#include "retroshelf/ee_gif_dmac.hpp"
#include "retroshelf/ee_vif1.hpp"
#include "retroshelf/ee_gif_gs_runtime.hpp"
#include "retroshelf/system_bus.hpp"

#include <memory>

namespace retroshelf {

Ps2DiscExecutionResult load_and_execute_ps2_disc(const Iso9660Disc& disc,
                                                  Ps2EeMemoryMap& memory,
                                                  Cpu& cpu,
                                                  std::size_t instruction_budget) {
    Ps2DiscExecutionResult result;
    if (!boot_ps2_disc_into_ee_memory(disc, memory, result.executable_path,
                                      result.entry_point, &result.error)) {
        result.execution.final_pc = cpu.pc();
        return result;
    }

    result.image_loaded = true;
    cpu.reset();
    cpu.set_pc(result.entry_point);

    // Connect the same first-stage EE devices used by the BIOS diagnostic.
    // The GIF runtime reads guest EE RAM directly; CHCR writes through the
    // SystemBus synchronously submit the DMA stream to the GS software path.
    SystemBus bus(memory);
    auto intc = std::make_shared<EeInterruptController>();
    auto timers = std::make_shared<EeTimerBlock>();
    auto gif_runtime = std::make_shared<EeGifGsRuntime>(memory, 320, 240);
    auto dmac_global = std::make_shared<EeDmacGlobalRegisters>();
    auto vif1 = std::make_shared<EeVif1Registers>();
    auto vif1_fifo = std::make_shared<EeVif1Fifo>(*vif1);
    auto gif_channel = std::make_shared<EeGifDmacChannel2>(*gif_runtime, *dmac_global);

    if (!bus.map_mmio(EeInterruptController::RegisterBase,
                      EeInterruptController::RegisterSize, intc, &result.error) ||
        !bus.map_mmio(EeTimerBlock::RegisterBase, EeTimerBlock::RegisterSize,
                      timers, &result.error) ||
        !bus.map_mmio(EeGifDmacChannel2::RegisterBase,
                      EeGifDmacChannel2::RegisterSize, gif_channel, &result.error) ||
        !bus.map_mmio(EeDmacGlobalRegisters::RegisterBase,
                      EeDmacGlobalRegisters::RegisterSize, dmac_global, &result.error) ||
        !bus.map_mmio(EeVif1Registers::RegisterBase,
                      EeVif1Registers::RegisterSize, vif1, &result.error) ||
        !bus.map_mmio(EeVif1Fifo::RegisterBase, EeVif1Fifo::RegisterSize,
                      vif1_fifo, &result.error)) {
        result.image_loaded = false;
        return result;
    }

    EeExecutionRunner runner(cpu, memory, bus, *intc, *timers);
    result.execution = runner.run(instruction_budget);
    return result;
}

} // namespace retroshelf
