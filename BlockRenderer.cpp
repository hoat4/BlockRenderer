#include <stdio.h>
#include "DVIDisplay.hpp"
#include "pico/stdlib.h"

#include "BlockRenderer.hpp"
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
    Scene* scene;
    try {
        scene = new Cubes();
    } catch (char const* s) {
        panic(s);
    }
    scene->rasterizer = &rasterizer;

    int expectedY = 0;
    try {
        scene->update();
    } catch (char const* s) {
        printf("can't update scene\n");
        panic(s);
    }
    try {
        rasterizer.beginFrame();
    } catch (char const* s) {
        printf("can't begin frame\n");
        panic(s);
    }
    rasterizer.prepareLine();

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
            rasterizer.prepareLine();
        } else if (y > expectedY) {
            rasterizer.skipLines(y - expectedY);
            rasterizer.prepareLine();
        }
        expectedY = y + 1;
        if (expectedY == display.height())
            expectedY = 0;

        //if (y%10 == 0)
        //    printf("R%d\n", y);
        try {
            absolute_time_t t2 = get_absolute_time();
            rasterizer.renderLine(lineBuffer.buffer);
            absolute_time_t t3 = get_absolute_time();
            if (absolute_time_diff_us(t2, t3) >= 30)
                printf("Rendering row %d took %lld microseconds\n", y, absolute_time_diff_us(t2, t3));
        } catch (const char* msg) {
            panic("failed to render line %d: %s\n", y, msg);
        }

        if (expectedY == 0) {
            scene->update();
            rasterizer.beginFrame();
        }
        try {
            rasterizer.prepareLine();
        } catch (const char* msg) {
            panic("failed to prepare line %d: %s\n", y, msg);
        }

        /*
        for (int i = 0; i < 640; i++)
        lineBuffer.buffer[i] = 24533;
        */

        rasterizer.waitForLineFinish();
        lineBuffer.doneWriting();

        // TODO enélkül most valamiért szétesik ("DVI output stalled")
        //if (lineBuffer.lineNumber == 0)
        //printf("%d\n", lineBuffer.lineNumber);
        //sleep_ms(10);
    }
}
