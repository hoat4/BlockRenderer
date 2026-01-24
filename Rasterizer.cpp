#include "Rasterizer.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <math.h>
#include <algorithm>
#include "textures.hpp" // méret konstansok miatt
#include "arm_acle.h" // usat miatt
#include <iomanip> // std::stringstream
#include <pico/stdlib.h> // get_absolute_time

const bool TRACE_CHANGE_ACTIVE_LIST = false;
const bool TRACE_FILLS = false;
const bool SHOW_SMALL_SPANS = false;

// mintha kicsit gyorsabb lenne így megfordítva, de nem biztos
const int INTERPOLATOR_LANE_U = 0;
const int INTERPOLATOR_LANE_V = 1;

const int TEXTURE_PIXEL_BITS = 1;
const int MAX_TEXTURE_ROW_BITS = 5;


Rasterizer::Rasterizer(int width, int height) : width(width), height(height) {
    shapeListsByLine.resize(height);

    // Get a free channel, panic() if there are none
    dmaChannel = dma_claim_unused_channel(true);

    dmaChannelConfig = dma_channel_get_default_config(dmaChannel);
    channel_config_set_transfer_data_size(&dmaChannelConfig, DMA_SIZE_32);
    channel_config_set_read_increment(&dmaChannelConfig, false);
    channel_config_set_write_increment(&dmaChannelConfig, true);


    interp_config cfg = interp_default_config();
    // set add_raw flag to use raw (un-shifted and un-masked) lane accumulator value when adding
    // it to the lane base to make the lane result
    interp_config_set_add_raw(&cfg, true);
    interp_config_set_shift(&cfg, PERSPECTIVE_UV_FRACTIONAL_BITS + PERSPECTIVE_SCANLINE_SUBDIVISION_BITS - TEXTURE_PIXEL_BITS);
    interp_config_set_mask(&cfg, TEXTURE_PIXEL_BITS, TEXTURE_SIZE_BITS - 1 + TEXTURE_PIXEL_BITS);
    interpolatorLaneUConfig = cfg;

    interp_config_set_shift(&cfg, PERSPECTIVE_UV_FRACTIONAL_BITS + PERSPECTIVE_SCANLINE_SUBDIVISION_BITS - TEXTURE_STRIDE_BITS - TEXTURE_PIXEL_BITS);
    interp_config_set_mask(&cfg, TEXTURE_STRIDE_BITS + TEXTURE_PIXEL_BITS, TEXTURE_STRIDE_BITS + MAX_TEXTURE_ROW_BITS - 1 + TEXTURE_PIXEL_BITS);
    interpolatorLaneVConfig = cfg;
}

void Rasterizer::beginFrame() {
    std::fill(shapeListsByLine.begin(), shapeListsByLine.end(), (ConvexShape*)NULL);

    for (int shapeIndex = 0; shapeIndex < shapes.size(); shapeIndex++) {
        ConvexShape* s = shapes[shapeIndex];
        if (s->top == s->bottom)
            // processCurrentLine előbb nézi a kiszedhetőeket mint a berakhatóakat,
            // ezért bennmaradnának a képernyő aljáig a 0 magasságú shapeek
            continue;
        s->nextInTopRow = shapeListsByLine[s->top];
        s->prevInActiveList = NULL;
        s->nextInActiveList = NULL;
        shapeListsByLine[s->top] = s;
    }

    activeShapes = NULL;

    y = 0;

    renderOrderInvalidated = true;
}

void Rasterizer::skipLines(int lines) {
    for (int i = 0; i < lines; i++) {
        processCurrentLine();
        y++;
    }
}

void Rasterizer::prepareLine() {
    if (TRACE_FILLS || TRACE_CHANGE_ACTIVE_LIST)
        printf("line %d\n", y);
    processCurrentLine();
}

