#include "Fill.hpp"

#include <iomanip>
#include "hardware/interp.h"

bool ColorFill::isOpaque() {
    return (color & 0xFF000000) == 0xFF000000;
}

void ColorFill::fillSpan(uint16_t* from, uint16_t* to,
                         std::vector<ConvexShape*>& shapeStack,
                         int x, int y) {
    int firstOpaque = shapeStack.size() - 1;
    while (!shapeStack[firstOpaque]->fill->isOpaque()) {
        if (firstOpaque == 0)
            throw "no opaque shapes on stack";
        firstOpaque--;
    }

    // TODO textúrára overlayelt color esetén ez elszáll

    Color color = reinterpret_cast<ColorFill*>(shapeStack[firstOpaque]->fill)->color;
    for (int i = firstOpaque + 1; i < shapeStack.size(); i++)
        color = blendColorsDstOpaque(color,
                                     reinterpret_cast<ColorFill*>(shapeStack[i]->fill)->color);

    unsigned int r = color >> 16 & 0xFF, g = color >> 8 & 0xFF, b = color & 0xFF;

    uint16_t rgb565 = ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | ((b & 0b11111000) >> 3);

    while (from != to)
        *(from++) = rgb565;
}

std::string ColorFill::toString() {
    std::stringstream s;
    s << "#" << std::setfill('0') << std::setw(6) << std::hex << color;
    return s.str();
}

Color ColorFill::blendColorsDstOpaque(Color dst, Color src) {
    unsigned int aa = src >> 24;
    if (aa == 255) {
        return src;
    }

    unsigned int ra = src >> 16 & 0xFF, rb = dst >> 16 & 0xFF;
    unsigned int ga = src >> 8 & 0xFF, gb = dst >> 8 & 0xFF;
    unsigned int ba = src & 0xFF, bb = dst & 0xFF;

    unsigned int ro = ra * aa + rb * (255 - aa) + 127;
    unsigned int go = ga * aa + gb * (255 - aa) + 127;
    unsigned int bo = ba * aa + bb * (255 - aa) + 127;

    //printf("(%d, %d, %d, %d) over (%d, %d, %d, %d): (%d, %d, %d, %d)\n", 
    //    ra, ga, ba, aa, rb, gb, bb, ab, ro, go, bo, ao);

    return 0xFF000000 | (ro & 0xFF00) << 8 | (go & 0xFF00) | bo >> 8;
}

const int TEXTURE_STRIDE_BITS = 5;
const int TEXTURE_SIZE = 18;
const int MAX_TEXTURE_ROW_BITS = 5;
const int SCANLINE_SUBDIVISION = 8;
const int UV_FRACTIONAL_BITS = 16;

void TextureFill::initInterpolator() {
    interp_config cfg = interp_default_config();
    // set add_raw flag to use raw (un-shifted and un-masked) lane accumulator value when adding
    // it to the lane base to make the lane result
    interp_config_set_add_raw(&cfg, true);
    interp_config_set_shift(&cfg, UV_FRACTIONAL_BITS - 1);
    interp_config_set_mask(&cfg, 1, TEXTURE_STRIDE_BITS - 1 + 1);
    interp_set_config(interp0, 0, &cfg);

    interp_config_set_shift(&cfg, UV_FRACTIONAL_BITS - TEXTURE_STRIDE_BITS - 1);
    interp_config_set_mask(&cfg, TEXTURE_STRIDE_BITS + 1, TEXTURE_STRIDE_BITS + MAX_TEXTURE_ROW_BITS - 1 + 1);
    interp_set_config(interp0, 1, &cfg);
}

