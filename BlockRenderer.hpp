#include "Rasterizer.hpp"
#include "Scene.hpp"

class Game {

private:
    int width;
    int height;
    Rasterizer rasterizer;
    Scene* scene;

public:
    Game(int width, int height);
    void beginFrame();
    void renderLine(int y, uint16_t* buffer);

private:
    void update();

};