void Rasterizer::renderLine(uint16_t* buffer) {
    //if (y == 300)
    //    TRACE_FILLS = true;

    //absolute_time_t t1 = get_absolute_time();
    renderSpans(buffer);
    //absolute_time_t t2 = get_absolute_time();
    //printf("%lld, %lld\n", t2 - t1, t3 - t2);

    //if (y == 300)
    //    TRACE_FILLS = false;

    y++;
}

void Rasterizer::waitForLineFinish() {
    dma_channel_wait_for_finish_blocking(dmaChannel);
}

void Rasterizer::addToActiveList(ConvexShape* shapeToAdd) {
    if (TRACE_CHANGE_ACTIVE_LIST || TRACE_FILLS) {
        printf("Add %s at %f-%f (z-index: %f)\n", shapeToAdd->toString().c_str(),
               shapeToAdd->leftX, shapeToAdd->rightX, shapeToAdd->zIndex);
        printf("Before addToActiveList: ");
        printActiveShapes();
    }

    ConvexShape* addAfter = NULL;
    for (ConvexShape* shape = activeShapes; shape && shouldBeAfterInActiveShapeList(shapeToAdd, shape); shape = shape->nextInActiveList) {
        addAfter = shape;
    }
    if (addAfter == shapeToAdd)
        throw "shape already in active list";
    if (!addAfter) {
        if (TRACE_CHANGE_ACTIVE_LIST || TRACE_FILLS)
            printf("Add at active list begin\n");
        shapeToAdd->prevInActiveList = NULL;
        shapeToAdd->nextInActiveList = activeShapes;
        if (activeShapes)
            activeShapes->prevInActiveList = shapeToAdd;
        activeShapes = shapeToAdd;
    } else {
        if (TRACE_CHANGE_ACTIVE_LIST || TRACE_FILLS)
            printf("Add after %s at %f-%f\n", addAfter->fill->toString().c_str(), addAfter->leftX, addAfter->rightX);
        shapeToAdd->prevInActiveList = addAfter;
        shapeToAdd->nextInActiveList = addAfter->nextInActiveList;
        addAfter->nextInActiveList = shapeToAdd;
        if (shapeToAdd->nextInActiveList)
            shapeToAdd->nextInActiveList->prevInActiveList = shapeToAdd;
    }

    if (TRACE_CHANGE_ACTIVE_LIST || TRACE_FILLS) {
        printf("After addToActiveList: ");
        printActiveShapes();
        printf("\n");
    }

    renderOrderInvalidated = true;
}

void Rasterizer::removeFromActiveList(ConvexShape* shape) {
    if (TRACE_CHANGE_ACTIVE_LIST || TRACE_FILLS)
        printf("Remove %s\n", shape->toString().c_str());
    if (shape->prevInActiveList)
        shape->prevInActiveList->nextInActiveList = shape->nextInActiveList;
    else
        activeShapes = shape->nextInActiveList;
    if (shape->nextInActiveList)
        shape->nextInActiveList->prevInActiveList = shape->prevInActiveList;
    shape->prevInActiveList = NULL;
    shape->nextInActiveList = NULL;

    renderOrderInvalidated = true;
}

/**
 * ha megfordul a és b, akkor az ellentétjét adja vissza (kivéve ha nem lehet
 * eldönteni hogy melyik van balra és jobbra)
 */
inline bool Rasterizer::shouldBeAfterInActiveShapeList(ConvexShape* a, ConvexShape* b) {
    int aLeft = a->leftXInt, bLeft = b->leftXInt;
    float az = a->zIndex, bz = b->zIndex;

    if (aLeft == bLeft) {
        return az < bz;
    }


    if (aLeft < bLeft) {
        // A bal oldala balra van B-től.
        // Csak akkor kell megcserélni a két shapeet, ha A B mögött van, 
        // de kilóg tőle balra és jobbra is, de a kilógás bal részéről tudjuk, hogy nem fog látszódni.
        return az < bz && a->groupX > b->groupX;
    } else {
        // B bal oldala balra van A bal oldalától.
        // Akkor kell így hagyni a sorrenet, ha B A mögött van, de kilóg
        // tőle balra és jobbra is, viszont a kilógás bal részéről tudjuk, hogy nem fog látszódni.
        return !(bz < az && b->groupX > a->groupX);
    }
}

