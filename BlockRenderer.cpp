#include <stdio.h>
#include "DVIDisplay.hpp"
#include "pico/stdlib.h"

#include "BlockRenderer.hpp"
#include "TriangleDemo.hpp"
#include "Cubes.hpp"

int main() {
    DVIDisplay::initSystemClocks();
    stdio_init_all();

    Pinout pinout = {
        .clk_p = 13,
        .rgb_p = {15, 17, 19}
    };

    DVIDisplay display(pinout);

    Rasterizer rasterizer(display.width(), display.height());
    Scene* scene = new Cubes();
    scene->rasterizer = &rasterizer;

    // induláskor érvénytelen értékre állítjuk, hogy első frameet új framenek
    // érzékelje
    int expectedY = display.height();

    while (true) {
        LineBuffer lineBuffer = display.nextLine();
        int y = lineBuffer.lineNumber;
        //printf("%d\n", y);

        if (y < expectedY) {
            // new frame

            scene->update();
            rasterizer.beginFrame();
            if (y != 0)
                rasterizer.skipLines(y);
        } else if (y > expectedY) {
            rasterizer.skipLines(y - expectedY);
        }
        expectedY = y + 1;

        //if (y%10 == 0)
        //    printf("R%d\n", y);
        try {
            absolute_time_t t1 = get_absolute_time();
            rasterizer.prepareLine();
            absolute_time_t t2 = get_absolute_time();
            rasterizer.renderLine(lineBuffer.buffer);
            absolute_time_t t3 = get_absolute_time();
            if (absolute_time_diff_us(t1, t3) >= 30&&false)
                printf("Rendering row %d took %lld microseconds (prepare %lld, render %lld)\n",
                       y,
                       absolute_time_diff_us(t1, t3),
                       absolute_time_diff_us(t1, t2),
                       absolute_time_diff_us(t2, t3));
        } catch (const char* msg) {
            panic("failed to render line %d: %s\n", y, msg);
        }
        /*
        for (int i = 0; i < 640; i++)
        lineBuffer.buffer[i] = 24533;
        */

        lineBuffer.doneWriting();

        // TODO enélkül most valamiért szétesik ("DVI output stalled")
        //if (lineBuffer.lineNumber == 0)
        //printf("%d\n", lineBuffer.lineNumber);
        //sleep_ms(10);
    }
}
