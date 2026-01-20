#include "DVIDisplay.hpp"

#include <pico/stdlib.h>

#include <stdio.h>
#include "pico/stdlib.h"
#include <cstdlib>
#include "string.h"

#include "hardware/dma.h"
#include "hardware/vreg.h"
#include "hardware/pll.h"
#include "hardware/resets.h"
#include "hardware/clocks.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"

extern "C"
{
#include <pico/lock_core.h>
}


void DVIDisplay::initSystemClocks() {
    printf("pi1\n");
    uint32_t intr_stash = save_and_disable_interrupts();

    // Before messing with clock speeds ensure QSPI clock is nice and slow
    hw_write_masked(&qmi_hw->m[0].timing, 6, QMI_M0_TIMING_CLKDIV_BITS);

    // We're going to go fast, boost the voltage a little
    vreg_set_voltage(VREG_VOLTAGE_1_15);

    // Force a read through XIP to ensure the timingMode is applied before raising the clock rate
    volatile uint32_t* ptr = (volatile uint32_t*)0x14000000;
    uint32_t val1 = *ptr;

    // Before we touch PLLs, switch sys and ref cleanly away from their aux sources.
    hw_clear_bits(&clocks_hw->clk[clk_sys].ctrl, CLOCKS_CLK_SYS_CTRL_SRC_BITS);
    while (clocks_hw->clk[clk_sys].selected != 0x1)
        tight_loop_contents();
    hw_write_masked(&clocks_hw->clk[clk_ref].ctrl, CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC, CLOCKS_CLK_REF_CTRL_SRC_BITS);
    while (clocks_hw->clk[clk_ref].selected != 0x4)
        tight_loop_contents();

    // Stop the other clocks so we don't worry about overspeed
    clock_stop(clk_usb);
    clock_stop(clk_adc);
    clock_stop(clk_peri);
    clock_stop(clk_hstx);

    // Set USB PLL to 528MHz
    pll_init(pll_usb, PLL_COMMON_REFDIV, 1584 * MHZ, 3, 1);

    const uint32_t usb_pll_freq = 528 * MHZ;

    // CLK SYS = PLL USB 528MHz / 2 = 264MHz
    if (!clock_configure(clk_sys,
                         CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                         CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                         usb_pll_freq, usb_pll_freq / 2)) {
        panic("clock_configure 1 failed");
    }

    // CLK PERI = PLL USB 528MHz / 4 = 132MHz
    if (!clock_configure(clk_peri,
                         0, // Only AUX mux on ADC
                         CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                         usb_pll_freq, usb_pll_freq / 4)) {
        panic("clock_configure 2 failed");
    }

    // CLK USB = PLL USB 528MHz / 11 = 48MHz
    if (!clock_configure(clk_usb,
                         0, // No GLMUX
                         CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                         usb_pll_freq,
                         USB_CLK_KHZ * KHZ)) {

        panic("clock_configure 3 failed");
    }

    // CLK ADC = PLL USB 528MHz / 11 = 48MHz
    if (!clock_configure(clk_adc,
                         0, // No GLMUX
                         CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                         usb_pll_freq,
                         USB_CLK_KHZ * KHZ)) {
        panic("clock_configure 4 failed");
    }

    uint32_t val2 = *ptr;

    // Now we are running fast set fast QSPI clock and read delay
    // ez nem működik, gondolom túl gyors a flashnak.
    if (0) {
        // TODO ki kéne deríteni, hogy náluk miért működött (és miért vették lejjebb
        //      a divisort és a read delayt a defaulthoz képest)

        // Make sure flash is deselected - QMI doesn't appear to have a busy flag(!)
        while ((ioqspi_hw->io[1].status & IO_QSPI_GPIO_QSPI_SS_STATUS_OUTTOPAD_BITS) != IO_QSPI_GPIO_QSPI_SS_STATUS_OUTTOPAD_BITS)
            ;

        qmi_hw->m[0].timing = 0x40000202; // defaultban 60007206, nem tudom miért akarják lejjebb venni
        // qmi_hw->m[0].timingMode = 0x40000101;
        //  Force a read through XIP to ensure the timingMode is applied
        (void)*ptr;
    }

    //set_qmi_timing();

    restore_interrupts(intr_stash);

    uint32_t val3 = *ptr;

    printf("pi10 %d, %d, %d\n", val1, val2, val3);

    //printf("QMI timing %x\n", qmi_hw->m[0].timing);
}

