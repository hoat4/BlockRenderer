#include <stdio.h>
#include "DVIDisplay.hpp"
#include "pico/stdlib.h"

#include "BlockRenderer.hpp"
#include "TriangleDemo.hpp"
#include "Cubes.hpp"

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

    Game game(display.width(), display.height());

    while (true) {
        LineBuffer lineBuffer = display.nextLine();
        game.renderLine(lineBuffer.lineNumber, lineBuffer.buffer);

        // TODO enélkül most valamiért szétesik ("DVI output stalled")
        //if (lineBuffer.lineNumber == 0)
        //printf("%d\n", lineBuffer.lineNumber);
        //sleep_ms(10);
    }
}

Game::Game(int width, int height) :
    width(width), height(height), rasterizer(Rasterizer(width, height)) {

    // induláskor érvénytelen értékre állítjuk, hogy első frameet új framenek
    // érzékelje
    expectedY = height;

    scene = new Cubes();
    scene->rasterizer = &rasterizer;
}

void Game::renderLine(int y, uint16_t* buffer) {
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
        rasterizer.renderLine(buffer);
    } catch (const char* msg) {
        panic("failed to render line %d: %s\n", y, msg);
    }
}
