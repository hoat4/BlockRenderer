#include "SpanRenderer.hpp"
#include <stdio.h>

#define TRACE_FILL_SPAN 0
#define TRACE_CHANGE_SPANS 0

SpanRenderer::SpanRenderer(unsigned int screenWidth) : screenWidth(screenWidth) {
    // Get a free channel, panic() if there are none
    dmaChannel = dma_claim_unused_channel(true);

    dmaChannelConfig = dma_channel_get_default_config(dmaChannel);
    channel_config_set_transfer_data_size(&dmaChannelConfig, DMA_SIZE_32);
    channel_config_set_read_increment(&dmaChannelConfig, false);
    channel_config_set_write_increment(&dmaChannelConfig, true);
}



SpanEdge* SpanRenderer::addEdge(
    FixedPointNumber topX, FixedPointNumber slope, uint16_t color) {

    SpanEdge* newEdge = new SpanEdge(color, topX, slope);

    SpanEdge** pp = &firstEdge;
    SpanEdge* p = *pp;
    while (p != NULL && isLeftTo(*p, *newEdge)) {
        pp = &p->activeEdges_next;
        p = *pp;
    }

    if (TRACE_CHANGE_SPANS) {
        printf("Add edge %p at %d.%d with slope %d.%d and color %x before %p\n", 
            newEdge,
            topX.floor(), topX.fractionalPart(), 
            slope.floor(), slope.fractionalPart(), 
            color, 
            p
        );
    }

    newEdge->activeEdges_pointerToNextOfPrev = pp;
    newEdge->activeEdges_next = p;
    if (p != NULL)
        p->activeEdges_pointerToNextOfPrev = &newEdge->activeEdges_next;
    *pp = newEdge;

    return newEdge;
}

bool SpanRenderer::isLeftTo(SpanEdge& a, SpanEdge& b) {
    if (a.x.floor() == b.x.floor())
        return a.slope < b.slope;
    else
        return a.x < b.x;
}

void SpanRenderer::removeEdge(SpanEdge* edge) {
    if (edge->activeEdges_next != NULL)
        edge->activeEdges_next->activeEdges_pointerToNextOfPrev = edge->activeEdges_pointerToNextOfPrev;
    *edge->activeEdges_pointerToNextOfPrev = edge->activeEdges_next;
    delete edge;
}

void SpanRenderer::clear() {
    SpanEdge* next;
    for (SpanEdge* e = firstEdge; e != NULL; e = next) {
        next = e->activeEdges_next;
        delete e;
    }
    firstEdge = NULL;
}

void SpanRenderer::renderLine(uint16_t* buffer) {
    FixedPointNumber prevX = FixedPointNumber::zero();
    for (SpanEdge* e = firstEdge; e != NULL; e = e -> activeEdges_next) {
        FixedPointNumber fromFP = prevX, toFP = e->x;

        if (fromFP >= toFP)
            if (fromFP.floor() == toFP.floor())
                // metszésekkor lehetséges ilyen. párhuzamosak esetén graphics.c kiszedi valamennyire.
                // egyszer láttam olyat hogy a pontos érték nem egyezett, csak az egészrész,
                // de slope akkor azt mutatta hogy széttartók voltak. 
                continue;
            else
                panic("invalid span (1)");
        if (fromFP < FixedPointNumber::zero())
            panic("invalid span (2)");
        if (toFP > FixedPointNumber::ofInt(screenWidth))
            panic("invalid span (3): %d.%d > %d (%p, %p, %p)", toFP.floor(), toFP.fractionalPart(), screenWidth, e, firstEdge, this);
        if (e->color == 0)
            panic("unsupported fill type");


        unsigned int from = fromFP.floor();
        unsigned int to = toFP.floor();

        unsigned short color = e->color;
    
        #if TRACE_FILL_SPAN    
        printf("%d-%d: %x\n", from, to, color);
        #endif
        
        if (to - from <= 10) {
            for (; from < to; from++)
                buffer[from] = color;
        } else {
            int* writeStart = (int*) ((int) (&buffer[from]) + 2 & ~3);
            int* writeEnd = (int*) ((int) (&buffer[to]) & ~3);

            int doubleColor = ((uint32_t) color)&0xFFFF | ((uint32_t)color) << 16;
    
            dma_channel_wait_for_finish_blocking(dmaChannel);
            dmaBuffer = doubleColor;
            dma_channel_configure(
                dmaChannel,
                &dmaChannelConfig,
                writeStart,   // The initial write address
                &dmaBuffer,     // The initial read address
                writeEnd - writeStart,      // Number of transfers
                true            // Start immediately.
            );
            
            buffer[from] = color;
            buffer[to - 1] = color;
        }

        prevX = e->x;

        e->x += e->slope;
    }

    dma_channel_wait_for_finish_blocking(dmaChannel);
}

void SpanRenderer::skipLines(unsigned int lines) {
    for (SpanEdge* e = firstEdge; e != NULL; e = e -> activeEdges_next) {
        e->x += e->slope * lines;
    }
}