inline void Rasterizer::shiftLeftInActiveList(ConvexShape* shape) {
    bool wasShifted = false;
    while (shape->prevInActiveList && !shouldBeAfterInActiveShapeList(shape, shape->prevInActiveList)) {
        wasShifted = true;

        ConvexShape* prev = shape->prevInActiveList;
        if (shape->nextInActiveList)
            shape->nextInActiveList->prevInActiveList = prev;
        if (prev->prevInActiveList)
            prev->prevInActiveList->nextInActiveList = shape;
        else
            activeShapes = shape;
        shape->prevInActiveList = prev->prevInActiveList;
        prev->nextInActiveList = shape->nextInActiveList;
        shape->nextInActiveList = prev;
        prev->prevInActiveList = shape;
    }
    if (wasShifted)
        renderOrderInvalidated = true;
}

void __not_in_flash_func(Rasterizer::processCurrentLine)() {
    for (ConvexShape* shape = activeShapes; shape; ) {
        ConvexShape* next = shape->nextInActiveList; // removeFromActiveList megváltoztatja shape->nextInActiveList-et
        if (shape->bottom == y) {
            removeFromActiveList(shape);
        } else {
            if (!processBreaksOnCurrentLine(shape)) {
                shape->leftX += shape->leftSlope;
                shape->leftXInt = (int)shape->leftX;
                shape->rightX += shape->rightSlope;
                shape->rightXInt = (int)shape->rightX;
            }
            shiftLeftInActiveList(shape);
        }
        shape = next;
    }

    for (ConvexShape* shape = shapeListsByLine[y];
         shape != NULL;
         shape = shape->nextInTopRow) {

        shape->leftX = shape->topLeftX;
        shape->leftSlope = shape->topLeftSlope;
        shape->leftXInt = (int)shape->leftX;
        shape->rightX = shape->topRightX;
        shape->rightSlope = shape->topRightSlope;
        shape->rightXInt = (int)shape->rightX;

        processBreaksOnCurrentLine(shape);
        addToActiveList(shape);
    }

    checkActiveShapes();
}

void Rasterizer::checkActiveShapes() {
    if (true)return;
    // TODO ezt aktualizálni kéne ConvexShapeGroup átalakítás óta
    ConvexShape* prevS = NULL;
    for (ConvexShape* shape = activeShapes; shape; shape = shape->nextInActiveList) {
        if (prevS && (shape->leftXInt < prevS->leftXInt || shape->prevInActiveList != prevS ||
                      shape->leftXInt == prevS->leftXInt && shape->zIndex > prevS->zIndex)) {
            printActiveShapes();
            printf("leftx: %d, %d\n", shape->leftXInt, prevS->leftXInt);
            throw "wrong active shapes";
        }
        prevS = shape;
    }
}

bool Rasterizer::processBreaksOnCurrentLine(ConvexShape* shape) {
    IntermediateVertex* v;
    if (shape->intermediateVertices[0].y == y) {
        v = &shape->intermediateVertices[0];
    } else if (shape->intermediateVertices[1].y == y) {
        v = &shape->intermediateVertices[1];
    } else
        return false;

    if (v->isLeft) {
        shape->leftX = v->x;
        shape->leftSlope = v->slope;
        shape->leftXInt = (int)shape->leftX;
    } else {
        shape->rightX = v->x;
        shape->rightSlope = v->slope;
        shape->rightXInt = (int)shape->rightX;
    }

    return true;
}

void Rasterizer::printActiveShapes() {
    printf("Active shapes: ");
    for (ConvexShape* shape = activeShapes; shape; shape = shape->nextInActiveList) {
        printf("%s at %d-%d (group: z=%f, x=%f), ",
               shape->fill->toString().c_str(),
               shape->leftXInt, shape->rightXInt,
               shape->zIndex, shape->groupX);
    }
    printf("\n");
}

