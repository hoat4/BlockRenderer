#include "Cubes.hpp"
#include <algorithm>
#include <stdio.h>

Cubes::Cubes() : cameraPos(Point3D(FixedPointNumber::zero(), FixedPointNumber::zero(), FixedPointNumber::zero())) {
    
    faces.push_back({
            .cubePos = Point3DInt(1, 0, 5),
            .side = FACE_SIDE_SOUTH,
            .color = 0xFFFF0000,
            .updatedAtFrame = 0,
            .cachedShape = NULL
                    });

    faces.push_back({
            .cubePos = Point3DInt(1, 0, 5),
            .side = FACE_SIDE_WEST,
            .color = 0xFFFFFF00,
            .updatedAtFrame = 0,
            .cachedShape = NULL
                    });

    faces.push_back({
            .cubePos = Point3DInt(1, -2, 5),
            .side = FACE_SIDE_TOP,
            .color = 0xFF00FF00,
            .updatedAtFrame = 0,
            .cachedShape = NULL
                    });

    faces.push_back({
            .cubePos = Point3DInt(1, -2, 5),
            .side = FACE_SIDE_WEST,
            .color = 0xFFFFFF00,
            .updatedAtFrame = 0,
            .cachedShape = NULL
                    });
    faces.push_back({
            .cubePos = Point3DInt(1, -2, 5),
            .side = FACE_SIDE_SOUTH,
            .color = 0xFFFF0000,
            .updatedAtFrame = 0,
            .cachedShape = NULL
                    });

    frameID = 0;
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
            FixedPointNumber::ofInt(0),
            0xFF55EEFF
            );
        rasterizer->shapes.push_back(background);
    }

    frameID++;

    rasterizer->shapes.erase(rasterizer->shapes.begin() + 1, rasterizer->shapes.end());

    for (auto it = faces.begin(); it != faces.end(); ++it) {
        CubeFace& t = *it;
        t.updatedAtFrame = frameID;
        if (t.cachedShape == NULL)
            t.cachedShape = ConvexShape::create(4, 4, FixedPointNumber::ofInt(1), t.color);
        rasterizer->shapes.push_back(t.cachedShape);
        projectCubeFace(t);
    }

    for (auto it = faces.begin(); it != faces.end(); ++it) {
        CubeFace& t = *it;
        if (t.updatedAtFrame != frameID && t.cachedShape != NULL) {
            delete t.cachedShape;
            t.cachedShape = NULL;
        }
    }
}

Point3DInt CUBE_VERTEX_OFFSETS[] = {
    /* TOP */    Point3DInt(0, 1, 1), Point3DInt(1, 1, 1), Point3DInt(0, 1, 0), Point3DInt(1, 1, 0),
    /* BOTTOM */ Point3DInt(0, 0, 1), Point3DInt(1, 0, 1), Point3DInt(0, 0, 0), Point3DInt(1, 0, 0),
    /* NORTH */ Point3DInt(0, 1, 1), Point3DInt(1, 1, 1), Point3DInt(0, 0, 1), Point3DInt(1, 0, 1),
    /* WEST */ Point3DInt(0, 1, 0), Point3DInt(0, 1, 1), Point3DInt(0, 0, 0), Point3DInt(0, 0, 1),
    /* SOUTH */ Point3DInt(0, 1, 0), Point3DInt(1, 1, 0), Point3DInt(0, 0, 0), Point3DInt(1, 0, 0),
    /* EAST */ Point3DInt(1, 1, 1), Point3DInt(1, 1, 0), Point3DInt(1, 0, 1), Point3DInt(1, 0, 0),
};

void Cubes::projectCubeFace(CubeFace& face) {
    // kamera pozíció és forgás alapján eliminálni lehetne a sortot

    std::vector<Point2D> points;

    points.push_back(projectPoint(face.cubePos + CUBE_VERTEX_OFFSETS[face.side * 4]));
    points.push_back(projectPoint(face.cubePos + CUBE_VERTEX_OFFSETS[face.side * 4 + 1]));
    points.push_back(projectPoint(face.cubePos + CUBE_VERTEX_OFFSETS[face.side * 4 + 2]));
    points.push_back(projectPoint(face.cubePos + CUBE_VERTEX_OFFSETS[face.side * 4 + 3]));

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

    face.cachedShape->modifyPoints(leftPoints, rightPoints);
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
