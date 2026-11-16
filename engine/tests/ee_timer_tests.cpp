#include "retroshelf/ee_timer.hpp"
#include "retroshelf/ee_execution_runner.hpp"
#include "retroshelf/ee_interrupt_controller.hpp"
#include "retroshelf/cpu.hpp"
#include "retroshelf/ps2_ee_memory_map.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
std::vector<std::uint8_t> word(std::uint32_t v) {
    return {static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v >> 8U),
            static_cast<std::uint8_t>(v >> 16U), static_cast<std::uint8_t>(v >> 24U)};
}
}

int main() {
    int failures = 0;
    const auto check = [&failures](bool ok, const char* label) {
        if (!ok) { std::cerr << "FAIL: " << label << "\n"; ++failures; }
        else std::cout << "PASS: " << label << "\n";
    };
    auto timers = std::make_shared<retroshelf::EeTimerBlock>();
    std::vector<std::uint8_t> ram(64, 0);
    retroshelf::SystemBus bus(ram);
    std::string error;
    check(bus.map_mmio(retroshelf::EeTimerBlock::RegisterBase,
                       retroshelf::EeTimerBlock::RegisterSize, timers, &error),
          "maps all four EE timer register channels");
    const auto base = retroshelf::EeTimerBlock::RegisterBase;

    timers->advance_cycles(100);
    check(timers->count(0) == 0, "timer remains stopped after reset");
    check(bus.write(base + retroshelf::EeTimerBlock::CompareOffset, word(3), &error) &&
              bus.write(base + retroshelf::EeTimerBlock::ModeOffset,
                        word(retroshelf::EeTimerBlock::ModeCountEnable |
                             retroshelf::EeTimerBlock::ModeCompareEnable |
                             retroshelf::EeTimerBlock::ModeZeroReturn), &error),
          "programs compare, zero-return and count-enable");
    timers->advance_cycles(3);
    check(timers->count(0) == 0 &&
              (timers->mode(0) & retroshelf::EeTimerBlock::ModeCompareFlag) != 0U,
          "compare match raises EQUF and zero-return resets COUNT");
    check(timers->pending_interrupts() == 1U,
          "enabled compare flag exposes timer interrupt request");

    check(bus.write(base + retroshelf::EeTimerBlock::ModeOffset,
                    word(retroshelf::EeTimerBlock::ModeCountEnable |
                         retroshelf::EeTimerBlock::ModeCompareEnable |
                         retroshelf::EeTimerBlock::ModeZeroReturn |
                         retroshelf::EeTimerBlock::ModeCompareFlag), &error) &&
              timers->pending_interrupts() == 0U,
          "writing EQUF clears compare status without disabling control bits");

    const auto ch1 = base + retroshelf::EeTimerBlock::ChannelStride;
    check(bus.write(ch1 + retroshelf::EeTimerBlock::ModeOffset,
                    word(retroshelf::EeTimerBlock::ModeCountEnable |
                         (1U << 0U)), &error),
          "configures independent divided-clock channel");
    timers->advance_cycles(15);
    check(timers->count(1) == 0, "divide-by-16 timer does not tick early");
    timers->advance_cycles(1);
    check(timers->count(1) == 1, "divide-by-16 timer advances on sixteenth bus cycle");

    check(bus.write(ch1 + retroshelf::EeTimerBlock::ModeOffset,
                    word(retroshelf::EeTimerBlock::ModeCountEnable |
                         retroshelf::EeTimerBlock::ModeOverflowEnable |
                         1U), &error),
          "enables overflow interrupt on timer channel 1");
    check(bus.write(ch1 + retroshelf::EeTimerBlock::CountOffset, word(0xFFFF), &error),
          "loads 16-bit timer counter");
    timers->advance_cycles(16);
    check(timers->count(1) == 0 &&
              (timers->mode(1) & retroshelf::EeTimerBlock::ModeOverflowFlag) != 0U,
          "counter wrap sets OVFF");
    check((timers->pending_interrupts() & (1U << 1U)) != 0U,
          "enabled overflow flag asserts timer channel 1 interrupt request");
    check(bus.write(ch1 + retroshelf::EeTimerBlock::ModeOffset,
                    word(retroshelf::EeTimerBlock::ModeCountEnable |
                         retroshelf::EeTimerBlock::ModeOverflowEnable |
                         retroshelf::EeTimerBlock::ModeOverflowFlag |
                         1U), &error) &&
              (timers->pending_interrupts() & (1U << 1U)) == 0U,
          "writing OVFF clears overflow interrupt without disabling timer");
    // A compare value of zero coincides with the 16-bit overflow edge.
    // Both status flags must latch, and clearing either one must leave the
    // other channel interrupt source asserted.
    const auto ch2 = base + 2U * retroshelf::EeTimerBlock::ChannelStride;
    check(bus.write(ch2 + retroshelf::EeTimerBlock::CompareOffset, word(0), &error) &&
              bus.write(ch2 + retroshelf::EeTimerBlock::CountOffset, word(0xFFFF), &error) &&
              bus.write(ch2 + retroshelf::EeTimerBlock::ModeOffset,
                        word(retroshelf::EeTimerBlock::ModeCountEnable |
                             retroshelf::EeTimerBlock::ModeCompareEnable |
                             retroshelf::EeTimerBlock::ModeOverflowEnable), &error),
          "arms simultaneous compare-zero and overflow events");
    timers->advance_cycles(1);
    const std::uint32_t both_timer_flags =
        retroshelf::EeTimerBlock::ModeCompareFlag |
        retroshelf::EeTimerBlock::ModeOverflowFlag;
    check(timers->count(2) == 0 &&
              (timers->mode(2) & both_timer_flags) == both_timer_flags,
          "counter rollover at compare zero latches EQUF and OVFF together");
    check((timers->pending_interrupts() & (1U << 2U)) != 0U,
          "simultaneous compare and overflow share the channel interrupt request");
    check(bus.write(ch2 + retroshelf::EeTimerBlock::ModeOffset,
                    word(retroshelf::EeTimerBlock::ModeCountEnable |
                         retroshelf::EeTimerBlock::ModeCompareEnable |
                         retroshelf::EeTimerBlock::ModeOverflowEnable |
                         retroshelf::EeTimerBlock::ModeCompareFlag), &error) &&
              (timers->mode(2) & retroshelf::EeTimerBlock::ModeOverflowFlag) != 0U &&
              (timers->mode(2) & retroshelf::EeTimerBlock::ModeCompareFlag) == 0U &&
              (timers->pending_interrupts() & (1U << 2U)) != 0U,
          "clearing EQUF preserves OVFF and its pending interrupt");
    check(bus.write(ch2 + retroshelf::EeTimerBlock::ModeOffset,
                    word(retroshelf::EeTimerBlock::ModeCountEnable |
                         retroshelf::EeTimerBlock::ModeCompareEnable |
                         retroshelf::EeTimerBlock::ModeOverflowEnable |
                         retroshelf::EeTimerBlock::ModeOverflowFlag), &error) &&
              (timers->pending_interrupts() & (1U << 2U)) == 0U,
          "clearing the remaining OVFF deasserts the timer channel request");

    std::vector<std::uint8_t> invalid_read;
    check(!bus.read(base + 4U, 4, invalid_read, &error) && invalid_read.empty(),
          "rejects unimplemented register offsets");
    check(!bus.write(base + 1U, word(1), &error),
          "rejects unaligned timer register writes");
    check(timers->count(4) == 0, "out-of-range channel query is safe");

    // Verify the full timer -> INTC -> COP0 interrupt path while executing EE instructions.
    retroshelf::Ps2EeMemoryMap ee_memory(64U * 1024U);
    retroshelf::SystemBus ee_bus(ee_memory);
    auto runtime_timers = std::make_shared<retroshelf::EeTimerBlock>();
    auto intc = std::make_shared<retroshelf::EeInterruptController>();
    check(ee_bus.map_mmio(retroshelf::EeTimerBlock::RegisterBase,
                          retroshelf::EeTimerBlock::RegisterSize, runtime_timers, &error) &&
              ee_bus.map_mmio(retroshelf::EeInterruptController::RegisterBase,
                              retroshelf::EeInterruptController::RegisterSize, intc, &error),
          "maps timers and INTC into the EE execution bus");
    check(ee_bus.write(retroshelf::EeTimerBlock::RegisterBase +
                       retroshelf::EeTimerBlock::CompareOffset, word(2), &error) &&
              ee_bus.write(retroshelf::EeTimerBlock::RegisterBase +
                       retroshelf::EeTimerBlock::ModeOffset,
                       word(retroshelf::EeTimerBlock::ModeCountEnable |
                            retroshelf::EeTimerBlock::ModeCompareEnable), &error) &&
              ee_bus.write(retroshelf::EeInterruptController::RegisterBase +
                       retroshelf::EeInterruptController::MaskOffset, word(1U << 9U), &error),
          "arms timer 0 compare interrupt and unmasks its INTC line");
    constexpr std::uint32_t program = 0x80001000U;
    const std::uint32_t instructions[] = {
        0x24080401U, // ADDIU t0, zero, 0x401 (COP0 IE + IM2)
        0x40886000U, // MTC0 t0, Status
        0x00000000U  // Interrupt must be accepted before this NOP
    };
    for (std::size_t i = 0; i < 3U; ++i)
        check(ee_memory.write(program + static_cast<std::uint32_t>(i * 4U),
                              4U, instructions[i], &error),
              "writes timer interrupt test instruction");
    check(ee_memory.write(static_cast<std::uint32_t>(retroshelf::Cpu::GeneralExceptionVector),
                          4U, 0U, &error), "maps EE general exception vector");
    retroshelf::Cpu ee_cpu;
    ee_cpu.set_pc(program);
    retroshelf::EeExecutionRunner runtime_runner(ee_cpu, ee_memory, ee_bus, *intc,
                                                  *runtime_timers);
    const auto runtime_result = runtime_runner.run(3U);
    check(runtime_result.trace.size() == 3U &&
              runtime_result.trace[2].exception_taken &&
              runtime_result.trace[2].exception_code == 0U,
          "timer compare reaches COP0 as an EE external interrupt");
    check((intc->status() & (1U << 9U)) != 0U &&
              (ee_cpu.cop0_cause() & retroshelf::Cpu::CauseEeInterruptPending) != 0U &&
              ee_cpu.cop0_epc() == program + 8U,
          "timer IRQ is latched in INTC and records the interrupted PC");

    // COP0 Count/Compare must also reach the CPU interrupt gate through
    // the runner, independently of the four external EE timer channels.
    retroshelf::Ps2EeMemoryMap count_memory(64U * 1024U);
    retroshelf::SystemBus count_bus(count_memory);
    retroshelf::Cpu count_cpu;
    constexpr std::uint32_t count_program = 0x80002000U;
    count_cpu.set_reg(8, 3U);
    count_cpu.set_reg(9, 0x00008001U); // Status.IE + Status.IM7
    const std::uint32_t count_instructions[] = {
        (0x10U << 26U) | (4U << 21U) | (8U << 16U) | (11U << 11U), // MTC0 r8, Compare
        (0x10U << 26U) | (4U << 21U) | (9U << 16U) | (12U << 11U), // MTC0 r9, Status
        0U, 0U, 0U // Count reaches Compare after third instruction
    };
    for (std::size_t i = 0; i < 5U; ++i)
        check(count_memory.write(count_program + static_cast<std::uint32_t>(i * 4U),
                                 4U, count_instructions[i], &error),
              "writes COP0 Count/Compare runner instruction");
    check(count_memory.write(static_cast<std::uint32_t>(retroshelf::Cpu::GeneralExceptionVector),
                             4U, 0U, &error),
          "maps COP0 timer exception vector");
    retroshelf::EeExecutionRunner count_runner(count_cpu, count_memory, count_bus);
    const auto count_result = count_runner.run(5U);
    std::cerr << "COUNTDBG trace=" << count_result.trace.size()
              << " pc=" << std::hex << count_cpu.pc()
              << " count=" << count_cpu.reg(0)
              << " cause=" << count_cpu.cop0_cause()
              << " status=" << count_cpu.cop0_status()
              << " epc=" << count_cpu.cop0_epc() << std::dec << "\\n";
    for (std::size_t debug_i = 0; debug_i < count_result.trace.size(); ++debug_i)
        std::cerr << "COUNTDBG[" << debug_i << "] pc=" << std::hex
                  << count_result.trace[debug_i].pc << " ins="
                  << count_result.trace[debug_i].instruction << " ex="
                  << count_result.trace[debug_i].exception_taken << std::dec << "\\n";
    check(count_result.trace.size() == 5U &&
              count_result.trace[3].exception_taken &&
              count_result.trace[3].exception_code == 0U,
          "COP0 Count/Compare raises an interrupt through the EE runner");
    check(count_cpu.cop0_epc() == count_program + 12U &&
              (count_cpu.cop0_cause() & retroshelf::Cpu::CauseTimerInterruptPending) != 0U,
          "COP0 timer interrupt records EPC and retains IP7 pending state");

    return failures == 0 ? 0 : 1;
}