void Rasterizer::renderSpans(uint16_t* buffer) {
    if (TRACE_FILLS) {
        printActiveShapes();
    }

    interp_set_config(interp0, INTERPOLATOR_LANE_U, &interpolatorLaneUConfig);
    interp_set_config(interp0, INTERPOLATOR_LANE_V, &interpolatorLaneVConfig);

    // TODO right<=0-eket kiszűrni
    ConvexShape* shape = activeShapes;

    int prevX = 0;
    for (; shape; shape = shape->nextInActiveList) {
        int shapeLeft = shape->leftXInt;
        int shapeRight = shape->rightXInt;

        int rightClip = shapeRight;
        ConvexShape* rightClipShape = NULL;
        if (renderOrderInvalidated) {
            for (ConvexShape* s = shape->nextInActiveList; s; s = s->nextInActiveList) {
                int sLeft = s->leftXInt;
                if (sLeft == shapeLeft)
                    continue;
                if (sLeft >= shapeRight)
                    break;
                if (s->zIndex > shape->zIndex) {
                    rightClipShape = s;
                    rightClip = sLeft;
                    break;
                }
            }
            shape->cachedRightClipPtr = rightClipShape ? &rightClipShape->leftXInt : &shape->rightXInt;
            shape->cachedRightClipShape = rightClipShape;
        } else {
            rightClip = *shape->cachedRightClipPtr;
            rightClipShape = shape->cachedRightClipShape;
        }

        int from = std::max(shapeLeft, prevX);
        int to = std::min(rightClip, width);
        if (from < to) {
            if (prevX < from) {
                if (TRACE_FILLS)
                    printf("Fill background %d-%d\n", prevX, from);
                fillWithBackground(&buffer[prevX], &buffer[from]);
            }
            if (TRACE_FILLS)
                printf("Fill %d-%d with %s\n", from, to, shape->fill->toString().c_str());

            Fill* fill = shape->fill;
            int x2 = from - fill->baseX, y2 = y - fill->baseY;
            if (fill->isInterpolated)
                fillWithInterpolatedTexture(fill, &buffer[from], &buffer[to], x2, y2);
            else
                fillWithPerspectiveTexture(fill, &buffer[from], &buffer[to], x2, y2);
            prevX = to;
        }
    }
    if (prevX < width) {
        if (TRACE_FILLS)
            printf("Fill background %d-%d\n", prevX, width);
        fillWithBackground(&buffer[prevX], &buffer[width]);
    }
    renderOrderInvalidated = false;
}

void Rasterizer::fillWithBackground(uint16_t* from, uint16_t* to) {
    uint16_t rgb565 = background;

    if (to - from <= 15) {
        while (from != to)
            (*from++) = rgb565;
        return;
    }

    if ((intptr_t)from & 2)
        *(from++) = rgb565;
    if (((intptr_t)to) & ~2)
        *(to - 1) = rgb565;

    uint32_t colorDoubled = rgb565 | rgb565 << 16;
    uint32_t* start2 = (uint32_t*)from;
    uint32_t* end2 = (uint32_t*)to;

    dma_channel_wait_for_finish_blocking(dmaChannel);
    dmaBuffer = colorDoubled;
    dma_channel_configure(
        dmaChannel,
        &dmaChannelConfig,
        from,   // The initial write address
        &dmaBuffer,     // The initial read address
        end2 - ((uint32_t*)from),      // Number of transfers
        true            // Start immediately.
    );
}

