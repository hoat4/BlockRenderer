#include "Rasterizer.hpp"
#include <stdio.h>

Rasterizer::Rasterizer(int width, int height) :
    width(width), height(height), spanRenderer(SpanRenderer(width)) {

    quadListsByLine.resize(height);
}


void Rasterizer::beginFrame() {
    spanRenderer.clear();

    VertexPointer emptyVertexPointer = { quadIndex: -1, vertex : 0 };
    std::fill(quadListsByLine.begin(), quadListsByLine.end(), emptyVertexPointer);

    for (int quadIndex = 0; quadIndex < quads.size(); quadIndex++) {
        Quad2D& q = quads[quadIndex];
        for (int i = 0; i < Quad2D::VERTICES; i++) {
            q.nexts[i].quadIndex = -1;
        }

        for (int i = 0; i < Quad2D::VERTICES; i++) {
            pushToQuadList(q, quadIndex, i);
        }
    }

    y = 0;
}

void Rasterizer::pushToQuadList(Quad2D& q, int quadIndex, int vertex) {
    Point2D p = q.points[vertex];

    int line = p.y.floor();
    if (line >= height)
        return;

    VertexPointer* list = &quadListsByLine[line];
    while (list->quadIndex != -1 &&
           p.x >= quads[list->quadIndex].points[list->vertex].x) {

        list = &quads[list->quadIndex].nexts[list->vertex];
    }

    q.nexts[vertex] = *list;
    list->quadIndex = quadIndex;
    list->vertex = vertex;
}

void Rasterizer::skipLines(int lines) {
    spanRenderer.skipLines(lines);
    for (int i = 0; i < lines; i++) {
        processCurrentLine();
        y++;
    }
}

void Rasterizer::renderLine(uint16_t* buffer) {
    processCurrentLine();
    spanRenderer.renderLine(buffer);
    y++;
}

void Rasterizer::processCurrentLine() {
    for (VertexPointer list = quadListsByLine[y];
         list.quadIndex != -1;
         list = quads[list.quadIndex].nexts[list.vertex]) {

        Quad2D& q = quads[list.quadIndex];
        switch (list.vertex) {
        case 0: // felső pont
            q.rightEdge = spanRenderer.addEdge(
                q.points[0].x,
                (q.points[1].x - q.points[0].x) / (q.points[1].y - q.points[0].y).floor(),
                q.color
            );
            break;

        case 1: // jobb oldali pont
        {
            int yDistance = (q.points[2].y - q.points[1].y).floor();
            if (yDistance != 0) // ha 0, akkor úgyis mindegy, hogy mennyi a slope
                q.rightEdge->slope = (q.points[2].x - q.points[1].x) / yDistance;
        }
        break;

        case 2: // alsó pont
            //printf("remove %x at %d\n", q.color, y);
            spanRenderer.removeEdge(q.rightEdge);
            break;
        }
    }
}