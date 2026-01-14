#include "TriangleDemo.hpp"
#include "pico/rand.h"

TriangleDemo::TriangleDemo() {
    for (int i = 0; i < 15; i++) {
        triangles.push_back(Triangle());
    }
}

void TriangleDemo::update() {
    for (auto it = rasterizer->shapes.begin(); it != rasterizer->shapes.end(); ++it)
        delete* it;
    rasterizer->shapes.clear();

    for (auto it = triangles.begin(); it != triangles.end(); ++it) {
        Triangle& t = *it;

        if (t.pos.x + t.dir.x < 0.0f ||
            t.pos.x + t.dir.x + 100.0f > rasterizer->screenWidth()) {
            t.dir.x = t.dir.x * -1;
        }
        if (t.pos.y + t.dir.y < 0.0f ||
            t.pos.y + t.dir.y + 100.0f > rasterizer->screenHeight()) {
            t.dir.y = t.dir.y * -1;
        }

        t.pos += t.dir;
    }

    ConvexShape* background = ConvexShape::create(
        {
            { x: 0.0f, y : 0.0f },
            { x: 0.0f, y : (float) rasterizer->screenHeight() }
        },
        {
            { x: (float) rasterizer->screenWidth(), y : 0.0f },
            { x: (float) rasterizer->screenWidth(), y : (float) rasterizer->screenHeight() },
        },
        0.0f,
        new ColorFill(0xFFFFFFFF)
        );
    rasterizer->shapes.push_back(background);

    for (int i = 0; i < triangles.size(); i++) {
        rasterizer->shapes.push_back(triangles[i].toShape(i + 1));
    }
}


Triangle::Triangle() :
    pos({
        .x = get_rand_32() % (640 - 100),
         .y = get_rand_32() % (480 - 100)
        }),
    dir({
        .x = (float) (sqrt(get_rand_32() % 10000) - 50) / 50,
        .y = (float) (sqrt(get_rand_32() % 10000) - 50) / 50,
        }),
        color(get_rand_32() & 0x00FFFFFF | 0xAA000000) {
}


// https://stackoverflow.com/questions/65986056/is-there-a-non-looping-unsigned-32-bit-integer-square-root-function-c
uint32_t Triangle::sqrt(uint32_t x) {
    int lz = __builtin_clz(x | 1) & 30;
    x <<= lz;
    uint32_t y = 1 + (x >> 30);
    y = (y << 1) + (x >> 27) / y;
    y = (y << 3) + (x >> 21) / y;
    y = (y << 7) + (x >> 9) / y;
    y -= x < (uint32_t)y * y;
    return y >> (lz >> 1);
}


ConvexShape* Triangle::toShape(float z) {
    return ConvexShape::create(
        {
            {
                .x = pos.x + 50.0f,
                .y = pos.y
            },
            {
                .x = pos.x,
                .y = pos.y + 100.0f
            }
        },
        {
            {
                .x = pos.x + 50.0f,
                .y = pos.y
            },
            {
                .x = pos.x + 100.0f,
                .y = pos.y + 100.0f
            }
        },
        z,
        new ColorFill(color)
    );
}