#include "Rasterizer.hpp"

class Triangle;

class Game {

private:
    int width;
    int height;
    Rasterizer rasterizer;
    int expectedY;

    std::vector<Triangle> triangles;

public:
    Game(int width, int height);
    void beginFrame();
    void renderLine(int y, uint16_t* buffer);

private:
    void update();

};

class Triangle {
    public: 
        Point2D pos;
        Point2D dir;
        Color color;

        Triangle();

        ConvexShape* toShape(FixedPointNumber z);
};