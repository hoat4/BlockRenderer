#include <stdio.h>
#include "pico/stdlib.h"
#include <cstdlib>
#include "string.h"

#include "hardware/dma.h"
#include "hardware/vreg.h"
#include "hardware/pll.h"
#include "hardware/clocks.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "dvi.hpp"

// DMA scanline filling
uint ch_num = 0;
int line_num = -1;
int v_inactive_total;
int v_total_active_lines;
volatile int v_scanline = 2;
//volatile bool flip_next;

// kijelző méret: 640x360
const int frame_width = 640;
const int frame_height = 480;
const int h_repeat_shift = 0;
const int v_repeat_shift = 0;
const dvi_timing* timing_mode = &dvi_timing_640x480p_60hz;

// RGB565
const int frame_bytes_per_pixel = 2;
const int line_bytes_per_pixel = 2;

uint32_t* line_buffers;

extern "C"
{
#include <pico/lock_core.h>
}

void vacak() {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
}

extern "C" void __no_inline_not_in_flash_func(setupClockPreinit)() {
    printf("pi1\n");
    uint32_t intr_stash = save_and_disable_interrupts();

    // Before messing with clock speeds ensure QSPI clock is nice and slow
    hw_write_masked(&qmi_hw->m[0].timing, 6, QMI_M0_TIMING_CLKDIV_BITS);

    // We're going to go fast, boost the voltage a little
    vreg_set_voltage(VREG_VOLTAGE_1_15);

    // Force a read through XIP to ensure the timing is applied before raising the clock rate
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
        // qmi_hw->m[0].timing = 0x40000101;
        //  Force a read through XIP to ensure the timing is applied
        (void)*ptr;
    }

    //set_qmi_timing();

    restore_interrupts(intr_stash);

    uint32_t val3 = *ptr;

    printf("pi10 %d, %d, %d\n", val1, val2, val3);
}