DVIDisplay* globalDVIDisplay;

void __scratch_x("display") dma_irq_handler() {
    globalDVIDisplay->handleInterrupt();
}
    
DVIDisplay::DVIDisplay(const Pinout pinout) : pinout(pinout) {
    printf("setup display clock\n");
    setupDisplayClock();
    printf("setup display clock done\n");

    v_inactive_total = timingMode.v_front_porch + timingMode.v_sync_width + timingMode.v_back_porch;
    v_total_active_lines = v_inactive_total + timingMode.v_active_lines;
    int h_repeat = 1 << h_repeat_shift;

    memcpy(vblank_line_vsync_off, vblank_line_vsync_off_src, sizeof(vblank_line_vsync_off_src));
    vblank_line_vsync_off[0] |= timingMode.h_front_porch;
    vblank_line_vsync_off[2] |= timingMode.h_sync_width;
    vblank_line_vsync_off[4] |= timingMode.h_back_porch + timingMode.h_active_pixels;

    memcpy(vblank_line_vsync_on, vblank_line_vsync_on_src, sizeof(vblank_line_vsync_on_src));
    vblank_line_vsync_on[0] |= timingMode.h_front_porch;
    vblank_line_vsync_on[2] |= timingMode.h_sync_width;
    vblank_line_vsync_on[4] |= timingMode.h_back_porch + timingMode.h_active_pixels;

    memcpy(vactive_line_header, vactive_line_header_src, sizeof(vactive_line_header_src));
    vactive_line_header[0] |= timingMode.h_front_porch;
    vactive_line_header[2] |= timingMode.h_sync_width;
    vactive_line_header[4] |= timingMode.h_back_porch;
    vactive_line_header[6] |= timingMode.h_active_pixels;

    const int frame_pixel_words = (frame_width * h_repeat * line_bytes_per_pixel + 3) >> 2;
    frame_line_words_without_padding = frame_pixel_words + count_of(vactive_line_header);
    frame_line_words_with_padding = frame_line_words_without_padding + sizeof(int) * VACTIVE_LINE_END_PADDING_IN_WORDS;
    line_buffers = (uint32_t*)malloc(frame_line_words_with_padding * 4 * LINE_BUFFER_COUNT);
    
    for (int i = 0; i < LINE_BUFFER_COUNT; ++i)
    {
        memcpy(&line_buffers[i * frame_line_words_with_padding], vactive_line_header, count_of(vactive_line_header) * sizeof(uint32_t));
    }

    
    for (int i = 0; i < NUM_CHANS; ++i) {
        dma_channel_claim(i);
    }
    globalDVIDisplay = this;

    printf("DMA channels claimed\n");

    startDMA();

    printf("Elindult\n");
}

