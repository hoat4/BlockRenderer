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
        float dx = a.x-b.x;
        float dy = a.y-b.y;
        float dz = a.z-b.z;
        return dx * dx + dy * dy + dz * dz; // FMA?
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

enum CubeType {
    DIRT, GRASS
};

struct ZUV {
    float zReciprocal;
    float uPerZ;
    float vPerZ;
};

class Cubes : public Scene {
    std::vector<Cube> cubes;
    uint64_t frameID;
    Point3D cameraPos;
    float aspectRatio;

    public:
    Cubes();
    void update();

    private:
    void addCube(Point3DInt pos, CubeType type);

    /** 
     * visszaad egy tetszőleges pontot, ami a kockán van
     */
    Point2D projectCubeFace(Cube& cube, CubeFace face);
    ZUV computeZUV(Cube& cube, CubeFace face, float x, float y);
    void computeZUVMatrix(Cube& cube, CubeFace face, TextureFill& textureFill, Point2D origin);
    Point2D projectPoint(Point3DInt p);
};

struct Cube {

    Point3DInt pos;
    CubeType type;

    uint64_t updatedAtFrame;
    ConvexShape* cachedShapes[6];
};
