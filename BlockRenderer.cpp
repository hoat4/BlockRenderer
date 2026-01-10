#include <stdio.h>
#include "DVIDisplay.hpp"
#include "pico/stdlib.h"
#include "pico/rand.h"

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
    width(width), height(height), rasterizer(Rasterizer(width, height)) {

    // induláskor érvénytelen értékre állítjuk, hogy első frameet új framenek
    // érzékelje
    expectedY = height;

    for (int i = 0; i < 15; i++) {
        triangles.push_back(Triangle());
    }
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
    } catch (const char* msg) {
        panic("failed to render line %d: %s\n", y, msg);
    }
}

void Game::update() {
    for (auto it = triangles.begin(); it != triangles.end(); ++it) {
        Triangle& t = *it;

        if (t.pos.x + t.dir.x < FixedPointNumber::zero() ||
            t.pos.x + t.dir.x + FixedPointNumber::ofInt(100) > FixedPointNumber::ofInt(width)) {
            t.dir.x = t.dir.x * -1;
        }
        if (t.pos.y + t.dir.y < FixedPointNumber::zero() ||
            t.pos.y + t.dir.y + FixedPointNumber::ofInt(100) > FixedPointNumber::ofInt(height)) {
            t.dir.y = t.dir.y * -1;
        }

        t.pos += t.dir;
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
        0xFFFFFFFF
        );
    rasterizer.shapes.push_back(background);

    for (int i = 0; i < triangles.size(); i++) {
        rasterizer.shapes.push_back(triangles[i].toShape(FixedPointNumber::ofInt(i + 1)));
    }

    rasterizer.beginFrame();
}

Triangle::Triangle() :
    pos({
        .x = FixedPointNumber::ofInt(get_rand_32() % (640 - 100)),
         .y = FixedPointNumber::ofInt(get_rand_32() % (480 - 100))
        }),
    dir({
        .x = FixedPointNumber::ofInt(get_rand_32() % 400 - 200) / 100,
        .y = FixedPointNumber::ofInt(get_rand_32() % 400 - 200) / 100
        }),
    color(get_rand_32() & 0x00FFFFFF | 0xAA000000) {
}

ConvexShape* Triangle::toShape(FixedPointNumber z) {
    return ConvexShape::create(
        {
            {
                .x = pos.x + FixedPointNumber::ofInt(50),
                .y = pos.y
            },
            {
                .x = pos.x,
                .y = pos.y + FixedPointNumber::ofInt(100)
            }
        },
        {
            {
                .x = pos.x + FixedPointNumber::ofInt(50),
                .y = pos.y
            },
            {
                .x = pos.x + FixedPointNumber::ofInt(100),
                .y = pos.y + FixedPointNumber::ofInt(100)
            }
        },
        z,
        color
    );
}