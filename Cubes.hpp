#include "Scene.hpp"

class CubeFace;
struct Point3D;
struct Point3DInt;


struct Point3D {
    FixedPointNumber x;
    FixedPointNumber y;
    FixedPointNumber z;

    Point3D(FixedPointNumber x, FixedPointNumber y, FixedPointNumber z) : x(x), y(y), z(z) {}
};

struct Point3DInt {
    int x;
    int y;
    int z;

    Point3DInt(int x, int y, int z) : x(x), y(y), z(z) {}

    inline Point3DInt operator + (Point3DInt other) {
        return Point3DInt(x + other.x, y + other.y, z + other.z);
    }
};

class Cubes : public Scene {
    std::vector<CubeFace> faces;
    uint64_t frameID;
    Point3D cameraPos;

    public:
    Cubes();
    void update();

    private:
    void projectCubeFace(CubeFace& face);
    Point2D projectPoint(Point3DInt p);
};

enum FaceSide {
    FACE_SIDE_TOP, 
    FACE_SIDE_BOTTOM, 
    FACE_SIDE_NORTH,
    FACE_SIDE_WEST, 
    FACE_SIDE_SOUTH, 
    FACE_SIDE_EAST
};

struct CubeFace {

    Point3DInt cubePos;
    FaceSide side;
    Color color;

    uint64_t updatedAtFrame;
    ConvexShape* cachedShape;
};
