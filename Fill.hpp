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
 * 16x16-os méret, átlátszóság nélkül. Nem valódi perspektíva, mert csak 8px-enként van inverz projekció végrehajtva,
 * azok között lineáris interpoláció van (emiatt 8px-nél keskenyef).
 */
class PerspectiveTextureFill : public Fill {

public:
    const uint16_t* texture;
    float baseX;
    float baseY;
    LerpableData zReciprocal;
    LerpableData uPerZ;
    LerpableData vPerZ;

    PerspectiveTextureFill() {}

    static void initInterpolator();

    virtual void fillSpan(uint16_t* from, uint16_t* to,
                          int x, int y);
    virtual std::string toString();

    static const int UV_FRACTIONAL_BITS = 12;
    static const int SCANLINE_SUBDIVISION_BITS = 3;
    // PerspectiveTextureFillnek nem kell padding mert van benne __usat, 
    // de InterpolatedTextureFillben viszont igen

private:
    static const int MAX_TEXTURE_ROW_BITS = 5;
    static const int SCANLINE_SUBDIVISION = 1 << SCANLINE_SUBDIVISION_BITS;
};


struct LerpableDataInt {

    /**
     * Value at InterpolatedTextureFill.baseX/Y
     */
    int base;
    int byX;
    int byY;

    int valueAt(int x, int y) {
        return base + x * byX + y * byY;
    }
};

/**
 * 16x16-os méret, átlátszóság nélkül. Nem valódi perspektíva, hanem 8px-enként van újraszámolva.
 */
class InterpolatedTextureFill : public Fill {

public:
    const uint16_t* texture;

    int baseX;
    int baseY;
    int uByX;
    int uByY;
    int vByX;
    int vByY;
    
    InterpolatedTextureFill() {}

    virtual void fillSpan(uint16_t* from, uint16_t* to,
                          int x, int y);
    virtual std::string toString();

    static const int UV_FRACTIONAL_BITS =
        PerspectiveTextureFill::UV_FRACTIONAL_BITS +
        PerspectiveTextureFill::SCANLINE_SUBDIVISION_BITS;
};
