#pragma once

#include <vector>
#include <string>
#include "Rasterizer.hpp"
#include <stdint.h>

typedef uint32_t Color; // TODO

class ConvexShape;

class Fill {
public:
    /**
     * from != to
     */
    virtual void fillSpan(uint16_t* from, uint16_t* to,
                          int x, int y);
    virtual std::string toString() = 0;
};

class ColorFill : public Fill {

public:
    Color color;

    ColorFill(Color color) : color(color) {}

    virtual void fillSpan(uint16_t* from, uint16_t* to,
                          int x, int y);
    virtual std::string toString();

    static void initDMAChannel();
    static void waitForDMAChannelToFinish();

private:
    static Color blendColorsDstOpaque(Color dst, Color src);
};

struct LerpableData {
    /**
     * Value at screen left top (0, 0)
     */
    float base;
    float byX;
    float by8X;
    float byY;

    float valueAt(float x, float y) {
        return base + x * byX + y * byY;
    }
};

/**
 * 16x16-os méret, átlátszóság nélkül
 */
class TextureFill : public Fill {

public:
    const uint16_t* texture;
    float baseX;
    float baseY;
    LerpableData zReciprocal;
    LerpableData uPerZ;
    LerpableData vPerZ;

    TextureFill() {}

    static void initInterpolator();

    virtual void fillSpan(uint16_t* from, uint16_t* to,
                          int x, int y);
    virtual std::string toString();

    static const int UV_FRACTIONAL_BITS = 12;
};
