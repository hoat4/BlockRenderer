#pragma once

#include <vector>
#include <string>
#include "Rasterizer.hpp"
#include <stdint.h>

class ConvexShape;

struct LerpableData {
    /**
     * Value at PerspectiveTextureFill.baseX/Y
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
 * Nem valódi perspektíva, mert csak 8px-enként van inverz projekció végrehajtva,
 * azok között lineáris interpoláció van (emiatt 8px-nél keskenyebb alakzatoknál nincs értelme használni).
 */
struct PerspectiveTextureMapping {

    float baseX;
    float baseY;
    LerpableData zReciprocal;
    LerpableData uPerZ;
    LerpableData vPerZ;
};

struct InterpolatedTextureMapping {

    int baseX;
    int baseY;
    int uByX;
    int uByY;
    int vByX;
    int vByY;
};

union TextureMapping {

    PerspectiveTextureMapping perspective;
    InterpolatedTextureMapping interpolated;
};


/**
 * 16x16-os méret, átlátszóság nélkül.
 */
struct Fill {

    const uint16_t* texture;

    bool isInterpolated;
    TextureMapping textureMapping;

    std::string toString();
};

