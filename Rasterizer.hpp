#pragma once

#include <vector>
#include "Fill.hpp"
#include <memory>
#include <span>

class Fill;

struct Point2D {
    float x;
    float y;

    void operator +=(Point2D other) {
        x += other.x;
        y += other.y;
    }
};

struct IntermediateVertex {
    int y;
    bool isLeft;
    float x;
    float slope;
};

class ConvexShape;

class ConvexShapeGroup {

    friend class Rasterizer;

public:
    ConvexShapeGroup() {}

    float zIndex;

private:
    std::vector<ConvexShape*> shapesInActiveList;
};

class ConvexShape {

    friend class Rasterizer;

    int top;
    /**
     * az ennyiedik sorban már nem lesz active ez a shape
     */
    int bottom;

    float topLeftX;
    float topRightX;
    float topLeftSlope;
    float topRightSlope;

public: // Fill.fillSpan miatt
    Fill* fill; // törölve lesz ~ConvexShape-ben

    ConvexShapeGroup* group;

private:
    int maxIntermediateVertexCount;
    int intermediateVertexCount;

    // Rasterizer::shapeListsByLine-ben használva
    ConvexShape* nextInTopRow;

    ConvexShape* prevInActiveList;
    ConvexShape* nextInActiveList;

    /**
     * Rasterizer::processCurrentLine-ból használva
     */
    int nextIntermediateVertexIndex;

    float leftX;
    float leftSlope;
    float rightX;
    float rightSlope;

    int leftXInt;
    int rightXInt;

    int minXFloor;

    /**
     * Fentről lefele haladva.
     */
    IntermediateVertex intermediateVertices[];

public:

    /**
     * Mindkét listában legalább két pontnak kell lennie (felső, alsó).
     * A pontok fentről lefele menjenek, és az első pont y-ja ugyanaz legyen mindkettő listában,
     * valamint az utolsó pont y-ja is ugyanaz legyen mindkettő listában.
     */
    static ConvexShape* create(std::span<Point2D> leftPoints, std::span<Point2D> rightPoints,
                               float z, Fill* fill);

    /**
     * Ezután a modifyPointsot meg kell hívni.
     */
    static ConvexShape* create(ConvexShapeGroup* group, int maxLeftPoints, int maxRightPoints,
                               Fill* fill);

    void modifyPoints(std::span<Point2D> leftPoints, std::span<Point2D> rightPoints);

    ~ConvexShape();

private:
    static float slope(Point2D top, Point2D bottom);
    static IntermediateVertex makeIntermediateVertex(Point2D top, Point2D bottom, bool isLeft);
};

class Rasterizer {

private:
    int width;
    int height;
    int y = 0;
    std::vector<ConvexShape*> shapeListsByLine;

    // sorrendje: 
    // 1. (int) leftX
    // 2. z
    ConvexShape* activeShapes;

public:
    std::vector<ConvexShape*> shapes;
    Fill* background;

    Rasterizer(int width, int height);

    void beginFrame();

    void skipLines(int lines);

    void prepareLine();
    void renderLine(uint16_t* buffer);
    void waitForLineFinish();

    int screenWidth();
    int screenHeight();

private:
    void addToActiveList(ConvexShape* shape);
    bool shouldBeAfterInActiveShapeList(ConvexShape* shape, ConvexShape* other);
    void removeFromActiveList(ConvexShape* shape);
    void shiftLeftInActiveList(ConvexShape* shape);
    bool processBreaksOnCurrentLine(ConvexShape* shape);
    void processCurrentLine();
    void checkActiveShapes();
    void printActiveShapes();
    void renderSpans(uint16_t* buffer);
    void fillSpan(uint16_t* buffer, float from, float to);
};