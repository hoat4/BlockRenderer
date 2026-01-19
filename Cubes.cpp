#include "Cubes.hpp"
#include <algorithm>
#include <stdio.h>
#include "textures.hpp"
#include "math.h"

const bool TRACE_UV_MATRICES = 0;

Cubes::Cubes() : cameraPos(Point3D(0.0f, 0.0f, 0.0f)) {
    //addCube(Point3DInt(2, 0, 3), GRASS);
    for (int x = -4; x <= 3; x++) {
        for (int y = -2; y <= 2; y++) {
            if (y % 2 == 0)
                for (int z = 4; z <= 4; z++) {
                    addCube(Point3DInt(x, y, z), x >= 0 ? GRASS : DIRT);
                }
        }
    }
    addCube(Point3DInt(-2, -2, 5), DIRT);
//    addCube(Point3DInt(-2, -2, 5), DIRT);
    addCube(Point3DInt(-1, -2, 5), DIRT);
    addCube(Point3DInt(0, -2, 5), DIRT);
    addCube(Point3DInt(1, -2, 5), DIRT);
    addCube(Point3DInt(2, -2, 5), DIRT);
    addCube(Point3DInt(-2, -2, 6), DIRT);
    addCube(Point3DInt(-2, -2, 7), DIRT);

    //addCube(Point3DInt(0, 0, 4), GRASS);
    //addCube(Point3DInt(1, 0, 4), DIRT);

    frameID = 0;
}

void Cubes::addCube(Point3DInt p, CubeType type) {
    cubes.push_back({
        .pos = p,
        .type = type,
        .updatedAtFrame = 0,
        .cachedShapes = {NULL, NULL, NULL, NULL, NULL, NULL} });
}

void Cubes::update() {
    if (frameID == 0) {
        printf("create bg\n");
        aspectRatio = (float)rasterizer->screenWidth() / (float)rasterizer->screenHeight();

        rasterizer->background = new ColorFill(0xFF55EEFF);
    }

    frameID++;

    rasterizer->shapes.erase(rasterizer->shapes.begin(), rasterizer->shapes.end());

    for (auto it = cubes.begin(); it != cubes.end(); ++it) {
        Cube& cube = *it;
        cube.updatedAtFrame = frameID;
        for (int face = 0; face < 6; face++) {
            Color faceColor;
            bool faceVisible;
            switch (face) {
            case FACE_TOP:
                faceColor = 0xFFFF5555;
                faceVisible = cameraPos.y > cube.pos.y + 1;
                break;
            case FACE_BOTTOM:
                faceColor = 0xFFFF00FF;
                faceVisible = cameraPos.y < cube.pos.y;
                break;
            case FACE_NORTH:
                faceColor = 0xFFAA5500;
                faceVisible = false;
                break;
            case FACE_WEST:
                faceColor = 0xFF00FF00;
                faceVisible = cameraPos.x < cube.pos.x;
                break;
            case FACE_EAST:
                faceColor = 0xFF0000FF;
                faceVisible = cameraPos.x > cube.pos.x + 1;
                break;
            case FACE_SOUTH:
                faceColor = 0xFFFFFF00;
                faceVisible = true;
                break;
            }
            if (!faceVisible) {
                if (cube.cachedShapes[face]) {
                    delete cube.cachedShapes[face];
                    cube.cachedShapes[face] = NULL;
                }
                continue;
            }

            ConvexShape* faceShape = cube.cachedShapes[face];
            if (faceShape == NULL) {
                TextureFill* textureFill = new TextureFill();
                textureFill->texture = cube.type == GRASS ?
                    face == FACE_TOP ? GRASS_TOP_TEXTURE :
                    face == FACE_BOTTOM ? DIRT_TEXTURE : GRASS_SIDE_TEXTURE :
                    DIRT_TEXTURE;
                textureFill->uvCache = NULL;
                cube.cachedShapes[face] = faceShape = ConvexShape::create(4, 4, textureFill);
            }

            rasterizer->shapes.push_back(faceShape);
            int maxWidth;
            Point2D point = projectCubeFace(cube, (CubeFace)face, maxWidth);
            computeZUVMatrix(cube, (CubeFace)face,
                             *reinterpret_cast<TextureFill*>(faceShape->fill),
                             point);

            TextureFill* textureFill = reinterpret_cast<TextureFill*>(faceShape->fill);
            if (textureFill->uvCache)
                delete textureFill->uvCache;
            textureFill->uvCache = (int*) malloc(8 * ((maxWidth + 7) / 8));
        }
    }

    for (auto it = cubes.begin(); it != cubes.end(); ++it) {
        Cube& t = *it;
        if (t.updatedAtFrame != frameID && t.updatedAtFrame != 0) {
            for (int face = 0; face < 6; face++) {
                delete t.cachedShapes[face];
                t.cachedShapes[face] = NULL;
            }
            t.updatedAtFrame = 0;
        }
    }
}

