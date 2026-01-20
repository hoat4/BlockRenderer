#include "Cubes.hpp"
#include <algorithm>
#include <stdio.h>
#include "textures.hpp"
#include "math.h"

const bool TRACE_UV_MATRICES = 0;

Cubes::Cubes() : cameraPos(Point3D(0.0f, 0.0f, 0.0f)) {
    //addCube(Point3DInt(2, 0, 3), GRASS);
    for (int x = -5; x <= 4; x++) {
        for (int z = 4; z <= 11; z++) {
            addCube(Point3DInt(x, -3, z), GRASS);
        }
    }

    for (int x = 5; x <= 7; x++) {
        for (int z = 9; z <= 11; z++) {
            addCube(Point3DInt(x, -3, z), GRASS);
        }
    }

    addCube(Point3DInt(2, -2, 6), DIRT);
    addCube(Point3DInt(-4, -2, 6), DIRT);
    addCube(Point3DInt(-2, -2, 7), GRASS);
    addCube(Point3DInt(-3, -2, 8), DIRT);
    ///addCube(Point3DInt(2, -1, 6), DIRT);

    //addCube(Point3DInt(-2, -2, 7), DIRT);

    //addCube(Point3DInt(0, 0, 4), GRASS);
    //addCube(Point3DInt(1, 0, 4), DIRT);

    frameID = 0;
}

Cube* Cubes::findByPos(Point3DInt p) {
    for (Cube* cube : cubes) {
        if (cube->pos == p)
            return cube;
    }
    return NULL;
}

Point3DInt CUBE_NEIGHBOUR_OFFSETS[] = {
    /* TOP */    Point3DInt(0, 1, 0),
    /* BOTTOM */ Point3DInt(0, -1, 0),
    /* NORTH */  Point3DInt(0, 0, 1),
    /* WEST */   Point3DInt(-1, 0, 0),
    /* SOUTH */  Point3DInt(0, 0, -1),
    /* EAST */   Point3DInt(1, 0, 0),
};

