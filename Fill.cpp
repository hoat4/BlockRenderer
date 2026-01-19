#include "Fill.hpp"

#include <iomanip>
#include "hardware/interp.h"
#include "hardware/dma.h"
#include "arm_acle.h"

int dmaChannel;
dma_channel_config dmaChannelConfig;
int dmaBuffer;

void ColorFill::initDMAChannel() {
    // Get a free channel, panic() if there are none
    dmaChannel = dma_claim_unused_channel(true);

    dmaChannelConfig = dma_channel_get_default_config(dmaChannel);
    channel_config_set_transfer_data_size(&dmaChannelConfig, DMA_SIZE_32);
    channel_config_set_read_increment(&dmaChannelConfig, false);
    channel_config_set_write_increment(&dmaChannelConfig, true);
}

void ColorFill::waitForDMAChannelToFinish() {
    dma_channel_wait_for_finish_blocking(dmaChannel);
}

void ColorFill::fillSpan(uint16_t* from, uint16_t* to, int x, int y) {
    unsigned int r = color >> 16 & 0xFF, g = color >> 8 & 0xFF, b = color & 0xFF;

    uint16_t rgb565 = ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | ((b & 0b11111000) >> 3);

    if ((intptr_t)from & 2)
        *(from++) = rgb565;
    if (((intptr_t)to) & ~2)
        *(to - 1) = rgb565;

    uint32_t colorDoubled = rgb565 | rgb565 << 16;
    uint32_t* start2 = (uint32_t*)from;
    uint32_t* end2 = (uint32_t*)to;

    dma_channel_wait_for_finish_blocking(dmaChannel);
    dmaBuffer = colorDoubled;
    dma_channel_configure(
        dmaChannel,
        &dmaChannelConfig,
        from,   // The initial write address
        &dmaBuffer,     // The initial read address
        end2 - ((uint32_t*)from),      // Number of transfers
        true            // Start immediately.
    );
}

std::string ColorFill::toString() {
    std::stringstream s;
    s << "#" << std::setfill('0') << std::setw(6) << std::hex << color;
    return s.str();
}

Color ColorFill::blendColorsDstOpaque(Color dst, Color src) {
    unsigned int aa = src >> 24;
    if (aa == 255) {
        return src;
    }

    unsigned int ra = src >> 16 & 0xFF, rb = dst >> 16 & 0xFF;
    unsigned int ga = src >> 8 & 0xFF, gb = dst >> 8 & 0xFF;
    unsigned int ba = src & 0xFF, bb = dst & 0xFF;

    unsigned int ro = ra * aa + rb * (255 - aa) + 127;
    unsigned int go = ga * aa + gb * (255 - aa) + 127;
    unsigned int bo = ba * aa + bb * (255 - aa) + 127;

    //printf("(%d, %d, %d, %d) over (%d, %d, %d, %d): (%d, %d, %d, %d)\n", 
    //    ra, ga, ba, aa, rb, gb, bb, ab, ro, go, bo, ao);

    return 0xFF000000 | (ro & 0xFF00) << 8 | (go & 0xFF00) | bo >> 8;
}

// először volt külön TEXTURE_STRIDE is mert __usat helyett padding volt, de __usat miatt muszáj kettő-hatványnak lennie a textúraméretnek
const int TEXTURE_SIZE_BITS = 4;
const int TEXTURE_SIZE = 1 << TEXTURE_SIZE_BITS;
const int MAX_TEXTURE_ROW_BITS = 5;
const int SCANLINE_SUBDIVISION_BITS = 3;
const int SCANLINE_SUBDIVISION = 1 << SCANLINE_SUBDIVISION_BITS;

// mintha kicsit gyorsabb lenne így megfordítva, de nem biztos
const int INTERPOLATOR_LANE_U = 1;
const int INTERPOLATOR_LANE_V = 0;

