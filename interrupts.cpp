/**
 * @file interrupts.cpp
 * @author
 *   Adam Rebello and Marc Aoun
 */

#include "interrupts.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <tuple>

int main(int argc, char** argv) {
    // vectors: ISR addresses as strings by device index
    // delays : device delays (ms) by device index
    auto [vectors, delays] = parse_args(argc, argv);

    std::ifstream input_file(argv[1]);
    std::string trace;      // one line from trace.txt
    std::string execution;  // all output lines concatenated

    // ---- Simulation clock and constants (ms) ----
    long long current_time = 0;
    const int SWITCH_TO_KERNEL_MS   = 1;
    const int CONTEXT_SAVE_MS       = 10;
    const int FIND_VECTOR_MS        = 1;   // compute mem position
    const int GET_ISR_ADDR_MS       = 1;   // read vector
    const int ISR_ACTIVITY_CHUNK_MS = 40;  // chunk size
    const int IRET_MS               = 1;

    // helper: append one CSV line and advance time
    auto log_step = [&](int dur, const std::string& what) {
        execution += std::to_string(current_time) + ", "
                   + std::to_string(dur) + ", "
                   + what + "\n";
        current_time += dur;
    };

    // helper: run the standard interrupt flow for device `dev`
    auto interrupt_sequence = [&](int dev, const std::string& kind) {
        // defensive lookup
        int device_delay = (dev >= 0 && dev < (int)delays.size()) ? delays[dev] : 0;
        std::string isr_addr = (dev >= 0 && dev < (int)vectors.size()) ? vectors[dev] : "0x00";
        int mem_pos = dev * 2; // each vector is 2 bytes

        log_step(SWITCH_TO_KERNEL_MS, "switch to kernel mode (" + kind + ")");
        log_step(CONTEXT_SAVE_MS,     "save context");
        log_step(FIND_VECTOR_MS,      "find vector " + std::to_string(dev) +
                                      " in memory position " + std::to_string(mem_pos));
        log_step(GET_ISR_ADDR_MS,     "obtain ISR address " + isr_addr);

        // execute ISR body in 40ms chunks + remainder so total == device_delay
        int remaining = device_delay;
        while (remaining >= ISR_ACTIVITY_CHUNK_MS) {
            log_step(ISR_ACTIVITY_CHUNK_MS, "ISR activity (" + kind + ")");
            remaining -= ISR_ACTIVITY_CHUNK_MS;
        }
        if (remaining > 0) {
            log_step(remaining, "ISR activity (" + kind + ")");
        }

        log_step(IRET_MS, "IRET");
    };

    // ---- Parse each line of the trace and simulate ----
    while (std::getline(input_file, trace)) {
        auto [activity, duration_or_dev] = parse_trace(trace);

        if (activity == "CPU") {
            int burst = duration_or_dev;
            if (burst > 0) log_step(burst, "CPU burst");
        } else if (activity == "SYSCALL") {
            int dev = duration_or_dev;
            interrupt_sequence(dev, "SYSCALL");
        } else if (activity == "END_IO") {
            int dev = duration_or_dev;
            interrupt_sequence(dev, "END_IO");
        } else {
            // keep output deterministic if a weird token appears
            log_step(0, "UNKNOWN activity: " + activity);
        }
    }

    input_file.close();
    write_output(execution);
    return 0;
}
