#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "board.h"

// Core 1: display and animations
static void core1_main(void) {
    while (true) {
        tight_loop_contents();
    }
}

// Core 0: key scan and MIDI output
int main(void) {
    stdio_init_all();
    multicore_launch_core1(core1_main);

    while (true) {
        tight_loop_contents();
    }
}