inline void Rasterizer::fillWithPerspectiveTexture(Fill* fill,
                                                   uint16_t* from, uint16_t* to,
                                                   int x, int y) {
    if (SHOW_SMALL_SPANS && to - from < 8) {
        while (from < to)
            (*from++) = 0xF800;
        return;
    }

    PerspectiveTextureMapping& m = fill->textureMapping.perspective;

    float zReciprocalValue = m.zReciprocal.valueAt(x, y);

    float uPerZValue = m.uPerZ.valueAt(x, y);
    float vPerZValue = m.vPerZ.valueAt(x, y);

    float z = 1 / zReciprocalValue;
    float u = uPerZValue * z, v = vPerZValue * z;

    // usat-ot használtunk, de az lassabb volt mint ha csak a textúra paddingra hagyatkozunk: 
    // int u = __usat((int)(uPerZValue * z), (PERSPECTIVE_UV_FRACTIONAL_BITS + TEXTURE_SIZE_BITS));
    // reméljük hogy nem lesz nagy kerekítési vagy numerikus hiba ami miatt nagyon kicsúszna a textúrából.
    // ha csak kicsit csúszik ki, az végülis nem baj, mert csak random színt fog beolvasni.

    /*
    if (u <= -(1<<PERSPECTIVE_UV_FRACTIONAL_BITS) || u >= (TEXTURE_SIZE+1) << PERSPECTIVE_UV_FRACTIONAL_BITS ||
        v <= -(1<<PERSPECTIVE_UV_FRACTIONAL_BITS) || v >= (TEXTURE_SIZE+1) << PERSPECTIVE_UV_FRACTIONAL_BITS) {
        printf("u: %d, v: %d, original pos: (%d, %d), base: (%f, %f)\n", u, v, x2, y2, m.baseX, m.baseY);
        printf("1/z base: %f, value: %f\n", m.zReciprocal.base, zReciprocalValue);
        printf("v/z value: %f\n", m.vPerZ, vPerZValue);
        throw "texture coordinates out of bounds (1)";
    }
    */

    zReciprocalValue += m.zReciprocal.by8X;
    float zNext = 1.0f / zReciprocalValue;

    interp0->base[2] = (uintptr_t)fill->texture; // ez itthagyva gyorsabb, mintha kiemelnénk renderSpansba
    interp0->accum[INTERPOLATOR_LANE_U] = u * PERSPECTIVE_SCANLINE_SUBDIVISION;
    interp0->accum[INTERPOLATOR_LANE_V] = v * PERSPECTIVE_SCANLINE_SUBDIVISION;


    do {
        // ld. "Scanline subdivision" itt: 
        // https://www.lysator.liu.se/~mikaelk/doc/perspectivetexture/

        uPerZValue += m.uPerZ.by8X;
        vPerZValue += m.vPerZ.by8X;

        float uNext = uPerZValue * zNext;
        interp0->base[INTERPOLATOR_LANE_U] = (int)(uNext - u); // uIncrement
        u = uNext; // azért nem a lenti ifbe, hogy felhasználhassa az uNext regiszterét később másra

        float vNext = vPerZValue * zNext;

        interp0->base[INTERPOLATOR_LANE_V] = (int)(vNext - v); // vIncrement


        //*(from++) = texture[(textureXFixedPoint >> UV_FRACTIONAL_BITS) +
        //        ((textureYFixedPoint >> UV_FRACTIONAL_BITS) << TEXTURE_SIZE_BITS)]

        if (to - from > 8) {
            v = vNext;
            zReciprocalValue += m.zReciprocal.by8X;
            zNext = 1.0f / zReciprocalValue; // a lenti memóriaírásokkal párhuzamosan menjen ez az osztás
        }

        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
    } while (from < to);
}


inline void Rasterizer::fillWithInterpolatedTexture(Fill* fill,
                                                    uint16_t* from, uint16_t* to,
                                                    int x, int y) {
    if (SHOW_SMALL_SPANS && to - from < 8) {
        while (from < to)
            (*from++) = 0xFFF0;
        return;
    }

    InterpolatedTextureMapping& m = fill->textureMapping.interpolated;

    int u1 = m.uByX * x + m.uByY * y;
    int v1 = m.vByX * x + m.vByY * y;

    /*
    if (u1 < 0 || u1 >= TEXTURE_SIZE << UV_FRACTIONAL_BITS ||
        v1 < 0 || v1 >= TEXTURE_SIZE << UV_FRACTIONAL_BITS) {
        printf("u: %d, v: %d, original pos: (%d, %d), base: (%d, %d)\n", u1, v1, x2, y2, baseX, baseY);
        throw "texture coordinates out of bounds (3)";
    }
        */

    interp0->accum[INTERPOLATOR_LANE_U] = u1;
    interp0->accum[INTERPOLATOR_LANE_V] = v1;
    interp0->base[INTERPOLATOR_LANE_U] = m.uByX;
    interp0->base[INTERPOLATOR_LANE_V] = m.vByX;
    interp0->base[2] = (uintptr_t)fill->texture;

    do {
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
    } while (from < to);
}

