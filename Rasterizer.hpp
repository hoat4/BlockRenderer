#pragma once

#include <vector>
#include "Fill.hpp"
#include <memory>
#include <span>
#include "hardware/interp.h"
#include "hardware/dma.h"

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

struct ConvexShapeGroup {

    float zIndex;
    float x;
    float y; // ez valójában nincs is használva
};

class ConvexShape {

    friend class Rasterizer;
    friend class ConvexShapeGroup;

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

    std::string toString();

private:

    // Rasterizer::shapeListsByLine-ben használva
    ConvexShape* nextInTopRow;

    ConvexShape* prevInActiveList;
    ConvexShape* nextInActiveList;

    float leftX;
    float leftSlope;
    float rightX;
    float rightSlope;

    int leftXInt;
    int rightXInt;

    int minXFloor;

    IntermediateVertex intermediateVertices[2];


public:

    std::string debugName;

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

    // sorrendje: a shape-ek látható területének bal oldala szerint növekvő
    ConvexShape* activeShapes;

    int dmaChannel;
    dma_channel_config dmaChannelConfig;
    int dmaBuffer;

    interp_config interpolatorLaneUConfig;
    interp_config interpolatorLaneVConfig;

public:
    std::vector<ConvexShape*> shapes;
    uint16_t background;

    Rasterizer(int width, int height);

    void beginFrame();

    void skipLines(int lines);

    void prepareLine();
    void renderLine(uint16_t* buffer);
    void waitForLineFinish();

    int screenWidth();
    int screenHeight();

    static const int PERSPECTIVE_UV_FRACTIONAL_BITS = 12;
    static const int PERSPECTIVE_SCANLINE_SUBDIVISION_BITS = 3;
    static const int PERSPECTIVE_SCANLINE_SUBDIVISION = 1 << PERSPECTIVE_SCANLINE_SUBDIVISION_BITS;
    static const int INTERPOLATED_UV_FRACTIONAL_BITS =
        PERSPECTIVE_UV_FRACTIONAL_BITS +
        PERSPECTIVE_SCANLINE_SUBDIVISION_BITS;

private:
    void addToActiveList(ConvexShape* shape);
    bool shouldBeAfterInActiveShapeList(ConvexShape* shape, ConvexShape* other);
    void removeFromActiveList(ConvexShape* shape);
    void shiftLeftInActiveList(ConvexShape* shape);
    bool processBreaksOnCurrentLine(ConvexShape* shape);
    void processCurrentLine();
    void checkActiveShapes();
    void printActiveShapes();
    void initInterpolator();
    void renderSpans(uint16_t* buffer);
    void fillSpan(uint16_t* buffer, float from, float to);
    void fillWithBackground(uint16_t* from, uint16_t* to);
    static void fillWithPerspectiveTexture(Fill* texture,
                                    uint16_t* from, uint16_t* to, int x2, int y2);
    static void fillWithInterpolatedTexture(Fill* texture,
                                     uint16_t* from, uint16_t* to, int x2, int y2);

};