// sorrend: 
// - 0.: textúra bal felül
// - 1.: textúra jobb felül
// - 2.: textúra bal alul
// - 3.: textúra jobb alul (valójában mindegy, de úgyis csak az maradt)
Point3DInt CUBE_VERTEX_OFFSETS[] = {
    /* TOP */    Point3DInt(0, 1, 1), Point3DInt(1, 1, 1), Point3DInt(0, 1, 0), Point3DInt(1, 1, 0),
    /* BOTTOM */ Point3DInt(0, 0, 0), Point3DInt(1, 0, 0), Point3DInt(0, 0, 1), Point3DInt(1, 0, 1),
    /* NORTH */  Point3DInt(1, 1, 1), Point3DInt(0, 1, 1), Point3DInt(1, 0, 1), Point3DInt(0, 0, 1),
    /* WEST */   Point3DInt(0, 1, 1), Point3DInt(0, 1, 0), Point3DInt(0, 0, 1), Point3DInt(0, 0, 0),
    /* SOUTH */  Point3DInt(0, 1, 0), Point3DInt(1, 1, 0), Point3DInt(0, 0, 0), Point3DInt(1, 0, 0),
    /* EAST */   Point3DInt(1, 1, 0), Point3DInt(1, 1, 1), Point3DInt(1, 0, 0), Point3DInt(1, 0, 1),
};
Point3D FACE_CENTER_OFFSETS[] = {
    /* TOP */ Point3D(0.5, 1, 0.5),
    /* BOTTOM */ Point3D(0.5, 0, 0.5),
    /* NORTH */ Point3D(0.5, 0.5, 1),
    /* WEST */ Point3D(0, 0.5, 0.5),
    /* SOUTH */ Point3D(0.5, 0.5, 0),
    /* EAST */ Point3D(1, 0.5, 0.5),
};

Point2D Cubes::projectCubeFace(Cube& cube, CubeFace face, int& maxWidth) {
    // kamera pozíció és forgás alapján eliminálni lehetne a sortot

    Point2D points[4] = {
        projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4]),
        projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4 + 1]),
        projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4 + 2]),
        projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4 + 3])
    };

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

    int minX = std::min(std::min((int)points[0].x, (int)points[1].x), std::min((int)points[2].x, (int)points[3].x));
    int maxX = std::max(std::max((int)points[0].x, (int)points[1].x), std::max((int)points[2].x, (int)points[3].x));
    maxWidth = maxX - minX;

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

    Point3D faceCenter = cube.pos + FACE_CENTER_OFFSETS[face];
    float zIndex = Point3D::distanceSq(faceCenter, cameraPos);

    cube.cachedShapes[face]->modifyPoints(
        std::span{ leftPoints }.subspan(0, leftPointCount),
        std::span{ rightPoints }.subspan(0, rightPointCount),
        -zIndex
    );

    return leftPoints[0];
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

void Cubes::computeZUVMatrix(Cube& cube, CubeFace face, TextureFill& textureFill, Point2D p) {
    // "1/z, u/z and v/z are linear in screen space": 
    // https://www.lysator.liu.se/~mikaelk/doc/perspectivetexture/

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

    u *= 1 << TextureFill::UV_FRACTIONAL_BITS;
    v *= 1 << TextureFill::UV_FRACTIONAL_BITS;

    return {
        .zReciprocal = 1 / z,
        .uPerZ = u / z,
        .vPerZ = v / z
    };
}
