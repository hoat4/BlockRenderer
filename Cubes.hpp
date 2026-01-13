#include "Scene.hpp"

struct Cube;
struct Point3D;
struct Point3DInt;


struct Point3D {
    FixedPointNumber x;
    FixedPointNumber y;
    FixedPointNumber z;

    Point3D(FixedPointNumber x, FixedPointNumber y, FixedPointNumber z) : x(x), y(y), z(z) {}

    inline static FixedPointNumber distanceSq(Point3D a, Point3D b) {
        FixedPointNumber dx = a.x-b.x;
        FixedPointNumber dy = a.y-b.y;
        FixedPointNumber dz = a.z-b.z;
        return FixedPointNumber::hypotSq(dx, dy, dz);
    }
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

// sorrend CUBE_VERTEX_OFFSETS-ben is van használva
enum CubeFace {
    FACE_TOP, 
    FACE_BOTTOM, 
    FACE_NORTH,
    FACE_WEST,  
    FACE_SOUTH, 
    FACE_EAST
};

class Cubes : public Scene {
    std::vector<Cube> cubes;
    uint64_t frameID;
    Point3D cameraPos;

    public:
    Cubes();
    void update();

    private:
    void addCube(Point3DInt pos);
    void projectCubeFace(Cube& cube, CubeFace face, TextureFill& textureFill);
    Point2D projectPoint(Point3DInt p);
};

struct Cube {

    Point3DInt pos;

    uint64_t updatedAtFrame;
    ConvexShape* cachedShapes[6];
};
