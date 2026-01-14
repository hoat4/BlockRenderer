#include "Fill.hpp"

#include <iomanip>

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

    //printf("(%f, %f): z: %f, u: %f, v: %f\n", x, y, z, u, v);
    //printf("v: %d,y:%d\n", v.floor(), y);

    while (from != to) {
        zReciprocalValue += zReciprocal.by8X;
        uPerZValue += uPerZ.by8X;
        vPerZValue += vPerZ.by8X;

        float zNext = 1 / zReciprocalValue;
        float uNext = uPerZValue * z;
        float vNext = vPerZValue * z;

        // ld. "Scanline subdivision" itt: 
        // https://www.lysator.liu.se/~mikaelk/doc/perspectivetexture/
        
        int uIncrement = (int)((uNext - u) * (65536 / 8));
        int vIncrement = (int)((vNext - v) * (65536 / 8));

        int textureXFixedPoint = (int)(u * 65536);
        int textureYFixedPoint = (int)(v * 65536);

        for (int i = 0; i < 8; i++) {
            if (textureXFixedPoint < 0)
                textureXFixedPoint = 0;
            if (textureYFixedPoint < 0)
                textureYFixedPoint = 0;
            if (textureXFixedPoint > 15 * 65536)
                textureXFixedPoint = 15 * 65536;
            if (textureYFixedPoint > 15 * 65536)
                textureYFixedPoint = 15 * 65536;
            *(from++) = texture[(textureXFixedPoint >> 16) + (textureYFixedPoint >> 16) * 16];
            //*(from++) = texture[20];

            if (i != 7) {
                textureXFixedPoint += uIncrement;
                textureYFixedPoint += vIncrement;
                if (from == to)
                    return;
            }
        }

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

