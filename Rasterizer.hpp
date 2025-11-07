#include "SpanRenderer.hpp"
#include "FixedPointNumber.hpp"
#include <vector>

struct Point2D {
    FixedPointNumber x;
    FixedPointNumber y;
};

struct Quad2D;

struct VertexPointer {
    int quadIndex;
    int vertex;
};

struct Quad2D {

    /**
     * Sorrend: felső, jobb, alsó, bal.
     * Ha a teteje vízszintes, akkor a
     * "felső" legyen a jobb felső, a "bal" pedig a bal felső.
     */
    Point2D points[4];

    /**
     * ugyanolyan sorrendben, mint points
     */
    VertexPointer nexts[4];

    uint16_t color;

    SpanEdge* rightEdge;

    static const int VERTICES = 4;

    Quad2D(Point2D a, Point2D b, Point2D c, Point2D d, uint16_t color) :
        points({ a, b, c, d }), 
        nexts({ {quadIndex: -1}, {quadIndex: -1}, {quadIndex: -1}, {quadIndex: -1} }), 
        color(color) {}
};

class Rasterizer {

private:
    int width;
    int height;
    SpanRenderer spanRenderer;
    int y = 0;
    std::vector<VertexPointer> quadListsByLine;

public:
    std::vector<Quad2D> quads;

    Rasterizer(int width, int height);

    void beginFrame();

    void skipLines(int lines);

    void renderLine(uint16_t* buffer);

private:
    void pushToQuadList(Quad2D& q, int quadIndex, int vertex);
    void processCurrentLine();
};