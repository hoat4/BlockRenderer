#include "Rasterizer.hpp"

class Game {

private:
    int width;
    int height;
    Rasterizer rasterizer;
    int expectedY;

    Point2D rect1Pos;
    Point2D rect2Pos;
    Point2D rect1Dir;
    Point2D rect2Dir;

public:
    Game(int width, int height);
    void beginFrame();
    void renderLine(int y, uint16_t* buffer);

private:
    void update();

};
