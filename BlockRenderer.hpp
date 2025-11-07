#include "SpanRenderer.hpp"
#include "Rasterizer.hpp"

class Game {

private:
    int width;
    int height;
    Rasterizer rasterizer;
    int expectedY;

public:
    Game(int width, int height);
    void beginFrame();
    void renderLine(int y, uint16_t* buffer);

};