void setupDisplayClock(const dvi_timing* timing_mode) {
    const uint32_t dvi_clock_khz = timing_mode->bit_clk_khz >> 1;
    uint vco_freq, post_div1, post_div2;
    if (!check_sys_clock_khz(dvi_clock_khz, &vco_freq, &post_div1, &post_div2))
        panic("System clock of %u kHz cannot be exactly achieved", dvi_clock_khz);
    const uint32_t freq = vco_freq / (post_div1 * post_div2);

    if (timing_mode->bit_clk_khz > 600000) {
        vreg_set_voltage(VREG_VOLTAGE_1_25);
    } else if (timing_mode->bit_clk_khz > 800000) {
        // YOLO mode
        hw_set_bits(&powman_hw->vreg_ctrl, POWMAN_PASSWORD_BITS | POWMAN_VREG_CTRL_DISABLE_VOLTAGE_LIMIT_BITS);
        vreg_set_voltage(VREG_VOLTAGE_1_40);
        if (timing_mode->bit_clk_khz > 900000) {
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


// ----------------------------------------------------------------------------
// HSTX command lists

// Lists are padded with NOPs to be >= HSTX FIFO size, to avoid DMA rapidly
// pingponging and tripping up the IRQs.

static const uint32_t vblank_line_vsync_off_src[] = {
    HSTX_CMD_RAW_REPEAT,
    SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT,
    SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT,
    SYNC_V1_H1
};
static uint32_t vblank_line_vsync_off[count_of(vblank_line_vsync_off_src)];

static const uint32_t vblank_line_vsync_on_src[] = {
    HSTX_CMD_RAW_REPEAT,
    SYNC_V0_H1,
    HSTX_CMD_RAW_REPEAT,
    SYNC_V0_H0,
    HSTX_CMD_RAW_REPEAT,
    SYNC_V0_H1
};
static uint32_t vblank_line_vsync_on[count_of(vblank_line_vsync_on_src)];

static const uint32_t vactive_line_header_src[] = {
    HSTX_CMD_RAW_REPEAT,
    SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT,
    SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT,
    SYNC_V1_H1,
    HSTX_CMD_TMDS      
};
static uint32_t vactive_line_header[count_of(vactive_line_header_src)];

static const int NUM_FRAME_LINES = 2;
static const int NUM_CHANS = 3;

    struct Pinout {
        uint8_t clk_p, rgb_p[3];
    };

static int irqCounter;

void __scratch_x("display") dma_irq_handler() {
    
    // ch_num indicates the channel that just finished, which is the one
    // we're about to reload.
    dma_channel_hw_t *ch = &dma_hw->ch[ch_num];
    dma_hw->intr = 1u << ch_num;
    if (++ch_num == NUM_CHANS) ch_num = 0;

    if (v_scanline >= timing_mode->v_front_porch && v_scanline < (timing_mode->v_front_porch + timing_mode->v_sync_width)) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_on;
        ch->transfer_count = count_of(vblank_line_vsync_on);
    } else if (v_scanline < v_inactive_total) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_off;
        ch->transfer_count = count_of(vblank_line_vsync_off);
    } else {
        const int y = (v_scanline - v_inactive_total) >> v_repeat_shift;
        const int new_line_num = (v_repeat_shift == 0) ? ch_num : (y & (NUM_FRAME_LINES - 1));
        const uint line_buf_total_len = ((timing_mode->h_active_pixels * line_bytes_per_pixel) >> 2) + count_of(vactive_line_header);

        ch->read_addr = (uintptr_t)&line_buffers[new_line_num * line_buf_total_len];
        ch->transfer_count = line_buf_total_len;

        // Fill line buffer
        if (line_num != new_line_num)
        {
            line_num = new_line_num;
            uint32_t* dst_ptr = &line_buffers[line_num * line_buf_total_len + count_of(vactive_line_header)];

            //if (line_bytes_per_pixel == 2) {
            // h_repeat_shift != 2
            //uint16_t* src_ptr = (uint16_t*)&frame_buffer_display[y * 2 * (timing_mode->h_active_pixels >> h_repeat_shift)];
            for (int i = 0; i < timing_mode->h_active_pixels >> 1; ++i) {
                uint16_t srcColor = (i & 64) ? 0xFF00 : 0x99FF; // *src_ptr++
                uint32_t val = (uint32_t)srcColor * 0x10001;
                *dst_ptr++ = val;
            }
        }
    }

    if (++v_scanline == v_total_active_lines) {
        v_scanline = 0;
        line_num = -1;
        /*
        if (flip_next) {
            flip_next = false;
            display->flip_now();
        }
        __sev();
        */
    }

    irqCounter++;
}

int main() {
    // nem lehet ezelőtt hívni stdio_init_allt, mert rossz UART frekvenciát fog használni
    setupClockPreinit();

    vacak();
    sleep_ms(0);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
    
    stdio_init_all();

    printf("clock preinit kész \n");
    printf("QMI timing %x\n", qmi_hw->m[0].timing);


    Pinout pinout = {
        .clk_p = 13,
        .rgb_p = {15, 17, 19}
    };

        printf("setup display clock\n");
    setupDisplayClock(timing_mode);
    printf("setup display clock done\n");

    v_inactive_total = timing_mode->v_front_porch + timing_mode->v_sync_width + timing_mode->v_back_porch;
    v_total_active_lines = v_inactive_total + timing_mode->v_active_lines;
    int v_repeat = 1 << v_repeat_shift;
    int h_repeat = 1 << h_repeat_shift;

    memcpy(vblank_line_vsync_off, vblank_line_vsync_off_src, sizeof(vblank_line_vsync_off_src));
    vblank_line_vsync_off[0] |= timing_mode->h_front_porch;
    vblank_line_vsync_off[2] |= timing_mode->h_sync_width;
    vblank_line_vsync_off[4] |= timing_mode->h_back_porch + timing_mode->h_active_pixels;

    memcpy(vblank_line_vsync_on, vblank_line_vsync_on_src, sizeof(vblank_line_vsync_on_src));
    vblank_line_vsync_on[0] |= timing_mode->h_front_porch;
    vblank_line_vsync_on[2] |= timing_mode->h_sync_width;
    vblank_line_vsync_on[4] |= timing_mode->h_back_porch + timing_mode->h_active_pixels;

    memcpy(vactive_line_header, vactive_line_header_src, sizeof(vactive_line_header_src));
    vactive_line_header[0] |= timing_mode->h_front_porch;
    vactive_line_header[2] |= timing_mode->h_sync_width;
    vactive_line_header[4] |= timing_mode->h_back_porch;
    vactive_line_header[6] |= timing_mode->h_active_pixels;

    const int frame_pixel_words = (frame_width * h_repeat * line_bytes_per_pixel + 3) >> 2;
    const int frame_line_words = frame_pixel_words + count_of(vactive_line_header);
    const int frame_lines = (v_repeat == 1) ? NUM_CHANS : NUM_FRAME_LINES;

    line_buffers = (uint32_t*)malloc(frame_line_words * 4 * frame_lines);

    for (int i = 0; i < frame_lines; ++i)
    {
        memcpy(&line_buffers[i * frame_line_words], vactive_line_header, count_of(vactive_line_header) * sizeof(uint32_t));
    }

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
        if (timing_mode->bit_clk_khz > 900000) {
            gpio_set_slew_rate(i, GPIO_SLEW_RATE_FAST);
        }
    }

    printf("GPIO configured\n");

        // The channels are set up identically, to transfer a whole scanline and
    // then chain to the next channel. Each time a channel finishes, we
    // reconfigure the one that just finished, meanwhile the other channel(s)
    // are already making progress.
    // Using just 2 channels was insufficient to avoid issues with the IRQ.
    dma_channel_config c;
    c = dma_channel_get_default_config(0);
    channel_config_set_chain_to(&c, 1);
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(
        0,
        &c,
        &hstx_fifo_hw->fifo,
        vblank_line_vsync_off,
        count_of(vblank_line_vsync_off),
        false
    );
    c = dma_channel_get_default_config(1);
    channel_config_set_chain_to(&c, 2);
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(
        1,
        &c,
        &hstx_fifo_hw->fifo,
        vblank_line_vsync_off,
        count_of(vblank_line_vsync_off),
        false
    );
    for (int i = 2; i < NUM_CHANS; ++i) {
        c = dma_channel_get_default_config(i);
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

    printf("DMA channels claimed\n");

    dma_hw->intr = (1 << NUM_CHANS) - 1;
    dma_hw->ints2 = (1 << NUM_CHANS) - 1;
    dma_hw->inte2 = (1 << NUM_CHANS) - 1;
    irq_set_exclusive_handler(DMA_IRQ_2, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_2, true);

    dma_channel_start(0);

    printf("Elindult\n");
    /*
        DVHSTX* dvhstx = new DVHSTX();
        if (!dvhstx->init(480, 270, DVHSTX::MODE_RGB565, pinout))
            panic("init fail\n");
        else
            printf("init success\n");
    */
    while (true) {
        printf("Hello, world! %d, %d\n", irqCounter, v_scanline);
        sleep_ms(1000);
    }
}
