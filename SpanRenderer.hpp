#pragma once

#include "FixedPointNumber.hpp"
#include "hardware/dma.h"

class SpanEdge {
    public:
        FixedPointNumber slope;

    private:
        friend class SpanRenderer;

        uint16_t color;
        FixedPointNumber topX;

        // menet közben van használva
        FixedPointNumber x;
        struct SpanEdge** activeEdges_pointerToNextOfPrev;
        struct SpanEdge* activeEdges_next;

        SpanEdge(uint16_t color, FixedPointNumber topX, FixedPointNumber slope) :
            color(color), topX(topX), slope(slope), 
            x(topX), activeEdges_pointerToNextOfPrev(NULL), activeEdges_next(NULL) {}
};

class SpanRenderer {

    private:

    SpanEdge* firstEdge = NULL;
    unsigned int screenWidth;

    int dmaChannel;
    dma_channel_config dmaChannelConfig;
    int dmaBuffer;

    public:

    SpanRenderer(unsigned int screenWidth);
    
    SpanEdge* addEdge(FixedPointNumber topX, FixedPointNumber slope, uint16_t color);

    void removeEdge(SpanEdge* edge);

    void clear();

    void renderLine(uint16_t* buffer);

    void skipLines(unsigned int lines);

    private:
    bool isLeftTo(SpanEdge& a, SpanEdge& b);
};
