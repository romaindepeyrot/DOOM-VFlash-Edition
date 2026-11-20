/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <limits.h>
#include <stdint.h>

#include "i_sound.h"
#include "w_wad.h"
#include "z_zone.h"

#define SOUND_REG(offset) (*(volatile uint32_t *)(uintptr_t)(0xb0000000u + (offset)))
#define VOICE_REG(voice, offset) SOUND_REG((voice) * 0x40u + (offset))

enum {
    VOICES = 8,
    VOICE_START = 0x00,
    VOICE_END = 0x04,
    VOICE_LOOP = 0x08,
    VOICE_MODE = 0x0c,
    VOICE_PITCH = 0x10,
    VOICE_GAIN = 0x20,
    VOICE_STATUS = 0x34,
    VOICE_ACTIVE = 0x70000,
    KEY_ON_LOW = 0x1010,
    KEY_OFF_LOW = 0x1018,
    MASTER_RAMP = 0x1184,
    MASTER_GAIN = 0x1188,
    PCM_U8_11025 = 0x1e9,
    PCM_U8_22050 = 0x1e5,
};

typedef struct {
    int handle;
    int lump;
    unsigned sample_rate;
} SoundVoice;

static SoundVoice voices[VOICES];
static int next_handle;
static int sound_ready;

static unsigned read_le16(const unsigned char *data)
{
    return data[0] | (unsigned)data[1] << 8;
}

static unsigned read_le32(const unsigned char *data)
{
    return read_le16(data) | read_le16(data + 2) << 16;
}

static void unpin_lump(int lump)
{
    unsigned voice;

    for (voice = 0; voice < VOICES; ++voice) {
        if (voices[voice].handle && voices[voice].lump == lump)
            return;
    }
    Z_ChangeTag2(lumpcache[lump], PU_CACHE);
}

static void release_voice(unsigned voice)
{
    SOUND_REG(KEY_OFF_LOW) = 1u << voice;
    voices[voice].handle = 0;
    unpin_lump(voices[voice].lump);
}

static unsigned find_voice(int handle)
{
    unsigned voice;

    if (handle > 0) {
        for (voice = 0; voice < VOICES; ++voice) {
            if (voices[voice].handle == handle)
                return voice;
        }
    }
    return VOICES;
}

static void set_voice_params(unsigned voice, int volume, int separation, int pitch)
{
    /* Approximate DOOM's exponential pitch steps by linear interpolation in Q12. */
    static const uint16_t pitch_steps[] = {
        1024, 1218, 1448, 1722, 2048, 2435, 2896, 3444, 4096,
        4871, 5793, 6889, 8192, 9742, 11585, 13777, 16384
    };
    unsigned index;
    unsigned step;
    unsigned hardware_pitch;
    unsigned mode = PCM_U8_11025;
    unsigned gain, left, right;
    unsigned pan;

    pitch = pitch < 0 ? 0 : pitch > 255 ? 255 : pitch;
    index = (unsigned)pitch / 16;
    step = pitch_steps[index] +
           (pitch_steps[index + 1] - pitch_steps[index]) * ((unsigned)pitch % 16) / 16;
    hardware_pitch = (voices[voice].sample_rate * step + 5512) / 11025;
    if (hardware_pitch > 0x1fff) {
        mode = PCM_U8_22050;
        hardware_pitch /= 2;
    }
    hardware_pitch = hardware_pitch < 1 ? 1 :
                     hardware_pitch > 0x1fff ? 0x1fff : hardware_pitch;

    volume = volume < 0 ? 0 : volume > 15 ? 15 : volume;
    separation = separation < 0 ? 0 : separation > 255 ? 255 : separation;
    pan = (unsigned)separation + 1;
    gain = (unsigned)volume * 17;
    left = gain - (gain * pan * pan >> 16);
    right = gain - (gain * (256 - pan) * (256 - pan) >> 16);

    VOICE_REG(voice, VOICE_MODE) = mode;
    VOICE_REG(voice, VOICE_PITCH) = hardware_pitch;
    VOICE_REG(voice, VOICE_GAIN) = left | (right << 8);
}

void I_InitSound(void)
{
    if (sound_ready)
        return;
    SOUND_REG(KEY_OFF_LOW) = (1u << VOICES) - 1;
    SOUND_REG(MASTER_RAMP) = 0xffffffffu;
    SOUND_REG(MASTER_GAIN) = 0x7fff7fffu;
    sound_ready = 1;
}

