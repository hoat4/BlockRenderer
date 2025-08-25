#include <stdio.h>
#include "DVIDisplay.hpp"
#include "hardware/clocks.h"
#include "pico/stdlib.h"

void vacak() {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
}

int main() {
    DVIDisplay::initSystemClocks();
    stdio_init_all();

    Pinout pinout = {
        .clk_p = 13,
        .rgb_p = {15, 17, 19}
    };

    DVIDisplay display(pinout);
    while (true) {
        LineBuffer lineBuffer = display.nextLine();
        int w = lineBuffer.lineNumber;
        uint16_t* dst_ptr = lineBuffer.buffer;
        for (int i = 0; i < w; i++) {
            *(dst_ptr++) = 0xF000;
        }
        for (int i = w; i < display.width(); ++i) {
            *(dst_ptr++) = (i & 128) ? 0xFF00 : 0x00FF;
        }
        //printf("%d\n", lineBuffer.lineNumber);
        //sleep_ms(10);
    }
}
