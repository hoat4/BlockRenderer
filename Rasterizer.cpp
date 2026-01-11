#include "Rasterizer.hpp"
#include <stdio.h>
#include <stdlib.h>

const bool TRACE_CHANGE_ACTIVE_LIST = false;
const bool TRACE_FILLS = false;

Rasterizer::Rasterizer(int width, int height) : width(width), height(height) {
    shapeListsByLine.resize(height);
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
        s->nextIntermediateVertexIndex = 0;
        s->nextInTopRow = shapeListsByLine[s->top];
        s->prevInActiveList = NULL;
        s->nextInActiveList = NULL;
        shapeListsByLine[s->top] = s;
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

void Rasterizer::renderLine(uint16_t* buffer) {
    //if (y == 300)
    //    TRACE_FILLS = true;

    if (TRACE_FILLS || TRACE_CHANGE_ACTIVE_LIST)
        printf("line %d\n", y);
    processCurrentLine();
    renderSpans(buffer);

    //if (y == 300)
    //    TRACE_FILLS = false;

    y++;
}

void Rasterizer::addToActiveList(ConvexShape* shapeToAdd) {
    if (TRACE_CHANGE_ACTIVE_LIST || TRACE_FILLS) {
        printf("Before addToActiveList: ");
        printActiveShapes();
        printf("Add %04x at %d-%d\n", shapeToAdd->color, shapeToAdd->leftX, shapeToAdd->rightX);
    }
    ConvexShape* addAfter = NULL;
    for (ConvexShape* shape = activeShapes; shape && shape->leftX <= shapeToAdd->leftX; shape = shape->nextInActiveList) {
        addAfter = shape;
    }
    if (addAfter == shapeToAdd)
        throw "shape already in active list";
    if (!addAfter) {
        shapeToAdd->prevInActiveList = NULL;
        shapeToAdd->nextInActiveList = activeShapes;
        if (activeShapes)
            activeShapes->prevInActiveList = shapeToAdd;
        activeShapes = shapeToAdd;
    } else {
        shapeToAdd->prevInActiveList = addAfter;
        shapeToAdd->nextInActiveList = addAfter->nextInActiveList;
        addAfter->nextInActiveList = shapeToAdd;
        if (shapeToAdd->nextInActiveList)
            shapeToAdd->nextInActiveList->prevInActiveList = shapeToAdd;
    }
}

void Rasterizer::removeFromActiveList(ConvexShape* shape) {
    if (TRACE_CHANGE_ACTIVE_LIST || TRACE_FILLS)
        printf("Remove %04x\n", shape->color);
    if (shape->prevInActiveList)
        shape->prevInActiveList->nextInActiveList = shape->nextInActiveList;
    else
        activeShapes = shape->nextInActiveList;
    if (shape->nextInActiveList)
        shape->nextInActiveList->prevInActiveList = shape->prevInActiveList;
    shape->prevInActiveList = NULL;
    shape->nextInActiveList = NULL;
}

void Rasterizer::shiftLeftInActiveList(ConvexShape* shape) {
    while (shape->prevInActiveList && shape->leftX < shape->prevInActiveList->leftX) {
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
                shape->rightX += shape->rightSlope;
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
        shape->rightX = shape->topRightX;
        shape->rightSlope = shape->topRightSlope;

        addToActiveList(shape);

        processBreaksOnCurrentLine(shape);

        shiftLeftInActiveList(shape);
    }

    checkActiveShapes();
}

void Rasterizer::checkActiveShapes() {
    ConvexShape* prevS = NULL;
    for (ConvexShape* shape = activeShapes; shape; shape = shape->nextInActiveList) {
        if (prevS && (shape->leftX < prevS->leftX || shape->prevInActiveList != prevS)) {
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
        } else {
            shape->rightX = v.x;
            shape->rightSlope = v.slope;
        }

        shape->nextIntermediateVertexIndex++;
        hadBreaks = true;
    }
    return hadBreaks;
}

void Rasterizer::printActiveShapes() {
    printf("Active shapes: ");
    for (ConvexShape* shape = activeShapes; shape; shape = shape->nextInActiveList) {
        printf("%04x at %d-%d, ", shape->color, shape->leftX.floor(), shape->rightX.floor());
    }
    printf("\n");
}

void Rasterizer::renderSpans(uint16_t* buffer) {
    if (!shapeStack.empty())
        throw "activeShapes size not zero (at begin)";

    if (TRACE_FILLS) {
        printActiveShapes();
    }

    FixedPointNumber prevX = FixedPointNumber::zero();
    ConvexShape* nextLeft = activeShapes;
    while (true) {
        int shapeWithNearestRight = -1;
        FixedPointNumber nearestRight = FixedPointNumber::max();
        for (int i = 0; i < shapeStack.size(); i++) {
            if (shapeStack[i]->rightX < nearestRight) {
                shapeWithNearestRight = i;
                nearestRight = shapeStack[i]->rightX;
            }
        }
        if (shapeWithNearestRight != -1 && (nextLeft == NULL || nearestRight <= nextLeft->leftX)) {
            if (TRACE_FILLS)
                printf("RIGHT at %d-%d\n", prevX.floor(), nearestRight.floor());
            fillSpan(buffer, prevX, nearestRight);
            prevX = nearestRight;
            shapeStack.erase(shapeStack.begin() + shapeWithNearestRight);
        } else if (nextLeft) {
            if (TRACE_FILLS)
                printf("LEFT at %d-%d\n", prevX.floor(), nextLeft->leftX.floor());
            fillSpan(buffer, prevX, nextLeft->leftX);
            prevX = nextLeft->leftX;

            bool inserted = false;
            for (auto it = shapeStack.begin(); it != shapeStack.end(); ++it) {
                if ((*it)->z > nextLeft->z) {
                    shapeStack.insert(it, nextLeft);
                    inserted = true;
                    break;
                }
            }
            if (!inserted)
                shapeStack.push_back(nextLeft);

            nextLeft = nextLeft->nextInActiveList;
        } else {
            break;
        }
    }

    if (!shapeStack.empty())
        throw "activeShapes size not zero (at end)";
}

void Rasterizer::fillSpan(uint16_t* buffer,
                          FixedPointNumber fromFP, FixedPointNumber toFP) {

    int from = std::max(fromFP.floor(), 0);
    int to = std::min(toFP.floor(), width);
    if (from >= to)
        return;

    if (shapeStack.empty()) {
        printf("span: %d-%d\n", fromFP.floor(), toFP.floor());
        throw "shape stack empty";
    }

    int firstOpaque = shapeStack.size() - 1;
    while ((shapeStack[firstOpaque]->color & 0xFF000000) != 0xFF000000) {
        if (firstOpaque == 0)
            throw "no opaque shapes on stack";
        firstOpaque--;
    }

    Color color = shapeStack[firstOpaque]->color;
    for (int i = firstOpaque + 1; i < shapeStack.size(); i++)
        color = blendColorsDstOpaque(color, shapeStack[i]->color);

    unsigned int r = color >> 16 & 0xFF, g = color >> 8 & 0xFF, b = color & 0xFF;

    uint16_t rgb565 = ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | ((b & 0b11111000) >> 3);

    if (TRACE_FILLS)
        printf("fill with %04x\n", rgb565);

    for (; from < to; from++)
        buffer[from] = rgb565;
}

Color Rasterizer::blendColorsDstOpaque(Color dst, Color src) {
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

FixedPointNumber ConvexShape::slope(Point2D top, Point2D bottom) {
    return (bottom.x - top.x) / (bottom.y.floor() - top.y.floor());
}

IntermediateVertex ConvexShape::makeIntermediateVertex(Point2D top, Point2D bottom, bool isLeft) {
    return {
        .y = top.y.floor(),
        .isLeft = isLeft,
        .x = top.x,
        .slope = slope(top, bottom)
    };
}

ConvexShape* ConvexShape::create(
    std::vector<Point2D> leftPoints, std::vector<Point2D> rightPoints,
    FixedPointNumber z,
    Color color) {

    ConvexShape* shape = create(leftPoints.size(), rightPoints.size(), z, color);
    shape->modifyPoints(leftPoints, rightPoints);
    return shape;
}

ConvexShape* ConvexShape::create(
    int maxLeftVertexCount, int maxRightVertexCount,
    FixedPointNumber z,
    Color color) {

    if (maxLeftVertexCount < 2 || maxRightVertexCount < 2)
        throw "too few points";
    int maxIntermediatePointCount = maxLeftVertexCount - 2 + maxRightVertexCount - 2;

    ConvexShape* shape = (ConvexShape*)malloc(
        sizeof(ConvexShape) + sizeof(IntermediateVertex) * maxIntermediatePointCount);

    shape->color = color;
    shape->maxIntermediateVertexCount = maxIntermediatePointCount;
    shape->z = z;

    return shape;
}

void ConvexShape::modifyPoints(std::vector<Point2D> leftPoints, std::vector<Point2D> rightPoints) {
    if (leftPoints.size() < 2 || rightPoints.size() < 2)
        throw "too few points";
    if (leftPoints[0].y != rightPoints[0].y)
        throw "top point Y coordinate differs";
    if (leftPoints.back().y != rightPoints.back().y)
        throw "bottom point Y coordinate differs";
    int intermediatePointCount = leftPoints.size() - 2 + rightPoints.size() - 2;

    if (this->maxIntermediateVertexCount < intermediatePointCount)
        throw "intermediate vertex count too large";

    this->intermediateVertexCount = intermediatePointCount;

    this->top = leftPoints.front().y.floor();
    this->bottom = leftPoints.back().y.ceil();

    this->topLeftX = leftPoints[0].x;
    this->topRightX = rightPoints[0].x;
    this->topLeftSlope = (leftPoints[1].x - leftPoints[0].x) /
        (leftPoints[1].y.floor() - leftPoints[0].y.floor());
    this->topRightSlope = (rightPoints[1].x - rightPoints[0].x) /
        (rightPoints[1].y.floor() - rightPoints[0].y.floor());

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
