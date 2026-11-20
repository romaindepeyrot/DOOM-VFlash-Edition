/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "vflash_hw.h"

#include <stdint.h>

#define REGISTER(address) (*(volatile uint32_t *)(uintptr_t)(address))

/* The scanout engine, timer pair, and UART0 in FlashEm's hw.c. */
#define VE_BASE 0xb8000000u
#define VE_SCREEN_SIZE (VE_BASE + 0x0cu)
#define VE_LAYER_ENABLE (VE_BASE + 0x100u)
#define VE_LAYER_LAYOUT (VE_BASE + 0x108u)
#define VE_BACKGROUND (VE_BASE + 0x10cu)
#define VE_LAYER0_START (VE_BASE + 0x130u)
#define VE_LAYER0_END (VE_BASE + 0x134u)
#define VE_LAYER0_ADDRESS (VE_BASE + 0x160u)

#define PMU_CLOCKS 0x900b0000u
#define TIMER_VALUE 0x90010000u
#define TIMER_DIVIDER 0x90010004u
#define TIMER_CONTROL 0x90010008u
#define TIMER_DISABLE 0x10u
#define TIMER_COUNT_UP 0x08u
#define TIMER_NO_RELOAD 0x07u
#define TIMER_COUNTER_HZ 2500u
#define DOOM_TIC_HZ 35u

#define PAD_DATA 0x90020000u
#define PAD_INTERRUPT_ENABLE 0x90020004u
#define PAD_FIFO_CONTROL 0x90020008u
#define PAD_LINE_CONTROL 0x9002000cu
#define PAD_LINE_STATUS 0x90020014u
#define PAD_RX_READY 0x01u
#define PAD_TX_READY 0x20u
#define PAD_RECONNECT_TICS (2 * DOOM_TIC_HZ)
#define PAD_TIMEOUT_TICS (DOOM_TIC_HZ / 4)

static uint16_t frames[2][VFLASH_DOOM_WIDTH * VFLASH_DOOM_HEIGHT]
    __attribute__((aligned(32)));
static unsigned front_frame;

static uint32_t timer_clock;
static uint32_t timer_step;
static uint32_t timer_phase;
static uint16_t timer_last;
static unsigned doom_tic;

static unsigned pad_buttons;
static unsigned pad_last_report;
static unsigned pad_last_request;
static unsigned report_pos;
static uint8_t report[12];

static uint32_t apb_clock(void)
{
    static const unsigned cpu_divisor[8] = {2, 4, 8, 16, 32, 32, 32, 32};
    const uint32_t clocks = REGISTER(PMU_CLOCKS);
    const unsigned pll_step = (clocks >> 12) & 31;
    const unsigned apb_divisor = 1 + ((clocks >> 8) & 7);

    return (300000000u - 6000000u * pll_step)
        / cpu_divisor[clocks & 7] / apb_divisor;
}

static void request_pad_reports(void)
{
    /* Type 1, two bytes 0x40 0x03, checksum 0x0046. */
    static const uint8_t request[] = {
        0xff, 0x01, 0x02, 0x40, 0x74, 0x40, 0x46, 0x44, 0x40,
    };

    for (unsigned i = 0; i < sizeof request; ++i) {
        unsigned timeout = 100000;
        while (!(REGISTER(PAD_LINE_STATUS) & PAD_TX_READY)) {
            if (--timeout == 0)
                return;
        }
        REGISTER(PAD_DATA) = request[i];
    }
}

static void init_timer_and_pad(void)
{
    if (timer_clock)
        return;

    const uint32_t bus_clock = apb_clock();
    const uint32_t uart_baud = 115200;
    const uint32_t uart_divisor = (bus_clock + 8 * uart_baud) / (16 * uart_baud);
    timer_clock = bus_clock / 2;
    const uint32_t divisor = (timer_clock + TIMER_COUNTER_HZ / 2) / TIMER_COUNTER_HZ;
    timer_step = divisor * DOOM_TIC_HZ;

    REGISTER(TIMER_CONTROL) = TIMER_DISABLE;
    REGISTER(TIMER_DIVIDER) = divisor - 1;
    REGISTER(TIMER_VALUE) = 0;
    REGISTER(TIMER_CONTROL) = TIMER_COUNT_UP | TIMER_NO_RELOAD;

    REGISTER(PAD_LINE_CONTROL) = 0x03;
    REGISTER(PAD_INTERRUPT_ENABLE) = 0;
    REGISTER(PAD_LINE_CONTROL) = 0x83;
    REGISTER(PAD_DATA) = uart_divisor & 0xff;
    REGISTER(PAD_INTERRUPT_ENABLE) = uart_divisor >> 8;
    REGISTER(PAD_LINE_CONTROL) = 0x03;
    REGISTER(PAD_FIFO_CONTROL) = 0x07;
    request_pad_reports();
}

