#include "Rasterizer.hpp"
#include <stdio.h>
#include <stdlib.h>

const bool TRACE_CHANGE_ACTIVE_LIST = false;
const bool TRACE_FILLS = false;

Rasterizer::Rasterizer(int width, int height) : width(width), height(height) {
    shapeListsByLine.resize(height);
}

void Rasterizer::beginFrame() {
    std::fill(shapeListsByLine.begin(), shapeListsByLine.end(), (ConvexShape*) NULL);

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
    //if (y == 0)
    //    TRACE_FILLS = true;

    if (TRACE_FILLS || TRACE_CHANGE_ACTIVE_LIST)
        printf("line %d\n", y);
    processCurrentLine();
    renderSpans(buffer);

    //if (y == 0)
    //    TRACE_FILLS = false;

    y++;
}

void Rasterizer::addToActiveList(ConvexShape* shapeToAdd) {
    if (TRACE_CHANGE_ACTIVE_LIST || TRACE_FILLS)
        printf("Add %04x\n", shapeToAdd->color);
    ConvexShape* shape = activeShapes;
    ConvexShape* addAfter = NULL;
    for (ConvexShape* shape = activeShapes; shape && shape->leftX <= shapeToAdd->leftX; shape = shape->nextInActiveList) {
        addAfter = shape;
    }
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

void Rasterizer::processCurrentLine() {
    for (ConvexShape* shape = activeShapes; shape; ) {
        ConvexShape* next = shape->nextInActiveList; // removeFromActiveList megváltoztatja shape->nextInActiveList-et
        if (shape->bottom == y) {
            removeFromActiveList(shape);
        } else {
            if (!processBreaksOnCurrentLine(shape)) {
                shape->leftX += shape->leftSlope;
                shape->rightX += shape->rightSlope;
                // TODO más shapeek szélével metszés detektálása
            }
        }
        shape = next;
    }

    for (ConvexShape* shape = shapeListsByLine[y];
         shape != NULL;
         shape = shape->nextInTopRow) {

        addToActiveList(shape);

        shape->leftX = shape->topLeftX;
        shape->leftSlope = shape->topLeftSlope;
        shape->rightX = shape->topRightX;
        shape->rightSlope = shape->topRightSlope;

        processBreaksOnCurrentLine(shape);
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

void Rasterizer::renderSpans(uint16_t* buffer) {
    if (!shapeStack.empty())
        throw "activeShapes size not zero (at begin)";

    if (TRACE_FILLS) {
        printf("Active shapes: ");
        for (ConvexShape* shape = activeShapes; shape; shape = shape->nextInActiveList) {
            printf("%04x at %d-%d, ", shape->color, shape->leftX.floor(), shape->rightX.floor());
        }
        printf("\n");
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

    // TODO blend
    Color color = shapeStack.back()->color;
    if (TRACE_FILLS)
        printf("fill with %04x\n", color);

    for (; from < to; from++)
        buffer[from] = color;
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
    std::vector<Point2D> leftPoints, std::vector<Point2D> rightPoints, Color color) {

    if (leftPoints.size() < 2 || rightPoints.size() < 2)
        throw "too few points";
    if (leftPoints[0].y != rightPoints[0].y)
        throw "top point Y coordinate differs";
    if (leftPoints.back().y != rightPoints.back().y)
        throw "bottom point Y coordinate differs";
    int intermediatePointCount = leftPoints.size() - 2 + rightPoints.size() - 2;

    ConvexShape* shape = (ConvexShape*)malloc(
        sizeof(ConvexShape) + sizeof(IntermediateVertex) * intermediatePointCount);

    shape->top = leftPoints.front().y.floor();
    shape->bottom = leftPoints.back().y.ceil();

    shape->topLeftX = leftPoints[0].x;
    shape->topRightX = rightPoints[0].x;
    shape->topLeftSlope = (leftPoints[1].x - leftPoints[0].x) /
        (leftPoints[1].y.floor() - leftPoints[0].y.floor());
    shape->topRightSlope = (rightPoints[1].x - rightPoints[0].x) /
        (rightPoints[1].y.floor() - rightPoints[0].y.floor());

    shape->color = color;
    shape->intermediateVertexCount = intermediatePointCount;

    for (int i = 1, j = 1, v = 0; ; ) {
        if (i < leftPoints.size() - 1) {
            if (j < rightPoints.size() - 1 && rightPoints[j].y < leftPoints[i].y) {
                shape->intermediateVertices[v++] =
                    makeIntermediateVertex(rightPoints[j], rightPoints[j + 1], false);
                j++;
            } else {
                shape->intermediateVertices[v++] =
                    makeIntermediateVertex(leftPoints[i], leftPoints[i + 1], true);
                i++;
            }
        } else if (j < rightPoints.size() - 1) {
            shape->intermediateVertices[v++] =
                makeIntermediateVertex(rightPoints[j], rightPoints[j + 1], false);
            j++;
        } else {
            break;
        }
    }

    return shape;
}