void DVIDisplay::startDMA() {
    irq_set_exclusive_handler(DMA_IRQ_2, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_2, true);

    printf("reset HSTX\n");
    reset_block_num(RESET_HSTX);
    sleep_us(10);
    unreset_block_num_wait_blocking(RESET_HSTX);
    sleep_us(10);
    
    // Configure HSTX's TMDS encoder for RGB565
    hstx_ctrl_hw->expand_tmds =
        4  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
        8 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   |
        5  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
        3  << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   |
        4  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
        29 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;

    // Pixels (TMDS) come in 2 16-bit chunks. Control symbols (RAW) are an
    // entire 32-bit word.
    hstx_ctrl_hw->expand_shift =
        2 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
        16 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
        1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
        0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

    // Serial output config: clock period of 5 cycles, pop from command
    // expander every 5 cycles, shift the output shiftreg by 2 every cycle.
    hstx_ctrl_hw->csr = 0;
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        5u << HSTX_CTRL_CSR_CLKDIV_LSB |
        5u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
        2u << HSTX_CTRL_CSR_SHIFT_LSB |
        HSTX_CTRL_CSR_EN_BITS; 

    // HSTX outputs 0 through 7 appear on GPIO 12 through 19.
    constexpr int HSTX_FIRST_PIN = 12;

    // Assign clock pair to two neighbouring pins:
    {
    int bit = pinout.clk_p - HSTX_FIRST_PIN;
    hstx_ctrl_hw->bit[bit    ] = HSTX_CTRL_BIT0_CLK_BITS;
    hstx_ctrl_hw->bit[bit ^ 1] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;
    }

    for (uint lane = 0; lane < 3; ++lane) {
        // For each TMDS lane, assign it to the correct GPIO pair based on the
        // desired pinout:
        int bit = pinout.rgb_p[lane] - HSTX_FIRST_PIN;
        // Output even bits during first half of each HSTX cycle, and odd bits
        // during second half. The shifter advances by two bits each cycle.
        uint32_t lane_data_sel_bits =
            (lane * 10    ) << HSTX_CTRL_BIT0_SEL_P_LSB |
            (lane * 10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB;
        // The two halves of each pair get identical data, but one pin is inverted.
        hstx_ctrl_hw->bit[bit    ] = lane_data_sel_bits;
        hstx_ctrl_hw->bit[bit ^ 1] = lane_data_sel_bits | HSTX_CTRL_BIT0_INV_BITS;
    }

    for (int i = 12; i <= 19; ++i) {
        gpio_set_function(i, GPIO_FUNC_HSTX);
        gpio_set_drive_strength(i, GPIO_DRIVE_STRENGTH_4MA);
        if (timingMode.bit_clk_khz > 900000) {
            gpio_set_slew_rate(i, GPIO_SLEW_RATE_FAST);
        }
    }

    printf("GPIO configured\n");

    // The channels are set up identically, to transfer a whole scanline and
    // then chain to the next channel. Each time a channel finishes, we
    // reconfigure the one that just finished, meanwhile the other channel(s)
    // are already making progress.
    // Using just 2 channels was insufficient to avoid issues with the IRQ.
    for (int i = 0; i < NUM_CHANS; ++i) {
        dma_channel_config c = dma_channel_get_default_config(i);
        channel_config_set_chain_to(&c, (i+1) % NUM_CHANS);
        channel_config_set_dreq(&c, DREQ_HSTX);
        dma_channel_configure(
            i,
            &c,
            &hstx_fifo_hw->fifo,
            vblank_line_vsync_off,
            count_of(vblank_line_vsync_off),
            false
        );
    }

    dma_hw->intr = (1 << NUM_CHANS) - 1;
    dma_hw->ints2 = (1 << NUM_CHANS) - 1;
    dma_hw->inte2 = (1 << NUM_CHANS) - 1;

    dma_channel_start(0);
}

void DVIDisplay::setupDisplayClock() {
    const uint32_t dvi_clock_khz = timingMode.bit_clk_khz >> 1;
    uint vco_freq, post_div1, post_div2;
    if (!check_sys_clock_khz(dvi_clock_khz, &vco_freq, &post_div1, &post_div2))
        panic("System clock of %u kHz cannot be exactly achieved", dvi_clock_khz);
    const uint32_t freq = vco_freq / (post_div1 * post_div2);

    if (timingMode.bit_clk_khz > 600000) {
        vreg_set_voltage(VREG_VOLTAGE_1_25);
    } else if (timingMode.bit_clk_khz > 800000) {
        // YOLO mode
        hw_set_bits(&powman_hw->vreg_ctrl, POWMAN_PASSWORD_BITS | POWMAN_VREG_CTRL_DISABLE_VOLTAGE_LIMIT_BITS);
        vreg_set_voltage(VREG_VOLTAGE_1_40);
        if (timingMode.bit_clk_khz > 900000) {
            vreg_set_voltage(VREG_VOLTAGE_1_50);
        }
        sleep_ms(1);
    }

    // Set the sys PLL to the requested freq
    pll_init(pll_sys, PLL_COMMON_REFDIV, vco_freq, post_div1, post_div2);

    // CLK HSTX = Requested freq
    clock_configure(clk_hstx,
                    0,
                    CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                    freq, freq);
}

void DVIDisplay::handleInterrupt() {
    // ch_num indicates the channel that just finished, which is the one
    // we're about to reload.
    dma_channel_hw_t *ch = &dma_hw->ch[ch_num];
    dma_hw->intr = 1u << ch_num;
    if (++ch_num == NUM_CHANS) ch_num = 0;

    if (v_scanline >= timingMode.v_front_porch && v_scanline < (timingMode.v_front_porch + timingMode.v_sync_width)) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_on;
        ch->transfer_count = count_of(vblank_line_vsync_on);
    } else if (v_scanline < v_inactive_total) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_off;
        ch->transfer_count = count_of(vblank_line_vsync_off);
    } else {
        const int y = v_scanline - v_inactive_total;
        const int new_line_num = y % LINE_BUFFER_COUNT;

        ch->read_addr = (uintptr_t)&line_buffers[new_line_num * frame_line_words_with_padding];
        ch->transfer_count = frame_line_words_without_padding;

        lineBuffers_minWriteable = (y + 1) % frame_height;
    }

    if (++v_scanline == v_total_active_lines) {
        v_scanline = 0;
    }

    irqCounter++;
}

