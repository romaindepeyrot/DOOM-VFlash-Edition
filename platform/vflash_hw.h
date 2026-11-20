/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef VFLASH_HW_H
#define VFLASH_HW_H

#define VFLASH_DOOM_WIDTH 320
#define VFLASH_DOOM_HEIGHT 200

/* Match FlashEm's VFLASH_INPUT button masks. */
#define VFLASH_BTN_UP       (1u << 0)
#define VFLASH_BTN_DOWN     (1u << 1)
#define VFLASH_BTN_LEFT     (1u << 2)
#define VFLASH_BTN_RIGHT    (1u << 3)
#define VFLASH_BTN_RED      (1u << 4)
#define VFLASH_BTN_YELLOW   (1u << 5)
#define VFLASH_BTN_GREEN    (1u << 6)
#define VFLASH_BTN_BLUE     (1u << 7)
#define VFLASH_BTN_ENTER    (1u << 8)
#define VFLASH_BTN_EXIT     (1u << 9)
#define VFLASH_BTN_L        (1u << 10)
#define VFLASH_BTN_R        (1u << 11)
#define VFLASH_BTN_STICK    (1u << 12)
#define VFLASH_BTN_QUESTION (1u << 13)
#define VFLASH_BTN_BOOK     (1u << 14)
#define VFLASH_BTN_A        (1u << 15)
#define VFLASH_BTN_B        (1u << 16)
#define VFLASH_BTN_C        (1u << 17)
#define VFLASH_BTN_D        (1u << 18)

void vflash_video_init(void);
void vflash_video_present(const unsigned char *pixels, const unsigned short *palette);
unsigned vflash_pad(void);
unsigned vflash_tics(void);

#endif
