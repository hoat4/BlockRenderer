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

    while (true) {
        LineBuffer lineBuffer = display.nextLine();
        game.renderLine(lineBuffer.lineNumber, lineBuffer.buffer);

        //printf("%d\n", lineBuffer.lineNumber);
        //sleep_ms(10);
    }
}

Game::Game(int width, int height) :
    width(width), height(height), rasterizer(Rasterizer(width, height)),
    rect1Pos({ .x = FixedPointNumber::ofInt(300), .y = FixedPointNumber::ofInt(200) }),
    rect2Pos({ .x = FixedPointNumber::ofInt(50), .y = FixedPointNumber::ofInt(200) }),
    rect1Dir({ .x = FixedPointNumber::ofInt(1), .y = FixedPointNumber::ofInt(1) }),
    rect2Dir({ .x = FixedPointNumber::ofInt(1), .y = FixedPointNumber::ofInt(1) }) {

    // induláskor érvénytelen értékre állítjuk, hogy első frameet új framenek
    // érzékelje
    expectedY = height;

    /*
    for (int i = 0; i <400; i++) {
        update();
    }
    */
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
    try {
        rasterizer.renderLine(buffer);
    } catch(const char* msg) {
        panic("failed to render line %d: %s\n", y, msg);
    }
}

void Game::update() {
    if (rect1Pos.x + rect1Dir.x < FixedPointNumber::zero() ||
        rect1Pos.x + rect1Dir.x + FixedPointNumber::ofInt(200) > FixedPointNumber::ofInt(width)) {
        rect1Dir.x = rect1Dir.x * -1;
    }
    if (rect1Pos.y + rect1Dir.y < FixedPointNumber::zero() ||
        rect1Pos.y + rect1Dir.y + FixedPointNumber::ofInt(200) > FixedPointNumber::ofInt(height)) {
        rect1Dir.y = rect1Dir.y * -1;
    }
    if (rect2Pos.x + rect2Dir.x < FixedPointNumber::zero() ||
        rect2Pos.x + rect2Dir.x + FixedPointNumber::ofInt(200) > FixedPointNumber::ofInt(width)) {
        rect2Dir.x = rect2Dir.x * -1;
    }
    if (rect2Pos.y + rect2Dir.y < FixedPointNumber::zero() ||
        rect2Pos.y + rect2Dir.y + FixedPointNumber::ofInt(200) > FixedPointNumber::ofInt(height)) {
        rect2Dir.y = rect2Dir.y * -1;
    }
    if (true) {
        rect1Pos += rect1Dir;
        rect2Pos += rect2Dir;
    } else {
        rect1Pos.x = FixedPointNumber::ofInt(0);
        rect1Pos.y = FixedPointNumber::ofInt(100);
    }
}

void Game::beginFrame() {
    for (auto it = rasterizer.shapes.begin(); it != rasterizer.shapes.end(); ++it)
        delete* it;
    rasterizer.shapes.clear();

    update();

    ConvexShape* background = ConvexShape::create(
        {
            { x: FixedPointNumber::zero(), y : FixedPointNumber::zero() },
            { x: FixedPointNumber::zero(), y : FixedPointNumber::ofInt(height) }
        },
        {
            { x: FixedPointNumber::ofInt(width), y : FixedPointNumber::zero() },
            { x: FixedPointNumber::ofInt(width), y : FixedPointNumber::ofInt(height) },
        },
        FixedPointNumber::ofInt(0),
        0xFFFF
        );
    ConvexShape* center1 = ConvexShape::create(
        {
            { x: FixedPointNumber::ofInt(200), y : FixedPointNumber::ofInt(100) },
            { x: FixedPointNumber::ofInt(200), y : FixedPointNumber::ofInt(300) },
        },
        {
            { x: FixedPointNumber::ofInt(400), y : FixedPointNumber::ofInt(100) },
            { x: FixedPointNumber::ofInt(400), y : FixedPointNumber::ofInt(300) },
        },
        FixedPointNumber::ofInt(1),
        0x0FFF
        );
    ConvexShape* center2 = ConvexShape::create(
        {
            { x: rect1Pos.x + FixedPointNumber::ofInt(100), y : rect1Pos.y },
            { x: rect1Pos.x, y : rect1Pos.y + FixedPointNumber::ofInt(200) },
        },
        {
            { x: rect1Pos.x + FixedPointNumber::ofInt(100), y : rect1Pos.y },
            { x: rect1Pos.x + FixedPointNumber::ofInt(200), y : rect1Pos.y + FixedPointNumber::ofInt(200) },
        },
        FixedPointNumber::ofInt(2),
        0xFFF0
        );
    ConvexShape* center3 = ConvexShape::create(
        {
            { x: rect2Pos.x, y : rect2Pos.y },
            { x: rect2Pos.x, y : rect2Pos.y + FixedPointNumber::ofInt(200) },
        },
        {
            { x: rect2Pos.x, y : rect2Pos.y },
            { x: rect2Pos.x + FixedPointNumber::ofInt(200), y : rect2Pos.y + FixedPointNumber::ofInt(200) },
        },
        FixedPointNumber::ofInt(3),
        0x0FF0
        );

    rasterizer.shapes.push_back(background);
    rasterizer.shapes.push_back(center1);
    rasterizer.shapes.push_back(center2);
    rasterizer.shapes.push_back(center3);

    rasterizer.beginFrame();
}