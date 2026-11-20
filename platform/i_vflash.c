/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "d_main.h"
#include "d_net.h"
#include "doomdef.h"
#include "doomstat.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_video.h"
#include "m_misc.h"
#include "runtime.h"
#include "v_video.h"
#include "vflash_hw.h"
#include "w_wad.h"
#include "z_zone.h"

extern int key_up, key_down, key_left, key_right;
extern int key_fire, key_use, key_strafeleft, key_straferight;
extern int messageToPrint;

#if SCREENWIDTH != VFLASH_DOOM_WIDTH || SCREENHEIGHT != VFLASH_DOOM_HEIGHT
#error Doom framebuffer size must match the V.Flash display backend
#endif

static unsigned short video_palette[256];
static unsigned held_buttons;
static ticcmd_t empty_command;

static int next_weapon_key(void)
{
    static const struct {
        weapontype_t weapon;
        int key;
    } cycle[] = {
        { wp_chainsaw, '1' }, { wp_fist, '1' }, { wp_pistol, '2' },
        { wp_supershotgun, '3' }, { wp_shotgun, '3' },
        { wp_chaingun, '4' }, { wp_missile, '5' }, { wp_plasma, '6' },
        { wp_bfg, '7' },
    };
    const unsigned count = sizeof(cycle) / sizeof(cycle[0]);
    const player_t *player = &players[consoleplayer];
    unsigned current;
    unsigned offset;

    if (gamestate != GS_LEVEL || !usergame)
        return 0;

    for (current = 0; current < count; ++current)
        if (cycle[current].weapon == player->readyweapon)
            break;

    for (offset = 1; offset < count; ++offset) {
        unsigned index = (current + offset) % count;
        weapontype_t weapon = cycle[index].weapon;

        if (!player->weaponowned[weapon])
            continue;

        // The vanilla weapon keys share slots for these pairs.
        if (weapon == wp_fist && player->weaponowned[wp_chainsaw]
            && (player->readyweapon != wp_chainsaw || !player->powers[pw_strength]))
            continue;
        if (weapon == wp_supershotgun && gamemode != commercial)
            continue;
        if (weapon == wp_shotgun && gamemode == commercial
            && player->weaponowned[wp_supershotgun]
            && player->readyweapon != wp_supershotgun)
            continue;
        if (gamemode == shareware && (weapon == wp_plasma || weapon == wp_bfg))
            continue;

        return cycle[index].key;
    }

    return 0;
}

static int button_key(unsigned button)
{
    switch (button) {
    case VFLASH_BTN_UP: return menuactive ? KEY_UPARROW : key_up;
    case VFLASH_BTN_DOWN: return menuactive ? KEY_DOWNARROW : key_down;
    case VFLASH_BTN_LEFT: return menuactive ? KEY_LEFTARROW : key_left;
    case VFLASH_BTN_RIGHT: return menuactive ? KEY_RIGHTARROW : key_right;
    case VFLASH_BTN_RED: return messageToPrint ? 'y' : key_fire;
    case VFLASH_BTN_GREEN:
        if (messageToPrint)
            return 'y';
        return menuactive ? KEY_ENTER : key_use;
    case VFLASH_BTN_YELLOW: return menuactive ? 0 : next_weapon_key();
    case VFLASH_BTN_BLUE:
        if (messageToPrint)
            return 'n';
        return menuactive ? KEY_BACKSPACE : KEY_TAB;
    case VFLASH_BTN_ENTER:
        if (messageToPrint)
            return 'y';
        return menuactive ? KEY_ENTER : KEY_ESCAPE;
    case VFLASH_BTN_EXIT: return messageToPrint ? 'n' : KEY_ESCAPE;
    case VFLASH_BTN_L: return key_strafeleft;
    case VFLASH_BTN_R: return key_straferight;
    case VFLASH_BTN_QUESTION: return KEY_F1;
    case VFLASH_BTN_BOOK: return KEY_PAUSE;
    default: return 0;
    }
}

