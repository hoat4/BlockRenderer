#include "Rasterizer.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <math.h>
#include <algorithm>

#include <pico/stdlib.h> // get_absolute_time

const bool TRACE_CHANGE_ACTIVE_LIST = false;
const bool TRACE_FILLS = false;

Rasterizer::Rasterizer(int width, int height) : width(width), height(height) {
    shapeListsByLine.resize(height);

    ColorFill::initDMAChannel(); // TODO ezt csak egyszer kéne
}

int Rasterizer::screenWidth() {
    return width;
}

int Rasterizer::screenHeight() {
    return height;
}

void Rasterizer::beginFrame() {
    std::fill(shapeListsByLine.begin(), shapeListsByLine.end(), (ConvexShape*)NULL);

    for (int shapeIndex = 0; shapeIndex < shapes.size(); shapeIndex++) {
        ConvexShape* s = shapes[shapeIndex];
        if (s->top == s->bottom)
            // processCurrentLine előbb nézi a kiszedhetőeket mint a berakhatóakat,
            // ezért bennmaradnának a képernyő aljáig a 0 magasságú shapeek
            continue;
        s->nextIntermediateVertexIndex = 0;
        s->nextInTopRow = shapeListsByLine[s->top];
        s->prevInActiveList = NULL;
        s->nextInActiveList = NULL;
        shapeListsByLine[s->top] = s;
        s->group->shapesInActiveList.clear();
    }

    activeShapes = NULL;

    y = 0;
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

    absolute_time_t t1 = get_absolute_time();
    renderSpans(buffer);
    absolute_time_t t2 = get_absolute_time();
    //printf("%lld, %lld\n", t2 - t1, t3 - t2);

    //if (y == 300)
    //    TRACE_FILLS = false;

    y++;
}

void Rasterizer::waitForLineFinish() {
    ColorFill::waitForDMAChannelToFinish();
}

void Rasterizer::addToActiveList(ConvexShape* shapeToAdd) {
    if (TRACE_CHANGE_ACTIVE_LIST || TRACE_FILLS) {
        printf("Before addToActiveList: ");
        printActiveShapes();
        printf("Add %s at %f-%f (z-index: %f)\n", shapeToAdd->fill->toString().c_str(),
               shapeToAdd->leftX, shapeToAdd->rightX, shapeToAdd->group->zIndex);
    }
    ConvexShape* addAfter = NULL;
    for (ConvexShape* shape = activeShapes; shape && shouldBeAfterInActiveShapeList(shapeToAdd, shape); shape = shape->nextInActiveList) {
        addAfter = shape;
    }
    if (addAfter == shapeToAdd)
        throw "shape already in active list";
    if (!addAfter) {
        if (TRACE_CHANGE_ACTIVE_LIST || TRACE_FILLS)
            printf("Add %s at active list begin\n", shapeToAdd->fill->toString().c_str());
        shapeToAdd->prevInActiveList = NULL;
        shapeToAdd->nextInActiveList = activeShapes;
        if (activeShapes)
            activeShapes->prevInActiveList = shapeToAdd;
        activeShapes = shapeToAdd;
    } else {
        if (TRACE_CHANGE_ACTIVE_LIST || TRACE_FILLS)
            printf("Add %s after %s\n", shapeToAdd->fill->toString().c_str(), addAfter->fill->toString().c_str());
        shapeToAdd->prevInActiveList = addAfter;
        shapeToAdd->nextInActiveList = addAfter->nextInActiveList;
        addAfter->nextInActiveList = shapeToAdd;
        if (shapeToAdd->nextInActiveList)
            shapeToAdd->nextInActiveList->prevInActiveList = shapeToAdd;
    }

    shapeToAdd->group->shapesInActiveList.push_back(shapeToAdd);

    if (TRACE_CHANGE_ACTIVE_LIST || TRACE_FILLS) {
        printf("After addToActiveList: ");
        printActiveShapes();
        printf("\n");
    }
}

void Rasterizer::removeFromActiveList(ConvexShape* shape) {
    if (TRACE_CHANGE_ACTIVE_LIST || TRACE_FILLS)
        printf("Remove %s\n", shape->fill->toString().c_str());
    if (shape->prevInActiveList)
        shape->prevInActiveList->nextInActiveList = shape->nextInActiveList;
    else
        activeShapes = shape->nextInActiveList;
    if (shape->nextInActiveList)
        shape->nextInActiveList->prevInActiveList = shape->prevInActiveList;
    shape->prevInActiveList = NULL;
    shape->nextInActiveList = NULL;

    std::vector<ConvexShape*>::iterator pos = std::find(
        shape->group->shapesInActiveList.begin(),
        shape->group->shapesInActiveList.end(),
        shape);
    if (pos == shape->group->shapesInActiveList.end())
        throw "not in shapesInActiveList of group";
    shape->group->shapesInActiveList.erase(pos);
}

bool Rasterizer::shouldBeAfterInActiveShapeList(ConvexShape* shape, ConvexShape* other) {
    int ax = shape->leftXInt, bx = other->leftXInt;
    if (shape->group != other->group) {
        if (shape->group->zIndex > other->group->zIndex)
            for (ConvexShape* s : shape->group->shapesInActiveList)
                ax = std::min(ax, s->leftXInt);
        else
            for (ConvexShape* s : other->group->shapesInActiveList)
                bx = std::min(bx, s->leftXInt);
    }
    return ax > bx || ax == bx && shape->group->zIndex < other->group->zIndex;
}

