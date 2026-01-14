#include "Rasterizer.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <math.h>

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
        if (s->top == s->bottom)
            // processCurrentLine előbb nézi a kiszedhetőeket mint a berakhatóakat,
            // ezért bennmaradnának a képernyő aljáig a 0 magasságú shapeek
            continue;
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
        printf("Add %s at %d-%d\n", shapeToAdd->fill->toString().c_str(),
               shapeToAdd->leftX, shapeToAdd->rightX);
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
        printf("Remove %s\n", shape->fill->toString().c_str());
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
        printf("%s at %f-%f, ", shape->fill->toString().c_str(), shape->leftX, shape->rightX);
    }
    printf("\n");
}

void Rasterizer::renderSpans(uint16_t* buffer) {
    if (!shapeStack.empty())
        throw "activeShapes size not zero (at begin)";

    if (TRACE_FILLS) {
        printActiveShapes();
    }

    float prevX = 0;
    ConvexShape* nextLeft = activeShapes;
    while (true) {
        int shapeWithNearestRight = -1;
        float nearestRight = std::numeric_limits<float>::infinity();
        for (int i = 0; i < shapeStack.size(); i++) {
            if (shapeStack[i]->rightX < nearestRight) {
                shapeWithNearestRight = i;
                nearestRight = shapeStack[i]->rightX;
            }
        }
        if (shapeWithNearestRight != -1 && (nextLeft == NULL || nearestRight <= nextLeft->leftX)) {
            if (TRACE_FILLS)
                printf("RIGHT at %f-%f\n", prevX, nearestRight);
            fillSpan(buffer, prevX, nearestRight);
            prevX = nearestRight;
            shapeStack.erase(shapeStack.begin() + shapeWithNearestRight);
        } else if (nextLeft) {
            if (TRACE_FILLS)
                printf("LEFT at %f-%f\n", prevX, nextLeft->leftX);
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

/**
 * fromFP nem lehet negatív (mert intté kasztolás rosszul működne, de amúgy is értelmetlen)
 */
void Rasterizer::fillSpan(uint16_t* buffer,
                          float fromFP, float toFP) {
    if (fromFP < 0)
        throw "negative fromFP";

    int from = std::max((int)fromFP, 0);
    int to = std::min((int)toFP, width);
    if (from >= to)
        return;

    if (shapeStack.empty()) {
        printf("span: %f-%f\n", fromFP, toFP);
        throw "shape stack empty";
    }

    ConvexShape* lastShape = shapeStack.back();
    if (TRACE_FILLS)
        printf("fill with %s\n", lastShape->fill->toString().c_str());

    lastShape->fill->fillSpan(&buffer[from], &buffer[to], shapeStack, from, y);
}

float ConvexShape::slope(Point2D top, Point2D bottom) {
    return (bottom.x - top.x) / (std::floor(bottom.y) - std::floor(top.y));
}

IntermediateVertex ConvexShape::makeIntermediateVertex(Point2D top, Point2D bottom, bool isLeft) {
    return {
        .y = (int) std::floor(top.y),
        .isLeft = isLeft,
        .x = top.x,
        .slope = slope(top, bottom)
    };
}

ConvexShape* ConvexShape::create(
    std::vector<Point2D> leftPoints, std::vector<Point2D> rightPoints,
    float z,
    Fill* fill) {

    ConvexShape* shape = create(leftPoints.size(), rightPoints.size(), fill);
    shape->modifyPoints(leftPoints, rightPoints, z);
    return shape;
}

ConvexShape* ConvexShape::create(
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

    return shape;
}

void ConvexShape::modifyPoints(std::vector<Point2D> leftPoints, std::vector<Point2D> rightPoints,
                               float z) {
    if (leftPoints.size() < 2 || rightPoints.size() < 2)
        throw "too few points";
    if (leftPoints[0].y != rightPoints[0].y)
        throw "top point Y coordinate differs";
    if (leftPoints.back().y != rightPoints.back().y)
        throw "bottom point Y coordinate differs";
    int intermediatePointCount = leftPoints.size() - 2 + rightPoints.size() - 2;

    if (this->maxIntermediateVertexCount < intermediatePointCount)
        throw "intermediate vertex count too large";


    this->top = (int) std::floor(leftPoints.front().y);
    this->bottom = (int) std::ceil(leftPoints.back().y);
    this->z = z;

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