float ConvexShape::slope(Point2D top, Point2D bottom) {
    return (bottom.x - top.x) / (std::floor(bottom.y) - std::floor(top.y));
}

IntermediateVertex ConvexShape::makeIntermediateVertex(Point2D top, Point2D bottom, bool isLeft) {
    return {
        .y = (int)std::floor(top.y),
        .isLeft = isLeft,
        .x = top.x,
        .slope = slope(top, bottom)
    };
}

ConvexShape* ConvexShape::create(
    std::span<Point2D> leftPoints, std::span<Point2D> rightPoints,
    float z,
    Fill* fill) {

    ConvexShape* shape = create(leftPoints.size(), rightPoints.size(), fill);
    shape->zIndex = z;
    shape->modifyPoints(leftPoints, rightPoints);
    return shape;
}

ConvexShape* ConvexShape::create(
    int maxLeftVertexCount, int maxRightVertexCount,
    Fill* fill) {

    ConvexShape* shape = new ConvexShape();

    // unique_ptr.reset megpróbálná törölni a memóriaszemetet
    shape->fill = fill;

    return shape;
}

void ConvexShape::modifyPoints(std::span<Point2D> leftPoints, std::span<Point2D> rightPoints) {
    if (leftPoints.size() < 2 || rightPoints.size() < 2)
        throw "too few points";
    if (leftPoints[0].y != rightPoints[0].y)
        throw "top point Y coordinate differs";
    if (leftPoints.back().y != rightPoints.back().y)
        throw "bottom point Y coordinate differs";
    int intermediatePointCount = leftPoints.size() - 2 + rightPoints.size() - 2;

    if (2 < intermediatePointCount)
        throw "intermediate vertex count too large";


    this->top = (int)std::floor(leftPoints.front().y);
    this->bottom = (int)std::floor(leftPoints.back().y);

    if (top == bottom)
        // felesleges kiszámolni a pontokat, mert beginFrame úgyis ignorálni fogja ezt a shapeet
        return;

    this->topLeftX = leftPoints[0].x;
    this->topRightX = rightPoints[0].x;
    this->topLeftSlope = (leftPoints[1].x - leftPoints[0].x) /
        (std::floor(leftPoints[1].y) - std::floor(leftPoints[0].y));
    this->topRightSlope = (rightPoints[1].x - rightPoints[0].x) /
        (std::floor(rightPoints[1].y) - std::floor(rightPoints[0].y));

    int v = 0;
    for (int i = 1, j = 1; ; ) {
        if (i < leftPoints.size() - 1) {
            if (j < rightPoints.size() - 1 && rightPoints[j].y < leftPoints[i].y) {
                this->intermediateVertices[v++] =
                    makeIntermediateVertex(rightPoints[j], rightPoints[j + 1], false);
                j++;
            } else {
                this->intermediateVertices[v++] =
                    makeIntermediateVertex(leftPoints[i], leftPoints[i + 1], true);
                i++;
            }
        } else if (j < rightPoints.size() - 1) {
            this->intermediateVertices[v++] =
                makeIntermediateVertex(rightPoints[j], rightPoints[j + 1], false);
            j++;
        } else {
            break;
        }
    }

    for (; v < 2; v++) {
        this->intermediateVertices[v].y = std::numeric_limits<int>::max();
    }
}

std::string ConvexShape::toString() {
    std::stringstream s;
    s << debugName;
    s << " ";
    s << fill->toString();
    return s.str();
}

ConvexShape::~ConvexShape() {
    delete fill;
}