void Cubes::addCube(Point3DInt p, CubeType type) {
    if (findByPos(p))
        throw "already exists";

    Cube* neighbours[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
    for (int face = 0; face < 6; face++)
        neighbours[face] = findByPos(p + CUBE_NEIGHBOUR_OFFSETS[face]);

    Cube* c = new Cube{
        .pos = p,
        .type = type,
        .updatedAtFrame = 0,
        .cachedShapes = {NULL, NULL, NULL, NULL, NULL, NULL},
        .convexShapeGroup = new ConvexShapeGroup(),
        .neighbours = {neighbours[0], neighbours[1], neighbours[2], neighbours[3], neighbours[4], neighbours[5] }
    };
    cubes.push_back(c);

    if (neighbours[FACE_TOP])
        neighbours[FACE_TOP]->neighbours[FACE_BOTTOM] = c;
    if (neighbours[FACE_BOTTOM])
        neighbours[FACE_BOTTOM]->neighbours[FACE_TOP] = c;
    if (neighbours[FACE_NORTH])
        neighbours[FACE_NORTH]->neighbours[FACE_SOUTH] = c;
    if (neighbours[FACE_SOUTH])
        neighbours[FACE_SOUTH]->neighbours[FACE_NORTH] = c;
    if (neighbours[FACE_WEST])
        neighbours[FACE_WEST]->neighbours[FACE_EAST] = c;
    if (neighbours[FACE_EAST])
        neighbours[FACE_EAST]->neighbours[FACE_WEST] = c;
}

// sorrend (computeUVMatrix-nak kell): 
// - 0.: textúra bal felül
// - 1.: textúra jobb felül
// - 2.: textúra bal alul
// - 3.: textúra jobb alul
Point3DInt CUBE_VERTEX_OFFSETS[] = {
    /* TOP */    Point3DInt(0, 1, 1), Point3DInt(1, 1, 1), Point3DInt(0, 1, 0), Point3DInt(1, 1, 0),
    /* BOTTOM */ Point3DInt(0, 0, 0), Point3DInt(1, 0, 0), Point3DInt(0, 0, 1), Point3DInt(1, 0, 1),
    /* NORTH */  Point3DInt(1, 1, 1), Point3DInt(0, 1, 1), Point3DInt(1, 0, 1), Point3DInt(0, 0, 1),
    /* WEST */   Point3DInt(0, 1, 1), Point3DInt(0, 1, 0), Point3DInt(0, 0, 1), Point3DInt(0, 0, 0),
    /* SOUTH */  Point3DInt(0, 1, 0), Point3DInt(1, 1, 0), Point3DInt(0, 0, 0), Point3DInt(1, 0, 0),
    /* EAST */   Point3DInt(1, 1, 0), Point3DInt(1, 1, 1), Point3DInt(1, 0, 0), Point3DInt(1, 0, 1),
};

void Cubes::update() {
    if (frameID == 0) {
        printf("create bg\n");
        aspectRatio = (float)rasterizer->screenWidth() / (float)rasterizer->screenHeight();

        rasterizer->background = new ColorFill(0xFF55EEFF);
    }

    frameID++;

    rasterizer->shapes.erase(rasterizer->shapes.begin(), rasterizer->shapes.end());

    for (auto it = cubes.begin(); it != cubes.end(); ++it) {
        Cube& cube = **it;
        cube.updatedAtFrame = frameID;

        Point3D cubeCenter = cube.pos + Point3D(0.5, 0.5, 0.5);
        cube.convexShapeGroup->zIndex = -Point3D::distanceSq(cubeCenter, cameraPos);

        for (int face = 0; face < 6; face++) {
            bool faceVisible;
            if (cube.neighbours[face])
                faceVisible = false;
            else {
                switch (face) {
                case FACE_TOP:
                    faceVisible = cameraPos.y > cube.pos.y + 1;
                    break;
                case FACE_BOTTOM:
                    faceVisible = cameraPos.y < cube.pos.y;
                    break;
                case FACE_NORTH:
                    faceVisible = false;
                    break;
                case FACE_WEST:
                    faceVisible = cameraPos.x < cube.pos.x;
                    break;
                case FACE_EAST:
                    faceVisible = cameraPos.x > cube.pos.x + 1;
                    break;
                case FACE_SOUTH:
                    faceVisible = true;
                    break;
                }
            }
            if (!faceVisible) {
                if (cube.cachedShapes[face]) {
                    delete cube.cachedShapes[face];
                    cube.cachedShapes[face] = NULL;
                }
                continue;
            }

            bool interpolatedTexture = cube.pos.z >= 9; // TODO kamera forgatását figyelembe kell itt majd venni
            ConvexShape* faceShape = cube.cachedShapes[face];
            if (faceShape == NULL) {
                const uint16_t* texture = cube.type == GRASS ?
                    face == FACE_TOP ? GRASS_TOP_TEXTURE :
                    face == FACE_BOTTOM ? DIRT_TEXTURE : GRASS_SIDE_TEXTURE :
                    DIRT_TEXTURE;

                Fill* fill;

                if (interpolatedTexture) {
                    InterpolatedTextureFill* textureFill = new InterpolatedTextureFill();
                    textureFill->texture = texture;
                    fill = textureFill;
                } else {
                    PerspectiveTextureFill* textureFill = new PerspectiveTextureFill();
                    textureFill->texture = texture;
                    fill = textureFill;
                }

                cube.cachedShapes[face] = faceShape = ConvexShape::create(
                    cube.convexShapeGroup, 4, 4, fill);
            }

            std::array<Point2D, 4> projectedPoints = {
                projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4]),
                projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4 + 1]),
                projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4 + 2]),
                projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4 + 3])
            };

            rasterizer->shapes.push_back(faceShape);
            setCubeFaceVertices(cube, (CubeFace)face, projectedPoints);

            if (interpolatedTexture) {
                computeUVMatrix(cube, (CubeFace)face,
                                *reinterpret_cast<InterpolatedTextureFill*>(faceShape->fill),
                                projectedPoints);
            } else {
                computeZUVMatrix(cube, (CubeFace)face,
                                 *reinterpret_cast<PerspectiveTextureFill*>(faceShape->fill),
                                 projectedPoints[0]);
            }
        }
    }

    for (auto it = cubes.begin(); it != cubes.end(); ++it) {
        Cube& t = *(*it);
        if (t.updatedAtFrame != frameID && t.updatedAtFrame != 0) {
            for (int face = 0; face < 6; face++) {
                delete t.cachedShapes[face];
                t.cachedShapes[face] = NULL;
            }
            t.updatedAtFrame = 0;
        }
    }
}

