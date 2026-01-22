#include "Scene.hpp"

struct Cube;
struct Point3D;
struct Point3DInt;


struct Point3D {
    float x;
    float y;
    float z;

    Point3D(float x, float y, float z) : x(x), y(y), z(z) {}

    inline static float distanceSq(Point3D a, Point3D b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz; // FMA?
    }
};

struct Point3DInt {
    const int x;
    const int y;
    const int z;

    Point3DInt(int x, int y, int z) : x(x), y(y), z(z) {}

    inline Point3DInt operator + (Point3DInt other) {
        return Point3DInt(x + other.x, y + other.y, z + other.z);
    }

    inline Point3D operator + (Point3D other) {
        return Point3D(x + other.x, y + other.y, z + other.z);
    }

    inline Point3D toFloat() {
        return Point3D(x, y, z);
    }

    inline bool operator == (Point3DInt other) {
        return x == other.x && y == other.y && z == other.z;
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

enum CubeType {
    DIRT, GRASS, OAK_PLANKS
};

struct ZUV {
    float zReciprocal;
    float uPerZ;
    float vPerZ;
    float u;
    float v;
};

class Cubes : public Scene {
    std::vector<Cube*> cubes;
    uint64_t frameID;
    Point3D cameraPos;
    float aspectRatio;

public:
    Cubes();
    void update();

private:
    Cube* findByPos(Point3DInt pos);
    void addCube(Point3DInt pos, CubeType type);

    void setCubeFaceVertices(Cube& cube, CubeFace face, std::array<Point2D, 4> points);
    ZUV computeZUV(Cube& cube, CubeFace face, float x, float y);
    void computeZUVMatrix(Cube& cube, CubeFace face, PerspectiveTextureMapping& textureFill,
                          Point2D origin);
    void computeUVMatrix(Cube& cube, CubeFace face, InterpolatedTextureMapping& textureFill,
                         std::array<Point2D, 4> points);
    Point2D projectPoint(Point3D p);
};

struct Cube {

    Point3DInt pos;
    CubeType type;

    uint64_t updatedAtFrame;
    ConvexShape* cachedShapes[6];

    ConvexShapeGroup* convexShapeGroup;

    Cube* neighbours[6];
};
