#pragma once

#include <vector>
#include <stdint.h>
#include <pico/stdlib.h>

struct Pinout {
    uint8_t clk_p, rgb_p[3];
};

struct DVITiming {
    bool h_sync_polarity;
    int h_front_porch;
    int h_sync_width;
    int h_back_porch;
    int h_active_pixels;

    bool v_sync_polarity;
    int v_front_porch;
    int v_sync_width;
    int v_back_porch;
    int v_active_lines;

    unsigned int bit_clk_khz;
};

extern const struct DVITiming dvi_timing_640x480p_60hz;
extern const struct DVITiming dvi_timing_720x480p_60hz;
extern const struct DVITiming dvi_timing_720x576p_50hz;
extern const struct DVITiming dvi_timing_720x400p_70hz;
extern const struct DVITiming dvi_timing_800x450p_60hz;
extern const struct DVITiming dvi_timing_800x480p_60hz;
extern const struct DVITiming dvi_timing_800x600p_60hz;
extern const struct DVITiming dvi_timing_960x540p_60hz;
extern const struct DVITiming dvi_timing_960x540p_50hz;
extern const struct DVITiming dvi_timing_1024x768_rb_60hz;
extern const struct DVITiming dvi_timing_1280x720p_rb_50hz;
extern const struct DVITiming dvi_timing_1280x720p_rb_60hz;
extern const struct DVITiming dvi_timing_1920x1080p_rb2_30hz;
extern const struct DVITiming dvi_timing_1920x1080p_yolo_48hz;
extern const struct DVITiming dvi_timing_2560x1440p_yolo_24hz;

class DVIDisplay;

class LineBuffer {

    friend class DVIDisplay;

    private:
        LineBuffer();
        DVIDisplay* display;

    public:
        int lineNumber;
        uint16_t* buffer;
        void doneWriting();
};

class DVIDisplay {

    friend class LineBuffer;

public:
    /**
     * must be called before stdio_init_all
     */
    static void initSystemClocks();

private:
    static constexpr int TMDS_CTRL_00 = 0x354u;
    static constexpr int TMDS_CTRL_01 = 0x0abu;
    static constexpr int TMDS_CTRL_10 = 0x154u;
    static constexpr int TMDS_CTRL_11 = 0x2abu;

    static constexpr int TMDS_BALANCED_LOW = 0x307u;
    static constexpr int TMDS_BALANCED_HIGH = 0x2f0u;

    static constexpr int TMDS_BLACK_A = 0x100u;
    static constexpr int TMDS_BLACK_B = 0x1ffu;

    static constexpr int SYNC_V0_H0 = (TMDS_CTRL_00 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20));
    static constexpr int SYNC_V0_H1 = (TMDS_CTRL_01 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20));
    static constexpr int SYNC_V1_H0 = (TMDS_CTRL_10 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20));
    static constexpr int SYNC_V1_H1 = (TMDS_CTRL_11 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20));
    static constexpr int MISSING_PIXEL = (TMDS_BALANCED_LOW | (TMDS_BALANCED_LOW << 10) | (TMDS_BALANCED_HIGH << 20));
    static constexpr int BLACK_PIXEL = (TMDS_BALANCED_LOW | (TMDS_BALANCED_LOW << 10) | (TMDS_BALANCED_LOW << 20));
    static constexpr int BLACK_PIXEL_A = (TMDS_BLACK_A | (TMDS_BLACK_A << 10) | (TMDS_BLACK_A << 20));
    static constexpr int BLACK_PIXEL_B = (TMDS_BLACK_B | (TMDS_BLACK_B << 10) | (TMDS_BLACK_B << 20));

    static constexpr int HSTX_CMD_RAW = (0x0u << 12);
    static constexpr int HSTX_CMD_RAW_REPEAT = (0x1u << 12);
    static constexpr int HSTX_CMD_TMDS = (0x2u << 12);
    const int HSTX_CMD_TMDS_REPEAT = (0x3u << 12);
    const int HSTX_CMD_NOP = (0xfu << 12);

    // HSTX command lists

    // Lists are padded with NOPs to be >= HSTX FIFO size, to avoid DMA rapidly
    // pingponging and tripping up the IRQs.

    static constexpr uint32_t vblank_line_vsync_off_src[] = {
        HSTX_CMD_RAW_REPEAT,
        SYNC_V1_H1,
        HSTX_CMD_RAW_REPEAT,
        SYNC_V1_H0,
        HSTX_CMD_RAW_REPEAT,
        SYNC_V1_H1
    };
    static constexpr uint32_t vblank_line_vsync_on_src[] = {
        HSTX_CMD_RAW_REPEAT,
        SYNC_V0_H1,
        HSTX_CMD_RAW_REPEAT,
        SYNC_V0_H0,
        HSTX_CMD_RAW_REPEAT,
        SYNC_V0_H1
    };
    static constexpr uint32_t vactive_line_header_src[] = {
        HSTX_CMD_RAW_REPEAT,
        SYNC_V1_H1,
        HSTX_CMD_RAW_REPEAT,
        SYNC_V1_H0,
        HSTX_CMD_RAW_REPEAT,
        SYNC_V1_H1,
        HSTX_CMD_TMDS
    };

    const Pinout pinout;
    
    // kijelző méret: 640x480
    const int frame_width = 640;
    const int frame_height = 480;
    const int h_repeat_shift = 0;
    const DVITiming timingMode = dvi_timing_640x480p_60hz;

    // RGB565
    const int frame_bytes_per_pixel = 2;
    const int line_bytes_per_pixel = 2;

    const int NUM_CHANS = 3;

    uint32_t vblank_line_vsync_off[count_of(vblank_line_vsync_off_src)];
    uint32_t vblank_line_vsync_on[count_of(vblank_line_vsync_on_src)];
    uint32_t vactive_line_header[count_of(vactive_line_header_src)];

    /**
     * ennyivel szabad továbbírni a buffer vége után (a túlcsordult tartalom ignormálva lesz)
     */
    const unsigned int VACTIVE_LINE_END_PADDING_IN_WORDS = 4; 

    static const int LINE_BUFFER_COUNT = 10;

private: 
    int irqCounter = 0;
    uint32_t* line_buffers;

    // DMA scanline filling
    unsigned int ch_num = 0;
    int v_inactive_total;
    int v_total_active_lines;
    unsigned int frame_line_words_without_padding;
    unsigned int frame_line_words_with_padding;

    volatile int v_scanline = 2;
    volatile uint lineBuffers_write = 0;
    volatile uint lineBuffers_minWriteable = 0;

public:
    // uses DMA channels [0, NUM_CHANS-1] and DMA_IRQ_2
    DVIDisplay(const Pinout pinout);

    void handleInterrupt(); // should not be public

    LineBuffer nextLine();

    inline unsigned int width() {
        return frame_width;
    }

    inline unsigned int height() {
        return frame_height;
    }

private:
    void startDMA();
    void setupDisplayClock();
};