void Cubes::setCubeFaceVertices(Cube& cube, CubeFace face, std::array<Point2D, 4> points) {
    // kamera pozíció és forgás alapján eliminálni lehetne a sortot

    //printf("uAtLeftTop:%d, uByX: %d\n",
    //       textureFill.uAtLeftTop.floor(), textureFill.uByX.floor());           

    if (TRACE_UV_MATRICES) {
        for (Point2D p : points) {
            ZUV zuv = computeZUV(cube, face, p.x, p.y);
            printf("Face %d, point (%f, %f): z=%f, u=%f, v=%f\n", face, p.x, p.y, 1.0f / zuv.zReciprocal, zuv.uPerZ / zuv.zReciprocal, zuv.vPerZ / zuv.zReciprocal);
        }
    }

    /*
    Point2D p = { .x = 399, .y = 319 };
    ZUV zuv = computeZUV(cube, face, p.x, p.y);
    printf("Face %d, point (%f, %f): z=%f, u=%f, v=%f\n", face, p.x, p.y, 1.0f / zuv.zReciprocal, zuv.uPerZ / zuv.zReciprocal, zuv.vPerZ / zuv.zReciprocal);
    */

    std::sort(std::begin(points), std::end(points), [](Point2D a, Point2D b) {
        return a.y == b.y ? a.x < b.x : a.y < b.y;});

    /*
printf("projected points: %d,%d; %d,%d; %d,%d; %d,%d\n",
       points[0].x.floor(), points[0].y.floor(),
       points[1].x.floor(), points[1].y.floor(),
       points[2].x.floor(), points[2].y.floor(),
       points[3].x.floor(), points[3].y.floor()
);
*/


    std::array<Point2D, 4> leftPoints, rightPoints;

    float diagonalSlope = (points[3].x - points[0].x) / std::floor(points[3].y - points[0].y);

    leftPoints[0] = points[0];
    rightPoints[0] = points[0];
    int leftPointCount = 1, rightPointCount = 1;

    if (points[1].x < points[0].x + diagonalSlope * std::floor(points[1].y - points[0].y))
        leftPoints[leftPointCount++] = points[1];
    else
        rightPoints[rightPointCount++] = points[1];

    if (points[2].x < points[0].x + diagonalSlope * std::floor(points[2].y - points[0].y))
        leftPoints[leftPointCount++] = points[2];
    else
        rightPoints[rightPointCount++] = points[2];

    leftPoints[leftPointCount++] = points[3];
    rightPoints[rightPointCount++] = points[3];

    cube.cachedShapes[face]->modifyPoints(
        std::span{ leftPoints }.subspan(0, leftPointCount),
        std::span{ rightPoints }.subspan(0, rightPointCount)
    );
}

Point2D Cubes::projectPoint(Point3DInt p) {
    //printf("project point %d,%d\n", p.x, p.y);
    float x = p.x * rasterizer->screenWidth() / 2.0f;
    float y = p.y * rasterizer->screenHeight() / 2.0f * aspectRatio;
    x = x / p.z;
    y = y / p.z;
    return {
        .x = rasterizer->screenWidth() / 2.0f + x,
        .y = rasterizer->screenHeight() / 2.0f - y
    };
}