void TextureFill::fillSpan(uint16_t* from, uint16_t* to,
                           std::vector<ConvexShape*>& shapeStack,
                           int x2, int y2) {

    float x = x2 - baseX;
    float y = y2 - baseY;

    float zReciprocalValue = zReciprocal.valueAt(x, y);

    float uPerZValue = uPerZ.valueAt(x, y);
    float vPerZValue = vPerZ.valueAt(x, y);

    float z = 1 / zReciprocalValue;
    float u = uPerZValue * z;
    float v = vPerZValue * z;

    if (u < 0 || u >= TEXTURE_SIZE || v < 0 || v >= TEXTURE_SIZE) {
        throw "texture coordinates out of bounds (1)";
    }

    int advance = to - from;
    /*
    printf("advance: %d\n", advance);
    printf("1/z: %f, u/z: %f, v/z: %f\n", zReciprocalValue, uPerZValue, vPerZValue);
    printf("new 1/z: %f, u/z: %f, v/z: %f\n", zReciprocalValue + advance * zReciprocal.byX, uPerZValue + advance * uPerZ.byX, vPerZValue + advance * vPerZ.byX);
    */
    float zEnd = 1 / (zReciprocalValue + advance * zReciprocal.byX);
    float uEnd = (uPerZValue + advance * uPerZ.byX) * zEnd;
    float vEnd = (vPerZValue + advance * vPerZ.byX) * zEnd;
    /*
    printf("at %d, %d draw %d ", x2, y2, to - from);
    printf("u: %f, v: %f, z: %f, uEnd: %f, vEnd: %f, zEnd: %f\n", u, v, z, uEnd, vEnd, zEnd);
    */
    if (uEnd < 0 || uEnd >= TEXTURE_SIZE || vEnd < 0 || vEnd >= TEXTURE_SIZE) {
        /*
        while (from != to)
            *(from++) = 0xF000;
        return;
        */
        throw "texture coordinates out of bounds (2)";
    }

    //printf("(%f, %f): z: %f, u: %f, v: %f\n", x, y, z, u, v);
    //printf("v: %d,y:%d\n", v.floor(), y);

    interp0->base[2] = (uintptr_t)texture;

    while (from != to) {
        zReciprocalValue += zReciprocal.by8X;
        uPerZValue += uPerZ.by8X;
        vPerZValue += vPerZ.by8X;

        float zNext = 1 / zReciprocalValue;
        float uNext = uPerZValue * zNext;
        float vNext = vPerZValue * zNext;

        // ld. "Scanline subdivision" itt: 
        // https://www.lysator.liu.se/~mikaelk/doc/perspectivetexture/

        int uIncrement = (int)((uNext - u) * ((1 << UV_FRACTIONAL_BITS) / SCANLINE_SUBDIVISION));
        int vIncrement = (int)((vNext - v) * ((1 << UV_FRACTIONAL_BITS) / SCANLINE_SUBDIVISION));

        int textureXFixedPoint = (int)(u * (1 << UV_FRACTIONAL_BITS));
        int textureYFixedPoint = (int)(v * (1 << UV_FRACTIONAL_BITS));

        interp0->accum[0] = textureXFixedPoint;
        interp0->base[0] = uIncrement;
        interp0->accum[1] = textureYFixedPoint;
        interp0->base[1] = vIncrement;

        //printf("Initial: %d, %d\n", u, v);
        //*(from++) = texture[(textureXFixedPoint >> UV_FRACTIONAL_BITS) +
        //        ((textureYFixedPoint >> UV_FRACTIONAL_BITS) << TEXTURE_STRIDE_BITS)]

        *(from++) = *(uint16_t*)interp0->pop[2];
        if (from == to)
            return;
        *(from++) = *(uint16_t*)interp0->pop[2];
        if (from == to)
            return;
        *(from++) = *(uint16_t*)interp0->pop[2];
        if (from == to)
            return;
        *(from++) = *(uint16_t*)interp0->pop[2];
        if (from == to)
            return;
        *(from++) = *(uint16_t*)interp0->pop[2];
        if (from == to)
            return;
        *(from++) = *(uint16_t*)interp0->pop[2];
        if (from == to)
            return;
        *(from++) = *(uint16_t*)interp0->pop[2];
        if (from == to)
            return;
        *(from++) = *(uint16_t*)interp0->pop[2];

        z = zNext;
        u = uNext;
        v = vNext;
    }
}

bool TextureFill::isOpaque() {
    return true;
}

std::string TextureFill::toString() {
    std::stringstream s;
    s << "0x" << std::hex << texture;
    return s.str();
}

