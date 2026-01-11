#pragma once

#include "FixedPointNumber.hpp"
#include <vector>

typedef uint32_t Color; // TODO

struct Point2D {
    FixedPointNumber x;
    FixedPointNumber y;

    void operator +=(Point2D other) {
        x += other.x;
        y += other.y;
    }
};

struct IntermediateVertex {
    int y;
    bool isLeft;
    FixedPointNumber x;
    FixedPointNumber slope;
};

class ConvexShape {

    friend class Rasterizer;

    int top;
    /**
     * az ennyiedik sorban már nem lesz active ez a shape
     */
    int bottom;

    FixedPointNumber topLeftX;
    FixedPointNumber topRightX;
    FixedPointNumber topLeftSlope;
    FixedPointNumber topRightSlope;
    
    FixedPointNumber z;

    Color color;

    int intermediateVertexCount;

    // Rasterizer::shapeListsByLine-ben használva
    ConvexShape* nextInTopRow;

    ConvexShape* prevInActiveList;
    ConvexShape* nextInActiveList;

    /**
     * Rasterizer::processCurrentLine-ból használva
     */
    int nextIntermediateVertexIndex;

    FixedPointNumber leftX;
    FixedPointNumber leftSlope;
    FixedPointNumber rightX;
    FixedPointNumber rightSlope;

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
    static ConvexShape* create(std::vector<Point2D> leftPoints, std::vector<Point2D> rightPoints, FixedPointNumber z, Color color);

    private:
    static FixedPointNumber slope(Point2D top, Point2D bottom);
    static IntermediateVertex makeIntermediateVertex(Point2D top, Point2D bottom, bool isLeft);
};

class Rasterizer {

private:
    int width;
    int height;
    int y = 0;
    std::vector<ConvexShape*> shapeListsByLine; 
    ConvexShape* activeShapes; 
    std::vector<ConvexShape*> shapeStack;

public:
    std::vector<ConvexShape*> shapes;

    Rasterizer(int width, int height);

    void beginFrame();

    void skipLines(int lines);

    void renderLine(uint16_t* buffer);

    int screenWidth();
    int screenHeight();

private:
    void addToActiveList(ConvexShape* shape);
    void removeFromActiveList(ConvexShape* shape);
    void shiftLeftInActiveList(ConvexShape* shape);
    bool processBreaksOnCurrentLine(ConvexShape* shape);
    void processCurrentLine();
    void checkActiveShapes();
    void printActiveShapes();
    void renderSpans(uint16_t* buffer);
    void fillSpan(uint16_t* buffer, FixedPointNumber from, FixedPointNumber to);
    Color blendColorsDstOpaque(Color dst, Color src);
};