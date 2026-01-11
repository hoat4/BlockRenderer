#include "Scene.hpp"

class Triangle;

class TriangleDemo : public Scene {
    std::vector<Triangle> triangles;

    public:
    TriangleDemo();
    void update();
};

class Triangle {
    public: 
        Point2D pos;
        Point2D dir;
        Color color;

        Triangle();

        ConvexShape* toShape(FixedPointNumber z);

    private:
        static uint32_t sqrt(uint32_t x);
};