void Cubes::computeZUVMatrix(Cube& cube, CubeFace face, PerspectiveTextureFill& textureFill, Point2D p) {
    // "1/z, u/z and v/z are linear in screen space": 
    // https://www.lysator.liu.se/~mikaelk/doc/perspectivetexture/

    // itt írják hogy hogyan lehet kiszámolni egyszerűbben 1/z,u/z,v/z változását:
    // https://flipcode.com/archives/The_Art_of_Demomaking-Issue_14_Perspective_Correct_Texture_Mapping.shtml

    int pixelStepRight = 8;

    textureFill.baseX = p.x;
    textureFill.baseY = p.y;
    ZUV origin = computeZUV(cube, face, textureFill.baseX, textureFill.baseY);
    ZUV right = computeZUV(cube, face, textureFill.baseX + pixelStepRight, textureFill.baseY);
    ZUV down = computeZUV(cube, face, textureFill.baseX, textureFill.baseY + 1);

    if (face == FACE_SOUTH && false) {
        printf("\n");
        printf("base: %f, %f\n", p.x, p.y);
        printf("(0, 0): z: %f, u: %f, v: %f\n", 1 / origin.zReciprocal, origin.uPerZ / origin.zReciprocal, origin.vPerZ / origin.zReciprocal);
        printf("(%d, 0): z: %f, u: %f, v: %f\n", pixelStepRight, 1 / right.zReciprocal, right.uPerZ / right.zReciprocal, right.vPerZ / right.zReciprocal);
        printf("(0, 1): z: %f, u: %f, v: %f\n", 1 / down.zReciprocal, down.uPerZ / down.zReciprocal, down.vPerZ / down.zReciprocal);
        printf("\n");
    }

    textureFill.zReciprocal.base = origin.zReciprocal;
    textureFill.zReciprocal.by8X = right.zReciprocal - origin.zReciprocal;
    textureFill.zReciprocal.byX = textureFill.zReciprocal.by8X * (1.0f / pixelStepRight);
    textureFill.zReciprocal.byY = down.zReciprocal - origin.zReciprocal;

    textureFill.uPerZ.base = origin.uPerZ;
    textureFill.uPerZ.by8X = right.uPerZ - origin.uPerZ;
    textureFill.uPerZ.byX = textureFill.uPerZ.by8X * (1.0f / pixelStepRight);
    textureFill.uPerZ.byY = down.uPerZ - origin.uPerZ;

    textureFill.vPerZ.base = origin.vPerZ;
    textureFill.vPerZ.by8X = right.vPerZ - origin.vPerZ;
    textureFill.vPerZ.byX = textureFill.vPerZ.by8X * (1.0f / pixelStepRight);
    textureFill.vPerZ.byY = down.vPerZ - origin.vPerZ;

    if (TRACE_UV_MATRICES) {
        Point2D p2 = { .x = 274, .y = 285 };
        float zReciprocal = textureFill.zReciprocal.valueAt(p2.x - p.x, p2.y - p.y);
        float uPerZ = textureFill.uPerZ.valueAt(p2.x - p.x, p2.y - p.y);
        float vPerZ = textureFill.vPerZ.valueAt(p2.x - p.x, p2.y - p.y);
        printf("Face %d, point (%f, %f) with matrix: z=%f, u=%f, v=%f\n", face, p2.x, p2.y, 1.0f / zReciprocal, uPerZ / zReciprocal, vPerZ / zReciprocal);
        printf("1/z: %f, u/z: %f, v/z: %f\n", zReciprocal, uPerZ, vPerZ);
        zReciprocal += textureFill.zReciprocal.byX * 15;
        uPerZ += textureFill.uPerZ.byX * 15;
        vPerZ += textureFill.vPerZ.byX * 15;
        printf("1/z: %f, u/z: %f, v/z: %f\n", zReciprocal, uPerZ, vPerZ);
        printf("Face %d, point (%f+15, %f) with matrix: z=%f, u=%f, v=%f\n", face, p2.x, p2.y, 1.0f / zReciprocal, uPerZ / zReciprocal, vPerZ / zReciprocal);
    }
}

ZUV Cubes::computeZUV(Cube& cube, CubeFace face, float x, float y) {
    float xNormalized = x * 2.0 / rasterizer->screenWidth() - 1.0;
    float yNormalized = (1.0 - y * 2.0 / rasterizer->screenHeight()) / aspectRatio;

    float z;
    float u;
    float v;
    switch (face) {
    case FACE_NORTH:
        z = cube.pos.z + 1;
        u = cube.pos.x + 1 - xNormalized * z;
        v = cube.pos.y + 1 - yNormalized * z;
        break;
    case FACE_SOUTH:
        z = cube.pos.z;
        u = xNormalized * z - cube.pos.x;
        v = cube.pos.y + 1 - yNormalized * z;
        break;
    case FACE_WEST:
        z = (float)cube.pos.x / xNormalized;
        u = cube.pos.z + 1 - z;
        v = cube.pos.y + 1 - yNormalized * z;
        break;
    case FACE_EAST:
        z = (float)(cube.pos.x + 1) / xNormalized;
        u = z - cube.pos.z;
        v = cube.pos.y + 1 - yNormalized * z;
        break;
    case FACE_TOP:
        z = (float)(cube.pos.y + 1) / yNormalized;
        u = xNormalized * z - cube.pos.x;
        v = cube.pos.z + 1 - z;
        break;
    case FACE_BOTTOM:
        z = (float)cube.pos.y / yNormalized;
        u = xNormalized * z - cube.pos.x;
        v = z - cube.pos.z;
        break;
    }

    u = u * 16;
    v = v * 16;

    u *= 1 << PerspectiveTextureFill::UV_FRACTIONAL_BITS;
    v *= 1 << PerspectiveTextureFill::UV_FRACTIONAL_BITS;

    return {
        .zReciprocal = 1 / z,
        .uPerZ = u / z,
        .vPerZ = v / z,
        .u = u,
        .v = v
    };
}


