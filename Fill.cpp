#include "Fill.hpp"

#include <iomanip>

bool ColorFill::isOpaque() {
    return (color & 0xFF000000) == 0xFF000000;
}

void ColorFill::fillSpan(uint16_t* from, uint16_t* to,
                         std::vector<ConvexShape*> shapeStack,
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
                           std::vector<ConvexShape*> shapeStack,
                           int x, int y) {
    FixedPointNumber u = uAtLeftTop + uByX * x + uByY * y;
    FixedPointNumber v = vAtLeftTop + vByX * x + vByY * y;
    //printf("v: %d,y:%d\n", v.floor(), y);

    while (from != to) {
        int textureX = u.floor();
        int textureY = v.floor();

        u += uByX;
        v += vByX;

        if (textureX < 0)
            textureX = 0;
        if (textureY < 0)
            textureY = 0;
        if (textureX > 15)
            textureX = 15;
        if (textureY > 15)
            textureY = 15;

        *(from++) = texture[textureX + textureY * 16];
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