void I_StartTic(void)
{
    static const unsigned buttons[] = {
        VFLASH_BTN_UP, VFLASH_BTN_DOWN, VFLASH_BTN_LEFT, VFLASH_BTN_RIGHT,
        VFLASH_BTN_RED, VFLASH_BTN_GREEN, VFLASH_BTN_YELLOW, VFLASH_BTN_BLUE,
        VFLASH_BTN_ENTER, VFLASH_BTN_EXIT, VFLASH_BTN_L, VFLASH_BTN_R,
        VFLASH_BTN_QUESTION, VFLASH_BTN_BOOK,
    };
    // Release the original Doom key if the menu changed while a button was held.
    static int active_keys[sizeof(buttons) / sizeof(buttons[0])];
    unsigned current_buttons = vflash_pad();
    unsigned changed = current_buttons ^ held_buttons;
    unsigned index;

    for (index = 0; index < sizeof(buttons) / sizeof(buttons[0]); ++index) {
        unsigned button = buttons[index];
        event_t event = { ev_keyup, 0, 0, 0 };

        if (!(changed & button))
            continue;

        if (current_buttons & button) {
            event.type = ev_keydown;
            active_keys[index] = button_key(button);
        }
        event.data1 = active_keys[index];
        if (event.type == ev_keyup)
            active_keys[index] = 0;

        if (event.data1 > 0 && event.data1 <= 255)
            D_PostEvent(&event);
    }
    held_buttons = current_buttons;
}

void I_InitGraphics(void)
{
    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
}

void I_SetPalette(byte *palette)
{
    int gamma = usegamma;
    unsigned index;

    if (gamma < 0 || gamma >= 5)
        gamma = 0;

    for (index = 0; index < 256; ++index) {
        unsigned red = gammatable[gamma][*palette++];
        unsigned green = gammatable[gamma][*palette++];
        unsigned blue = gammatable[gamma][*palette++];

        video_palette[index] = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
    }
}

void I_FinishUpdate(void)
{
    vflash_video_present(screens[0], video_palette);
}

void I_ReadScreen(byte *destination)
{
    memcpy(destination, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

void I_ShutdownGraphics(void) {}
void I_UpdateNoBlit(void) {}
void I_StartFrame(void) {}
void I_BeginRead(void) {}
void I_EndRead(void) {}

void I_Init(void)
{
    vflash_video_init();
    I_InitSound();
}

int I_GetTime(void)
{
    return (int) vflash_tics();
}

void I_WaitVBL(int count)
{
    unsigned start;
    unsigned ticks;

    if (count <= 0)
        return;

    // Doom specifies 70 Hz retraces, and the machine clock exposes 35 Hz tics.
    start = vflash_tics();
    ticks = ((unsigned) count + 1) / 2;
    while (vflash_tics() - start < ticks)
        I_UpdateSound();
}

ticcmd_t *I_BaseTiccmd(void)
{
    return &empty_command;
}

byte *I_ZoneBase(int *size)
{
    const unsigned reserve = 640 * 1024;
    const unsigned zone_min = 2 * 1024 * 1024;
    const unsigned zone_max = 5 * 1024 * 1024;
    unsigned available = vflash_heap_available();
    unsigned zone_size;
    byte *zone;

    if (available < reserve + zone_min)
        I_Error("Only %u KiB of heap remains for Doom", available / 1024);

    zone_size = available - reserve;
    if (zone_size > zone_max)
        zone_size = zone_max;
    zone_size &= ~7u;
    zone = malloc(zone_size);
    if (!zone)
        I_Error("Could not allocate %u KiB for the Doom zone", zone_size / 1024);

    *size = zone_size;
    return zone;
}

byte *I_AllocLow(int length)
{
    byte *memory = calloc(1, (size_t) length);

    if (!memory)
        I_Error("Could not allocate %d bytes", length);
    return memory;
}

void I_Tactile(int on, int off, int total)
{
    (void) on;
    (void) off;
    (void) total;
}

void I_Quit(void)
{
    D_QuitNetGame();
    I_ShutdownSound();
    I_ShutdownMusic();
    M_SaveDefaults();
    I_ShutdownGraphics();
    exit(0);
}

void I_Error(char *format, ...)
{
    va_list arguments;

    fprintf(stderr, "Doom error: ");
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
    fflush(stderr);

    D_QuitNetGame();
    I_ShutdownSound();
    I_ShutdownMusic();
    I_ShutdownGraphics();
    exit(1);
}