static void decode_pad_report(void)
{
    static const unsigned button_mask[15] = {
        VFLASH_BTN_L, VFLASH_BTN_R, VFLASH_BTN_ENTER, VFLASH_BTN_STICK,
        VFLASH_BTN_A, VFLASH_BTN_B, VFLASH_BTN_C, VFLASH_BTN_D,
        VFLASH_BTN_QUESTION, VFLASH_BTN_BOOK, VFLASH_BTN_EXIT,
        VFLASH_BTN_YELLOW, VFLASH_BTN_RED, VFLASH_BTN_BLUE, VFLASH_BTN_GREEN,
    };
    uint8_t data[8];

    for (unsigned i = 0; i < 4; ++i) {
        const unsigned first = report[3 * i] & 0x0f;
        const unsigned middle = report[3 * i + 1] & 0x3f;
        const unsigned last = report[3 * i + 2] & 0x3f;
        data[2 * i] = (uint8_t)(first | (middle & 0x0f) << 4);
        data[2 * i + 1] = (uint8_t)(middle >> 4 | last << 2);
    }

    unsigned sum = 1 + 6;
    for (unsigned i = 0; i < 6; ++i)
        sum += data[i];
    if (sum != (data[6] | (unsigned)data[7] << 8) || (data[0] & 0x0f) != 1)
        return;

    const int vertical = (int8_t)((data[0] >> 4) | (data[1] & 0x0f) << 4);
    const int horizontal = (int8_t)((data[1] >> 5) | (data[2] & 0x1f) << 3);
    const unsigned raw = data[2] >> 6 | (unsigned)data[3] << 2
        | (unsigned)(data[4] & 0x3f) << 10;
    unsigned buttons = 0;

    if (vertical > 32)
        buttons |= VFLASH_BTN_UP;
    if (vertical < -32)
        buttons |= VFLASH_BTN_DOWN;
    if (horizontal > 32)
        buttons |= VFLASH_BTN_RIGHT;
    if (horizontal < -32)
        buttons |= VFLASH_BTN_LEFT;
    for (unsigned i = 0; i < sizeof button_mask / sizeof button_mask[0]; ++i) {
        if (raw & (1u << i))
            buttons |= button_mask[i];
    }

    pad_buttons = buttons;
    pad_last_report = doom_tic;
}

static void poll_pad(void)
{
    while (REGISTER(PAD_LINE_STATUS) & PAD_RX_READY) {
        const uint8_t value = (uint8_t)REGISTER(PAD_DATA);

        if (value == 0xff) {
            report_pos = 1;
        } else if (report_pos == 1) {
            report_pos = (value == 1) ? 2 : 0;
        } else if (report_pos == 2) {
            report_pos = (value == 6) ? 3 : 0;
        } else if (report_pos >= 3) {
            if (value < 0x40 || value > 0x7f) {
                report_pos = 0;
                continue;
            }

            report[report_pos - 3] = value;
            if (++report_pos == sizeof report + 3) {
                decode_pad_report();
                report_pos = 0;
            }
        }
    }

    if (doom_tic - pad_last_report >= PAD_TIMEOUT_TICS)
        pad_buttons = 0;
    if (doom_tic - pad_last_report >= PAD_RECONNECT_TICS
        && doom_tic - pad_last_request >= PAD_RECONNECT_TICS) {
        pad_last_request = doom_tic;
        request_pad_reports();
    }
}

void vflash_video_init(void)
{
    const uint32_t size = (VFLASH_DOOM_WIDTH - 1u) | (VFLASH_DOOM_HEIGHT - 1u) << 16;

    REGISTER(VE_LAYER_ENABLE) = 0;
    REGISTER(VE_SCREEN_SIZE) = size;
    REGISTER(VE_LAYER_LAYOUT) = 0;
    REGISTER(VE_BACKGROUND) = 0;
    REGISTER(VE_LAYER0_START) = 0;
    REGISTER(VE_LAYER0_END) = size;
    REGISTER(VE_LAYER0_ADDRESS) = (uint32_t)(uintptr_t)frames[front_frame];
    REGISTER(VE_LAYER_ENABLE) = 1u << 4;

    init_timer_and_pad();
}

void vflash_video_present(const unsigned char *pixels, const unsigned short *palette)
{
    uint16_t colors[256];
    const unsigned back_frame = front_frame ^ 1u;
    uint16_t *destination = frames[back_frame];

    for (unsigned i = 0; i < 256; ++i) {
        const unsigned rgb = palette[i];
        colors[i] = (uint16_t)((rgb >> 11) | ((rgb >> 1) & 0x03e0) | ((rgb & 0x1f) << 10));
    }
    for (unsigned i = 0; i < VFLASH_DOOM_WIDTH * VFLASH_DOOM_HEIGHT; ++i)
        destination[i] = colors[pixels[i]];

    /* Complete the framebuffer stores before publishing its address. */
    __asm__ volatile ("" ::: "memory");
    REGISTER(VE_LAYER0_ADDRESS) = (uint32_t)(uintptr_t)destination;
    front_frame = back_frame;
}

unsigned vflash_tics(void)
{
    init_timer_and_pad();

    const uint16_t counter = (uint16_t)REGISTER(TIMER_VALUE);
    const uint16_t elapsed = (uint16_t)(counter - timer_last);
    uint64_t phase = timer_phase + (uint64_t)elapsed * timer_step;
    timer_last = counter;

    /* Retain the fraction so prescaler rounding doesn't skew the game clock. */
    while (phase >= timer_clock) {
        phase -= timer_clock;
        ++doom_tic;
    }
    timer_phase = (uint32_t)phase;

    poll_pad();
    return doom_tic;
}

unsigned vflash_pad(void)
{
    vflash_tics();
    return pad_buttons;
}