void I_SetChannels(void)
{
}

int I_GetSfxLumpNum(sfxinfo_t *sfx)
{
    char name[9] = "ds";
    unsigned i;
    int lump;

    if (sfx->link)
        sfx = sfx->link;
    for (i = 0; i < 6 && sfx->name[i]; ++i)
        name[i + 2] = sfx->name[i];
    lump = W_CheckNumForName(name);
    return lump >= 0 ? lump : W_CheckNumForName("dspistol");
}

int I_StartSound(int id, int volume, int separation, int pitch, int priority)
{
    unsigned voice, oldest, count, rate;
    int lump, length;
    const unsigned char *data;
    uint32_t start;

    (void)priority;
    if (!sound_ready || id <= 0 || id >= NUMSFX)
        return 0;
    lump = S_sfx[id].lumpnum;
    if (lump < 0)
        return 0;
    length = W_LumpLength(lump);
    if (length < 9)
        return 0;

    I_UpdateSound();
    oldest = 0;
    for (voice = 0; voice < VOICES && voices[voice].handle; ++voice) {
        if (voices[voice].handle < voices[oldest].handle)
            oldest = voice;
    }
    data = W_CacheLumpNum(lump, PU_STATIC);
    rate = read_le16(data + 2);
    count = read_le32(data + 4);
    if (read_le16(data) != 3 || !rate || !count || count > (unsigned)length - 8) {
        unpin_lump(lump);
        return 0;
    }

    if (voice == VOICES) {
        voice = oldest;
        release_voice(voice);
        /* The evicted voice may have been using the same sample. */
        Z_ChangeTag2(lumpcache[lump], PU_STATIC);
    }

    /* Zevio uses inclusive byte addresses for unsigned 8-bit PCM. */
    start = (uint32_t)(uintptr_t)(data + 8);
    VOICE_REG(voice, VOICE_START) = start;
    VOICE_REG(voice, VOICE_END) = start + count - 1;
    VOICE_REG(voice, VOICE_LOOP) = start;
    VOICE_REG(voice, 0x14) = 0xffffffffu;
    VOICE_REG(voice, 0x18) = 0xffff0000u;
    VOICE_REG(voice, 0x1c) = 0xffffffffu;
    VOICE_REG(voice, 0x24) = 0;
    VOICE_REG(voice, VOICE_STATUS) = 0;

    next_handle = next_handle == INT_MAX ? 1 : next_handle + 1;
    voices[voice].handle = next_handle;
    voices[voice].lump = lump;
    voices[voice].sample_rate = rate;
    set_voice_params(voice, volume, separation, pitch);
    SOUND_REG(KEY_ON_LOW) = 1u << voice;
    return next_handle;
}

void I_StopSound(int handle)
{
    unsigned voice = find_voice(handle);

    if (voice < VOICES)
        release_voice(voice);
}

int I_SoundIsPlaying(int handle)
{
    unsigned voice = find_voice(handle);

    if (voice == VOICES)
        return 0;
    if (VOICE_REG(voice, VOICE_STATUS) & VOICE_ACTIVE)
        return 1;
    release_voice(voice);
    return 0;
}

void I_UpdateSoundParams(int handle, int volume, int separation, int pitch)
{
    unsigned voice = find_voice(handle);

    if (voice < VOICES)
        set_voice_params(voice, volume, separation, pitch);
}

void I_UpdateSound(void)
{
    unsigned voice;

    for (voice = 0; voice < VOICES; ++voice) {
        if (voices[voice].handle && !(VOICE_REG(voice, VOICE_STATUS) & VOICE_ACTIVE))
            release_voice(voice);
    }
}

void I_SubmitSound(void)
{
}

void I_ShutdownSound(void)
{
    unsigned voice;

    for (voice = 0; voice < VOICES; ++voice) {
        if (voices[voice].handle)
            release_voice(voice);
    }
    sound_ready = 0;
}

/* The WAD's MUS tracks require instrument synthesis. */
void I_InitMusic(void) {}
void I_ShutdownMusic(void) {}
void I_SetMusicVolume(int volume) { (void)volume; }
void I_PauseSong(int handle) { (void)handle; }
void I_ResumeSong(int handle) { (void)handle; }
int I_RegisterSong(void *data) { (void)data; return 0; }
void I_PlaySong(int handle, int looping) { (void)handle; (void)looping; }
void I_StopSong(int handle) { (void)handle; }
void I_UnRegisterSong(int handle) { (void)handle; }
