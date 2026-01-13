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

        if (t.pos.x + t.dir.x < FixedPointNumber::zero() ||
            t.pos.x + t.dir.x + FixedPointNumber::ofInt(100) > FixedPointNumber::ofInt(rasterizer->screenWidth())) {
            t.dir.x = t.dir.x * -1;
        }
        if (t.pos.y + t.dir.y < FixedPointNumber::zero() ||
            t.pos.y + t.dir.y + FixedPointNumber::ofInt(100) > FixedPointNumber::ofInt(rasterizer->screenHeight())) {
            t.dir.y = t.dir.y * -1;
        }

        t.pos += t.dir;
    }

    ConvexShape* background = ConvexShape::create(
        {
            { x: FixedPointNumber::zero(), y : FixedPointNumber::zero() },
            { x: FixedPointNumber::zero(), y : FixedPointNumber::ofInt(rasterizer->screenHeight()) }
        },
        {
            { x: FixedPointNumber::ofInt(rasterizer->screenWidth()), y : FixedPointNumber::zero() },
            { x: FixedPointNumber::ofInt(rasterizer->screenWidth()), y : FixedPointNumber::ofInt(rasterizer->screenHeight()) },
        },
        FixedPointNumber::ofInt(0),
        new ColorFill(0xFFFFFFFF)
        );
    rasterizer->shapes.push_back(background);

    for (int i = 0; i < triangles.size(); i++) {
        rasterizer->shapes.push_back(triangles[i].toShape(FixedPointNumber::ofInt(i + 1)));
    }
}


Triangle::Triangle() :
    pos({
        .x = FixedPointNumber::ofInt(get_rand_32() % (640 - 100)),
         .y = FixedPointNumber::ofInt(get_rand_32() % (480 - 100))
        }),
    dir({
        .x = FixedPointNumber::ofInt(sqrt(get_rand_32() % 10000) - 50) / 50,
        .y = FixedPointNumber::ofInt(sqrt(get_rand_32() % 10000) - 50) / 50,
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
        new ColorFill(color)
    );
}