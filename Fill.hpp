#pragma once

#include <vector>
#include <string>
#include "Rasterizer.hpp"
#include <stdint.h>

typedef uint32_t Color; // TODO

class ConvexShape;

class Fill {
public:
    virtual bool isOpaque() = 0;
    virtual void fillSpan(uint16_t* from, uint16_t* to,
                          std::vector<ConvexShape*>& shapeStack,
                          int relativeX, int relativeY);
    virtual std::string toString() = 0;
};

class ColorFill : public Fill {

public:
    Color color;

    ColorFill(Color color) : color(color) {}

    virtual bool isOpaque();
    virtual void fillSpan(uint16_t* from, uint16_t* to,
                          std::vector<ConvexShape*>& shapeStack,
                          int relativeX, int relativeY);
    virtual std::string toString();

private:
    static Color blendColorsDstOpaque(Color dst, Color src);
};

struct LerpableData {
    /**
     * Value at screen left top (0, 0)
     */
    float base;
    float byX;
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

    virtual bool isOpaque();
    virtual void fillSpan(uint16_t* from, uint16_t* to,
                          std::vector<ConvexShape*>& shapeStack,
                          int relativeX, int relativeY);
    virtual std::string toString();
};