LineBuffer DVIDisplay::nextLine() {
    int toBeWritten;

    int waitCount = 0;
    // legyen elég front/back porchhoz
    const int STALL_LIMIT = 20000; // 1000

    for (; waitCount<STALL_LIMIT; waitCount++) {
        toBeWritten = lineBuffers_write;

        int minWriteable = lineBuffers_minWriteable;
        int maxWriteable = (minWriteable + LINE_BUFFER_COUNT - 2) % frame_height;
        if (minWriteable < maxWriteable) {
            if (toBeWritten >= minWriteable && toBeWritten <= maxWriteable)
                break;
        } else {
            if (toBeWritten >= minWriteable || toBeWritten <= maxWriteable)
                break;
        }
        sleep_us(5);
    }

    if (waitCount == STALL_LIMIT) {
        // debugban resume után van ez
        printf("DVI output stalled (irq count: %d, ch0: %d, ch1: %d, ch2: %d, ch0: %d, ch1: %d, ch2: %d, FIFO stat: %u, INTR: %d)\n", 
            irqCounter, 
            dma_channel_is_busy(0), dma_channel_is_busy(1), dma_channel_is_busy(2), 
            dma_channel_is_busy(0), dma_channel_is_busy(1), dma_channel_is_busy(2), 
            hstx_fifo_hw->stat, 
            dma_hw->intr
        );
        printf("DMA read addr: %p, write: %p, ctrl: %p\n", 
            dma_hw->ch[0].read_addr, 
            dma_hw->ch[0].write_addr,
            dma_hw->ch[0].al1_ctrl
        );
        printf("DMA transfer count: %d, %d, %d\n", dma_hw->ch[0].transfer_count, dma_hw->ch[1].transfer_count, dma_hw->ch[2].transfer_count);
        
        dma_hw->ch[0].al1_ctrl = 1u<<30; // read error ack
        dma_hw->ch[1].al1_ctrl = 1u<<30; // read error ack
        dma_hw->ch[2].al1_ctrl = 1u<<30; // read error ack
        toBeWritten = 0;
        v_scanline = 2;
        lineBuffers_write = 0;
        lineBuffers_minWriteable = 0;
        ch_num = 0;
        startDMA();
            
        printf("DMA read addr: %p, write: %p, ctrl: %p\n", 
            dma_hw->ch[0].read_addr, 
            dma_hw->ch[0].write_addr,
            dma_hw->ch[0].al1_ctrl
        );
    }

    LineBuffer l;
    l.display = this;
    l.lineNumber = toBeWritten;
    l.buffer = (uint16_t*) &line_buffers[
        (l.lineNumber % LINE_BUFFER_COUNT) * frame_line_words_with_padding + count_of(vactive_line_header)];
    return l;
}

LineBuffer::LineBuffer() {}

void LineBuffer::doneWriting() {
    display->lineBuffers_write = (display->lineBuffers_write+1) % display->frame_height;
}


// -------------- VIDEO MODES --------------

// VGA -- we do this mode properly, with a pretty comfortable clk_sys (252 MHz)
const struct DVITiming dvi_timing_640x480p_60hz = {
    .h_sync_polarity = false,
    .h_front_porch = 16,
    .h_sync_width = 96,
    .h_back_porch = 48,
    .h_active_pixels = 640,

    .v_sync_polarity = false,
    .v_front_porch = 10,
    .v_sync_width = 2,
    .v_back_porch = 33,
    .v_active_lines = 480,

    .bit_clk_khz = 252000
};