void Cubes::computeUVMatrix(Cube& cube, CubeFace face, InterpolatedTextureFill& textureFill,
                            std::array<Point2D, 4> points) {

    textureFill.baseX = std::floor(points[0].x);
    textureFill.baseY = std::floor(points[0].y);

    Point2D pointAtTextureRightTop = {
        .x = std::floor(points[1].x) - textureFill.baseX,
        .y = std::floor(points[1].y) - textureFill.baseY
    };

    Point2D pointAtTextureLeftBottom = {
        .x = std::floor(points[2].x) - textureFill.baseX,
        .y = std::floor(points[2].y) - textureFill.baseY
    };

    Point2D pointAtTextureRightBottom = {
        .x = std::floor(points[3].x) - textureFill.baseX,
        .y = std::floor(points[3].y) - textureFill.baseY
    };

    float m00 = (pointAtTextureRightTop.x) / 15.0f;
    float m01 = (pointAtTextureLeftBottom.x) / 15.0f;
    float m10 = (pointAtTextureRightTop.y) / 15.0f;
    float m11 = (pointAtTextureLeftBottom.y) / 15.0f;
    float detInv = 1 / (m00 * m11 - m01 * m10);

    float im00 = m11 * detInv;
    float im10 = -m10 * detInv;
    float im01 = -m01 * detInv;
    float im11 = m00 * detInv;

    textureFill.uByX = im00 * (1 << InterpolatedTextureFill::UV_FRACTIONAL_BITS);
    textureFill.vByX = im10 * (1 << InterpolatedTextureFill::UV_FRACTIONAL_BITS);
    textureFill.uByY = im01 * (1 << InterpolatedTextureFill::UV_FRACTIONAL_BITS);
    textureFill.vByY = im11 * (1 << InterpolatedTextureFill::UV_FRACTIONAL_BITS);

    // TODO ha rightBottom esetén megnő 16 fölé U vagy V, akkor csökkenteni kéne a mátrix értékeit

    /*
    printf("Unproject (base at %d, %d): \n", textureFill.baseX, textureFill.baseY);
    printf("- (%f, %f): (%f, %f)\n", pointAtTextureRightTop.x, pointAtTextureRightTop.y,
           (textureFill.uByX * pointAtTextureRightTop.x +
           textureFill.uByY * pointAtTextureRightTop.y) / (1 << InterpolatedTextureFill::UV_FRACTIONAL_BITS),
           (textureFill.vByX * pointAtTextureRightTop.x +
           textureFill.vByY * pointAtTextureRightTop.y) / (1 << InterpolatedTextureFill::UV_FRACTIONAL_BITS));
    printf("- (%f, %f): (%f, %f)\n", pointAtTextureLeftBottom.x, pointAtTextureLeftBottom.y,
           (textureFill.uByX * pointAtTextureLeftBottom.x +
           textureFill.uByY * pointAtTextureLeftBottom.y) / (1 << InterpolatedTextureFill::UV_FRACTIONAL_BITS),
           (textureFill.vByX * pointAtTextureLeftBottom.x +
           textureFill.vByY * pointAtTextureLeftBottom.y) / (1 << InterpolatedTextureFill::UV_FRACTIONAL_BITS));
    printf("- (%f, %f): (%f, %f)\n", pointAtTextureRightBottom.x, pointAtTextureRightBottom.y,
           (textureFill.uByX * pointAtTextureRightBottom.x +
           textureFill.uByY * pointAtTextureRightBottom.y) / (1 << InterpolatedTextureFill::UV_FRACTIONAL_BITS),
           (textureFill.vByX * pointAtTextureRightBottom.x +
           textureFill.vByY * pointAtTextureRightBottom.y) / (1 << InterpolatedTextureFill::UV_FRACTIONAL_BITS));
    printf("\n");
    */
}