void Rasterizer::shiftLeftInActiveList(ConvexShape* shape) {
    while (shape->prevInActiveList && !shouldBeAfterInActiveShapeList(shape, shape->prevInActiveList)) {
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
}

void Rasterizer::processCurrentLine() {
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
                      shape->leftXInt == prevS->leftXInt && shape->group->zIndex > prevS->group->zIndex)) {
            printActiveShapes();
            printf("leftx: %d, %d\n", shape->leftXInt, prevS->leftXInt);
            throw "wrong active shapes";
        }
        prevS = shape;
    }
}

bool Rasterizer::processBreaksOnCurrentLine(ConvexShape* shape) {
    bool hadBreaks = false;
    while (shape->nextIntermediateVertexIndex < shape->intermediateVertexCount &&
           shape->intermediateVertices[shape->nextIntermediateVertexIndex].y == y) {

        IntermediateVertex v = shape->intermediateVertices[shape->nextIntermediateVertexIndex];
        if (v.isLeft) {
            shape->leftX = v.x;
            shape->leftSlope = v.slope;
            shape->leftXInt = (int)shape->leftX;
        } else {
            shape->rightX = v.x;
            shape->rightSlope = v.slope;
            shape->rightXInt = (int)shape->rightX;
        }

        shape->nextIntermediateVertexIndex++;
        hadBreaks = true;
    }
    return hadBreaks;
}

void Rasterizer::printActiveShapes() {
    printf("Active shapes: ");
    for (ConvexShape* shape = activeShapes; shape; shape = shape->nextInActiveList) {
        printf("%s at %d-%d (z-index %f), ", shape->fill->toString().c_str(), shape->leftXInt, shape->rightXInt, shape->group->zIndex);
    }
    printf("\n");
}

void Rasterizer::renderSpans(uint16_t* buffer) {
    if (TRACE_FILLS) {
        printActiveShapes();
    }

    PerspectiveTextureFill::initInterpolator();

    // TODO right<=0-eket kiszűrni
    ConvexShape* shape = activeShapes;

    int prevX = 0;
    for (; shape; shape = shape->nextInActiveList) {
        int shapeLeft = shape->leftXInt;
        int shapeRight = shape->rightXInt;

        int rightClip = shapeRight;
        ConvexShape* rightClipShape = NULL;
        for (ConvexShape* s = shape->nextInActiveList; s; s = s->nextInActiveList) {
            int sLeft = s->leftXInt;
            if (sLeft == shapeLeft)
                continue;
            if (sLeft >= shapeRight)
                break;
            if (s->group->zIndex > shape->group->zIndex &&
                (!rightClipShape || s->group->zIndex > rightClipShape->group->zIndex &&
                 sLeft <= rightClip)) {
                rightClipShape = s;
                rightClip = sLeft;
            }
        }

        int from = std::max(shapeLeft, prevX);
        int to = std::min(rightClip, width);
        if (from < to) {
            if (prevX < from) {
                if (TRACE_FILLS)
                    printf("Fill background %d-%d\n", prevX, from);
                background->fillSpan(&buffer[prevX], &buffer[from], prevX, y);
            }
            if (TRACE_FILLS)
                printf("Fill %d-%d with %s\n", from, to, shape->fill->toString().c_str());
            shape->fill->fillSpan(&buffer[from], &buffer[to], from, y);
            prevX = to;
        }
    }
    if (prevX < width) {
        if (TRACE_FILLS)
            printf("Fill background %d-%d\n", prevX, width);
        background->fillSpan(&buffer[prevX], &buffer[width], prevX, y);
    }
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

    ConvexShapeGroup* group = new ConvexShapeGroup();
    group->zIndex = z;
    ConvexShape* shape = create(group, leftPoints.size(), rightPoints.size(), fill);
    shape->modifyPoints(leftPoints, rightPoints);
    return shape;
}

ConvexShape* ConvexShape::create(
    ConvexShapeGroup* group,
    int maxLeftVertexCount, int maxRightVertexCount,
    Fill* fill) {

    if (maxLeftVertexCount < 2 || maxRightVertexCount < 2)
        throw "too few points";
    int maxIntermediatePointCount = maxLeftVertexCount - 2 + maxRightVertexCount - 2;

    ConvexShape* shape = (ConvexShape*)malloc(
        sizeof(ConvexShape) + sizeof(IntermediateVertex) * maxIntermediatePointCount);

    // unique_ptr.reset megpróbálná törölni a memóriaszemetet
    shape->fill = fill;
    shape->maxIntermediateVertexCount = maxIntermediatePointCount;
    shape->group = group;

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

    if (this->maxIntermediateVertexCount < intermediatePointCount)
        throw "intermediate vertex count too large";


    this->top = (int)std::floor(leftPoints.front().y);
    this->bottom = (int)std::ceil(leftPoints.back().y);

    if (top == bottom)
        // felesleges kiszámolni a pontokat, mert beginFrame úgyis ignorálni fogja ezt a shapeet
        return;

    this->intermediateVertexCount = intermediatePointCount;

    this->topLeftX = leftPoints[0].x;
    this->topRightX = rightPoints[0].x;
    this->topLeftSlope = (leftPoints[1].x - leftPoints[0].x) /
        (std::floor(leftPoints[1].y) - std::floor(leftPoints[0].y));
    this->topRightSlope = (rightPoints[1].x - rightPoints[0].x) /
        (std::floor(rightPoints[1].y) - std::floor(rightPoints[0].y));

    for (int i = 1, j = 1, v = 0; ; ) {
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
}

ConvexShape::~ConvexShape() {
    delete fill;
}