void TextureFill::initInterpolator() {
    interp_config cfg = interp_default_config();
    // set add_raw flag to use raw (un-shifted and un-masked) lane accumulator value when adding
    // it to the lane base to make the lane result
    interp_config_set_add_raw(&cfg, true);
    interp_config_set_shift(&cfg, UV_FRACTIONAL_BITS + SCANLINE_SUBDIVISION_BITS - 1);
    interp_config_set_mask(&cfg, 1, TEXTURE_SIZE_BITS - 1 + 1);
    interp_set_config(interp0, INTERPOLATOR_LANE_U, &cfg);

    interp_config_set_shift(&cfg, UV_FRACTIONAL_BITS + SCANLINE_SUBDIVISION_BITS - TEXTURE_SIZE_BITS - 1);
    interp_config_set_mask(&cfg, TEXTURE_SIZE_BITS + 1, TEXTURE_SIZE_BITS + MAX_TEXTURE_ROW_BITS - 1 + 1);
    interp_set_config(interp0, INTERPOLATOR_LANE_V, &cfg);
}

// valamiért gyorsabb, ha egy ilyen mutable változóba rakjuk bele ezt a konstanst
io_rw_32* INTERP0_BASE = interp0->base;

void __not_in_flash_func(TextureFill::fillSpan)(uint16_t* from, uint16_t* to,
                                                int x2, int y2) {
    float x = x2 - baseX;
    float y = y2 - baseY;

    float zReciprocalValue = zReciprocal.valueAt(x, y);

    float uPerZValue = uPerZ.valueAt(x, y);
    float vPerZValue = vPerZ.valueAt(x, y);

    float z = 1 / zReciprocalValue;
    int u = __usat((int)(uPerZValue * z), (UV_FRACTIONAL_BITS + TEXTURE_SIZE_BITS));
    int v = __usat((int)(vPerZValue * z), (UV_FRACTIONAL_BITS + TEXTURE_SIZE_BITS));

    // ez az if már felesleges, mert usat miatt nem lehetséges ilyen (ami nem feltétlen jó,
    // mert a tényleges se tudjuk így kiszűrni)
    /*
    if (u < 0 || u >= TEXTURE_SIZE << UV_FRACTIONAL_BITS || v < 0 || v >= TEXTURE_SIZE << UV_FRACTIONAL_BITS) {
        printf("u: %d, v: %d, original pos: (%d, %d), base: (%f, %f)\n", u, v, x2, y2, baseX, baseY);
        printf("1/z base: %f, value: %f\n", zReciprocal.base, zReciprocalValue);
        printf("v/z base: %f, value: %f\n", vPerZ.base, vPerZValue);
        throw "texture coordinates out of bounds (1)";
    }
    */

    
    interp0->base[2] = (uintptr_t)texture;
    interp0->accum[INTERPOLATOR_LANE_U] = u << SCANLINE_SUBDIVISION_BITS;
    interp0->accum[INTERPOLATOR_LANE_V] = v << SCANLINE_SUBDIVISION_BITS;

    zReciprocalValue += zReciprocal.by8X;
    float zNext = 1.0f / zReciprocalValue;

    do {
        // ld. "Scanline subdivision" itt: 
        // https://www.lysator.liu.se/~mikaelk/doc/perspectivetexture/

        uPerZValue += uPerZ.by8X;
        vPerZValue += vPerZ.by8X;

        int uNext = __usat((int)(uPerZValue * zNext), UV_FRACTIONAL_BITS + TEXTURE_SIZE_BITS);
        INTERP0_BASE[INTERPOLATOR_LANE_U] = uNext - u; // uIncrement
        u = uNext; // azért nem a lenti ifbe, hogy felhasználhassa az uNext regiszterét később másra

        int vNext = __usat((int)(vPerZValue * zNext), UV_FRACTIONAL_BITS + TEXTURE_SIZE_BITS);

        INTERP0_BASE[INTERPOLATOR_LANE_V] = vNext - v; // vIncrement


        //*(from++) = texture[(textureXFixedPoint >> UV_FRACTIONAL_BITS) +
        //        ((textureYFixedPoint >> UV_FRACTIONAL_BITS) << TEXTURE_SIZE_BITS)]

        if (to - from > 8) {
            v = vNext;
            zReciprocalValue += zReciprocal.by8X;
            zNext = 1.0f / zReciprocalValue; // a lenti memóriaírásokkal párhuzamosan menjen ez az osztás
        }

        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
        *(from++) = *(uint16_t*)interp0->pop[2];
    } while (from < to);
}

std::string TextureFill::toString() {
    std::stringstream s;
    s << "0x" << std::hex << texture;
    return s.str();
}

