#include "Cubes.hpp"
#include <algorithm>
#include <stdio.h>
#include "textures.hpp"

Cubes::Cubes() : cameraPos(Point3D(FixedPointNumber::zero(), FixedPointNumber::zero(), FixedPointNumber::zero())) {
    /*
    addCube(Point3DInt(-1, 0, 5));
    addCube(Point3DInt(1, 0, 5));
    */
    addCube(Point3DInt(1, -2, 5));
    /*
    addCube(Point3DInt(-3, -2, 10));
    addCube(Point3DInt(-3, 2, 10));
    addCube(Point3DInt(1, -1, 4));
    */

    frameID = 0;
}

void Cubes::addCube(Point3DInt p) {
    cubes.push_back({
        .pos = p,
        .updatedAtFrame = 0,
        .cachedShapes = {NULL, NULL, NULL, NULL, NULL, NULL} });
}

void Cubes::update() {
    if (frameID == 0) {
        printf("create bg\n");
        ConvexShape* background = ConvexShape::create(
            {
                { x: FixedPointNumber::zero(), y : FixedPointNumber::zero() },
                { x: FixedPointNumber::zero(), y : FixedPointNumber::ofInt(rasterizer->screenHeight()) }
            },
            {
                { x: FixedPointNumber::ofInt(rasterizer->screenWidth()), y : FixedPointNumber::zero() },
                { x: FixedPointNumber::ofInt(rasterizer->screenWidth()), y : FixedPointNumber::ofInt(rasterizer->screenHeight()) },
            },
            FixedPointNumber::min(),
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
                textureFill->texture = TEXTURE;
                cube.cachedShapes[face] = faceShape = ConvexShape::create(4, 4, textureFill);
            }

            rasterizer->shapes.push_back(faceShape);
            projectCubeFace(cube, (CubeFace)face,
                            *reinterpret_cast<TextureFill*>(faceShape->fill));
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

void Cubes::projectCubeFace(Cube& cube, CubeFace face, TextureFill& textureFill) {
    // kamera pozíció és forgás alapján eliminálni lehetne a sortot

    std::vector<Point2D> points;

    Point2D pointAtTextureLeftTop = projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4]);
    Point2D pointAtTextureRightTop = projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4 + 1]);
    Point2D pointAtTextureLeftBottom = projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4 + 2]);
    Point2D pointAtTextureRightBottom = projectPoint(cube.pos + CUBE_VERTEX_OFFSETS[face * 4 + 3]);

    float m00 = (pointAtTextureRightTop.x.toFloat() - pointAtTextureLeftTop.x.toFloat()) / 15;
    float m01 = (pointAtTextureLeftBottom.x.toFloat() - pointAtTextureLeftTop.x.toFloat()) / 15;
    float m02 = pointAtTextureLeftTop.x.toFloat();
    float m10 = (pointAtTextureRightTop.y.toFloat() - pointAtTextureLeftTop.y.toFloat()) / 15;
    float m11 = (pointAtTextureLeftBottom.y.toFloat() - pointAtTextureLeftTop.y.toFloat()) / 15;
    float m12 = pointAtTextureLeftTop.y.toFloat();
    float detInv = 1 / (m00 * m11 - m01 * m10);
    
    /*
    printf("SIDE %d\n", face);
    printf("m:\n%f,%f,%f\n%f,%f,%f\n", m00, m01, m02, m10, m11, m12);
    printf("devInv:%f\n", detInv);
    */
    

    float im00 = m11 * detInv;
    float im10 = -m10 * detInv;
    float im01 = -m01 * detInv;
    float im11 = m00 * detInv;
    float im02 = (m01 * m12 - m11 * m02) * detInv;
    float im12 = (m10 * m02 - m00 * m12) * detInv;
 //   printf("inverse:\n%f,%f,%f\n%f,%f,%f\n", im00, im01, im02, im10, im11, im12);
    textureFill.uAtLeftTop = FixedPointNumber::ofFloat(im02);
    textureFill.vAtLeftTop = FixedPointNumber::ofFloat(im12);
    textureFill.uByX = FixedPointNumber::ofFloat(im00);
    textureFill.vByX = FixedPointNumber::ofFloat(im10);
    textureFill.uByY = FixedPointNumber::ofFloat(im01);
    textureFill.vByY = FixedPointNumber::ofFloat(im11);
    //printf("uAtLeftTop:%d, uByX: %d\n",
    //       textureFill.uAtLeftTop.floor(), textureFill.uByX.floor());
           

    points.push_back(pointAtTextureLeftTop);
    points.push_back(pointAtTextureRightTop);
    points.push_back(pointAtTextureLeftBottom);
    points.push_back(pointAtTextureRightBottom);

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

    FixedPointNumber diagonalSlope = (points[3].x - points[0].x) / (points[3].y - points[0].y).floor();

    leftPoints.push_back(points[0]);
    rightPoints.push_back(points[0]);

    if (points[1].x < points[0].x + diagonalSlope * (points[1].y - points[0].y).floor())
        leftPoints.push_back(points[1]);
    else
        rightPoints.push_back(points[1]);

    if (points[2].x < points[0].x + diagonalSlope * (points[2].y - points[0].y).floor())
        leftPoints.push_back(points[2]);
    else
        rightPoints.push_back(points[2]);

    leftPoints.push_back(points[3]);
    rightPoints.push_back(points[3]);

    Point3D cubeCenter(
        FixedPointNumber::ofInt(cube.pos.x) + FixedPointNumber::half(),
        FixedPointNumber::ofInt(cube.pos.y) + FixedPointNumber::half(),
        FixedPointNumber::ofInt(cube.pos.z) + FixedPointNumber::half()
    );
    FixedPointNumber zIndex = Point3D::distanceSq(cubeCenter, cameraPos);

    cube.cachedShapes[face]->modifyPoints(leftPoints, rightPoints, -zIndex);
}

Point2D Cubes::projectPoint(Point3DInt p) {
    //printf("project point %d,%d\n", p.x, p.y);
    FixedPointNumber x = FixedPointNumber::ofInt(p.x * 500);
    FixedPointNumber y = FixedPointNumber::ofInt(p.y * 500);
    x = x / p.z;
    y = y / p.z;
    return {
        .x = FixedPointNumber::ofInt(rasterizer->screenWidth()) / 2 + x,
        .y = FixedPointNumber::ofInt(rasterizer->screenHeight()) / 2 - y
    };
}
