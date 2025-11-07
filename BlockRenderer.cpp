#include <stdio.h>
#include "DVIDisplay.hpp"
#include "pico/stdlib.h"

#include "BlockRenderer.hpp"

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

    // érvénytelen értékre állítjuk induláskor, hogy az első frameet új framenek érzékelje
    int expectedY = display.height();
    while (true) {
        LineBuffer lineBuffer = display.nextLine();
        game.renderLine(lineBuffer.lineNumber, lineBuffer.buffer);

        //printf("%d\n", lineBuffer.lineNumber);
        //sleep_ms(10);
    }
}

Game::Game(int width, int height) :
    width(width), height(height), rasterizer(Rasterizer(width, height)) {

    // induláskor érvénytelen értékre állítjuk, hogy első frameet új framenek
    // érzékelje
    expectedY = height;
}

void Game::renderLine(int y, uint16_t* buffer) {
    if (y < expectedY) {
        // new frame

        beginFrame();

        if (y != 0)
            rasterizer.skipLines(y);
    } else if (y > expectedY) {
        rasterizer.skipLines(y - expectedY);
    }
    expectedY = y + 1;

    //if (y%10 == 0)
    //    printf("R%d\n", y);
    rasterizer.renderLine(buffer);
}

void Game::beginFrame() {
    rasterizer.quads.clear();

    Quad2D left(
        { x: FixedPointNumber::ofInt(200), y : FixedPointNumber::zero() },
        { x: FixedPointNumber::ofInt(200), y : FixedPointNumber::ofInt(height) },
        { x: FixedPointNumber::zero(), y : FixedPointNumber::ofInt(height) },
        { x: FixedPointNumber::zero(), y : FixedPointNumber::zero() },
        0xFFF0
    );
    Quad2D center(
        { x: FixedPointNumber::ofInt(300), y : FixedPointNumber::zero() },
        { x: FixedPointNumber::ofInt(400), y : FixedPointNumber::ofInt(height) },
        { x: FixedPointNumber::ofInt(200), y : FixedPointNumber::ofInt(height) },
        { x: FixedPointNumber::ofInt(200), y : FixedPointNumber::zero() },
        0x0FFF
    );
    Quad2D rightTop(
        { x: FixedPointNumber::ofInt(width), y : FixedPointNumber::zero() },
        { x: FixedPointNumber::ofInt(width), y : FixedPointNumber::ofInt(240) },
        { x: FixedPointNumber::ofInt(400), y : FixedPointNumber::ofInt(240) },
        { x: FixedPointNumber::ofInt(400), y : FixedPointNumber::zero() },
        0xF0FF
    );
    Quad2D rightBottom(
        { x: FixedPointNumber::ofInt(width), y : FixedPointNumber::ofInt(240) },
        { x: FixedPointNumber::ofInt(width), y : FixedPointNumber::ofInt(height) },
        { x: FixedPointNumber::ofInt(400), y : FixedPointNumber::ofInt(height) },
        { x: FixedPointNumber::ofInt(400), y : FixedPointNumber::ofInt(240) },
        0xF000
    );

    rasterizer.quads.push_back(left);
    rasterizer.quads.push_back(center);
    rasterizer.quads.push_back(rightTop);
    rasterizer.quads.push_back(rightBottom);

    rasterizer.beginFrame();
}