// SVGA -- completely by-the-book but requires 400 MHz clk_sys
const struct DVITiming dvi_timing_800x600p_60hz = {
    .h_sync_polarity = false,
    .h_front_porch = 44,
    .h_sync_width = 128,
    .h_back_porch = 88,
    .h_active_pixels = 800,

    .v_sync_polarity = false,
    .v_front_porch = 1,
    .v_sync_width = 4,
    .v_back_porch = 23,
    .v_active_lines = 600,

    .bit_clk_khz = 400000
};

// 720x480 - timings from dumping the EDID of my monitor
const struct DVITiming dvi_timing_720x480p_60hz = {
    .h_sync_polarity = false,
    .h_front_porch = 16,
    .h_sync_width = 62,
    .h_back_porch = 60,
    .h_active_pixels = 720,

    .v_sync_polarity = false,
    .v_front_porch = 9,
    .v_sync_width = 6,
    .v_back_porch = 30,
    .v_active_lines = 480,

    .bit_clk_khz = 270000
};

// 720x576@50Hz - CEA timingMode
const struct DVITiming dvi_timing_720x576p_50hz = {
    .h_sync_polarity = false,
    .h_front_porch = 12,
    .h_sync_width = 64,
    .h_back_porch = 68,
    .h_active_pixels = 720,

    .v_sync_polarity = false,
    .v_front_porch = 5,
    .v_sync_width = 5,
    .v_back_porch = 39,
    .v_active_lines = 576,

    .bit_clk_khz = 270000
};

// 720x400@70Hz - "IBM" timingMode
const struct DVITiming dvi_timing_720x400p_70hz = {
    .h_sync_polarity = false,
    .h_front_porch = 18,
    .h_sync_width = 108,
    .h_back_porch = 54,
    .h_active_pixels = 720,

    .v_sync_polarity = true,
    .v_front_porch = 13,
    .v_sync_width = 2,
    .v_back_porch = 34,
    .v_active_lines = 400,

    .bit_clk_khz = 283200
};

// 800x480p 60 Hz (note this doesn't seem to be a CEA mode, I just used the
// output of `cvt 800 480 60`), 295 MHz bit clock
const struct DVITiming dvi_timing_800x480p_60hz = {
    .h_sync_polarity = false,
    .h_front_porch = 24,
    .h_sync_width = 72,
    .h_back_porch = 96,
    .h_active_pixels = 800,

    .v_sync_polarity = true,
    .v_front_porch = 3,
    .v_sync_width = 10,
    .v_back_porch = 7,
    .v_active_lines = 480,

    .bit_clk_khz = 295200
};

// 800x450p 60 Hz Similarly not a CEA mode, but is 16:9
const struct DVITiming dvi_timing_800x450p_60hz = {
    .h_sync_polarity = false,
    .h_front_porch = 24,
    .h_sync_width = 72,
    .h_back_porch = 96,
    .h_active_pixels = 800,

    .v_sync_polarity = true,
    .v_front_porch = 3,
    .v_sync_width = 5,
    .v_back_porch = 10,
    .v_active_lines = 450,

    .bit_clk_khz = 278400
};

// SVGA reduced blanking (355 MHz bit clock) -- valid CVT mode, less common
// than fully-blanked SVGA, but doesn't require such a high system clock
const struct DVITiming dvi_timing_800x600p_reduced_60hz = {
    .h_sync_polarity = true,
    .h_front_porch = 48,
    .h_sync_width = 32,
    .h_back_porch = 80,
    .h_active_pixels = 800,

    .v_sync_polarity = false,
    .v_front_porch = 3,
    .v_sync_width = 4,
    .v_back_porch = 11,
    .v_active_lines = 600,

    .bit_clk_khz = 354000
};

// Also known as qHD, bit uncommon, but it's a nice modest-resolution 16:9
// aspect mode. Pixel clock 40.75 MHz for full CVT mode (no reduced blanking)
const struct DVITiming dvi_timing_960x540p_60hz = {
    .h_sync_polarity = false,
    .h_front_porch = 32,
    .h_sync_width = 96,
    .h_back_porch = 128,
    .h_active_pixels = 960,

    .v_sync_polarity = true,
    .v_front_porch = 3,
    .v_sync_width = 5,
    .v_back_porch = 14,
    .v_active_lines = 540,

    .bit_clk_khz = 408000
};

