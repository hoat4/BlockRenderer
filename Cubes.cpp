#include "Cubes.hpp"
#include <algorithm>
#include <stdio.h>
#include "textures.hpp"
#include "math.h"

Cubes::Cubes() : cameraPos(Point3D(0.0f, 0.0f, 0.0f)) {
    addCube(Point3DInt(1, -2, 4), GRASS);
    addCube(Point3DInt(-2, -2, 4), DIRT);
    //addCube(Point3DInt(-1, -2, 4), DIRT);
    //addCube(Point3DInt(-2, -2, 5), DIRT);
    //addCube(Point3DInt(-2, -2, 6), DIRT);
    //    addCube(Point3DInt(-1, -2, 6), DIRT);

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
        aspectRatio = (float) rasterizer->screenWidth() / (float) rasterizer->screenHeight();

        ConvexShape* background = ConvexShape::create(
            {
                { x: 0.0f, y : 0.0f },
                { x: 0.0f, y : (float) rasterizer->screenHeight() }
            },
            {
                { x: (float) rasterizer->screenWidth(), y : 0.0f },
                { x: (float) rasterizer->screenWidth(), y : (float) rasterizer->screenHeight() },
            },
            -std::numeric_limits<float>::infinity(),
            new ColorFill(0xFF55EEFF)
            );
        rasterizer->shapes.push_back(background);
    }

    frameID++;

    rasterizer->shapes.erase(rasterizer->shapes.begin() + 1, rasterizer->shapes.end());

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
                cube.cachedShapes[face] = faceShape = ConvexShape::create(4, 4, textureFill);
            }

            rasterizer->shapes.push_back(faceShape);
            Point2D point = projectCubeFace(cube, (CubeFace)face);
            computeZUVMatrix(cube, (CubeFace)face,
                             *reinterpret_cast<TextureFill*>(faceShape->fill), 
                            point);
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

Point2D Cubes::projectCubeFace(Cube& cube, CubeFace face) {
    // kamera pozíció és forgás alapján eliminálni lehetne a sortot

    std::vector<Point2D> points;

    //printf("uAtLeftTop:%d, uByX: %d\n",
    //       textureFill.uAtLeftTop.floor(), textureFill.uByX.floor());           

    points.push_back(projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4]));
    points.push_back(projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4 + 1]));
    points.push_back(projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4 + 2]));
    points.push_back(projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4 + 3]));

    std::sort(points.begin(), points.end(), [](Point2D a, Point2D b) {
        return a.y == b.y ? a.x < b.x : a.y < b.y;});

    /*
printf("projected points: %d,%d; %d,%d; %d,%d; %d,%d\n",
       points[0].x.floor(), points[0].y.floor(),
       points[1].x.floor(), points[1].y.floor(),
       points[2].x.floor(), points[2].y.floor(),
       points[3].x.floor(), points[3].y.floor()
);
*/


    std::vector<Point2D> leftPoints, rightPoints;

    float diagonalSlope = (points[3].x - points[0].x) / std::floor(points[3].y - points[0].y);

    leftPoints.push_back(points[0]);
    rightPoints.push_back(points[0]);

    if (points[1].x < points[0].x + diagonalSlope * std::floor(points[1].y - points[0].y))
        leftPoints.push_back(points[1]);
    else
        rightPoints.push_back(points[1]);

    if (points[2].x < points[0].x + diagonalSlope * std::floor(points[2].y - points[0].y))
        leftPoints.push_back(points[2]);
    else
        rightPoints.push_back(points[2]);

    leftPoints.push_back(points[3]);
    rightPoints.push_back(points[3]);

    Point3D cubeCenter(
        cube.pos.x + 0.5f,
        cube.pos.y + 0.5f,
        cube.pos.z + 0.5f
    );
    float zIndex = Point3D::distanceSq(cubeCenter, cameraPos);

    cube.cachedShapes[face]->modifyPoints(leftPoints, rightPoints, -zIndex);

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

    if (face == FACE_SOUTH&&false) {
        printf("\n");
        printf("base: %f, %f\n", p.x, p.y);
        printf("(0, 0): z: %f, u: %f, v: %f\n", 1/origin.zReciprocal, origin.uPerZ/origin.zReciprocal, origin.vPerZ/origin.zReciprocal);
        printf("(%d, 0): z: %f, u: %f, v: %f\n", pixelStepRight, 1/right.zReciprocal, right.uPerZ/right.zReciprocal, right.vPerZ/right.zReciprocal);
        printf("(0, 1): z: %f, u: %f, v: %f\n", 1/down.zReciprocal, down.uPerZ/down.zReciprocal, down.vPerZ/down.zReciprocal);
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

    u *= 15;
    v *= 15;

    return {
        .zReciprocal = 1 / z,
        .uPerZ = u / z,
        .vPerZ = v / z
    };
}