// Also known as qHD, bit uncommon, but it's a nice modest-resolution 16:9
// aspect mode. Pixel clock 33.5 MHz for 50Hz CVT mode (no reduced blanking)
const struct DVITiming dvi_timing_960x540p_50hz = {
    .h_sync_polarity = false,
    .h_front_porch = 24,
    .h_sync_width = 96,
    .h_back_porch = 120,
    .h_active_pixels = 960,

    .v_sync_polarity = true,
    .v_front_porch = 3,
    .v_sync_width = 5,
    .v_back_porch = 11,
    .v_active_lines = 540,

    .bit_clk_khz = 336000
};

// 1024x768, CVT-RB
const struct DVITiming dvi_timing_1024x768_rb_60hz = {
    .h_sync_polarity = true,
    .h_front_porch = 48,
    .h_sync_width = 32,
    .h_back_porch = 80,
    .h_active_pixels = 1024,

    .v_sync_polarity = false,
    .v_front_porch = 3,
    .v_sync_width = 4,
    .v_back_porch = 15,
    .v_active_lines = 768,

    .bit_clk_khz = 560000
};

// 720p50, this is a standard HD mode, the CVT-RB variant
// should be widely supported
const struct DVITiming dvi_timing_1280x720p_rb_50hz = {
    .h_sync_polarity = true,
    .h_front_porch = 48,
    .h_sync_width = 32,
    .h_back_porch = 80,
    .h_active_pixels = 1280,

    .v_sync_polarity = false,
    .v_front_porch = 3,
    .v_sync_width = 5,
    .v_back_porch = 9,
    .v_active_lines = 720,

    .bit_clk_khz = 528000
};

// 720p60, this is the CVT-RB variant, again should be widely supported
const struct DVITiming dvi_timing_1280x720p_rb_60hz = {
    .h_sync_polarity = true,
    .h_front_porch = 48,
    .h_sync_width = 32,
    .h_back_porch = 80,
    .h_active_pixels = 1280,

    .v_sync_polarity = false,
    .v_front_porch = 3,
    .v_sync_width = 5,
    .v_back_porch = 13,
    .v_active_lines = 720,

    .bit_clk_khz = 640000
};

// 1080p30 - not a normal mode but seems to work on a wide variety of hardware
// Strictly speaking RB2 should have a clock speed matching the target frequency more closely
// but it seems to work!
const struct DVITiming dvi_timing_1920x1080p_rb2_30hz = {
    .h_sync_polarity = true,
    .h_front_porch = 8,
    .h_sync_width = 32,
    .h_back_porch = 40,
    .h_active_pixels = 1920,

    .v_sync_polarity = false,
    .v_front_porch = 7,
    .v_sync_width = 8,
    .v_back_porch = 6,
    .v_active_lines = 1080,

    .bit_clk_khz = 660000
};

// 1080p48 YOLO - may require more than 1.3V, and sorting through your 
// devboards to find one that gives a clean signal.
const struct DVITiming dvi_timing_1920x1080p_yolo_48hz = {
        .h_sync_polarity = true,
        .h_front_porch = 8,
        .h_sync_width = 16,
        .h_back_porch = 24,
        .h_active_pixels = 1920,

        .v_sync_polarity = false,
        .v_front_porch = 4,
        .v_sync_width = 2,
        .v_back_porch = 6,
        .v_active_lines = 1080,

        .bit_clk_khz = 1032000
};

// 1440p24 YOLO - works on my Dell Ultrasharp, that most forgiving of monitors.  May require a little more than 1.3V
const struct DVITiming dvi_timing_2560x1440p_yolo_24hz = {
    .h_sync_polarity = true,
    .h_front_porch = 8,
    .h_sync_width = 32,
    .h_back_porch = 20,
    .h_active_pixels = 2560,

    .v_sync_polarity = false,
    .v_front_porch = 2,
    .v_sync_width = 6,
    .v_back_porch = 2,
    .v_active_lines = 1440,

    .bit_clk_khz = 912000
};
