/*
 * Minimal libretro frontend for driving mgba_libretro.bc under Emscripten.
 *
 * This does NOT reimplement RetroArch. It's the smallest possible amount of
 * glue that satisfies the retro_* API: set up the six callbacks the core
 * needs (environment, video, audio x2, input poll, input state), call
 * retro_init()/retro_load_game(), then let JS drive retro_run() once per
 * frame and pull the video/audio buffers back out.
 *
 * Build with build.sh (links this against mgba_libretro.bc).
 */

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "libretro.h"

/* GBA's largest frame is 240x160 @ up to 4 bytes/px; this leaves generous
 * headroom without needing to size it exactly. */
#define MAX_VIDEO_BYTES (1024 * 1024)
/* One video frame's worth of stereo audio is normally ~800 frames at 48kHz/60fps;
 * this is sized way larger so a slow/late call never overflows. */
#define MAX_AUDIO_FRAMES 48000

static uint8_t g_video_buf[MAX_VIDEO_BYTES];
static unsigned g_video_width = 0;
static unsigned g_video_height = 0;
static size_t g_video_pitch = 0;
/* Matches enum retro_pixel_format: 0 = 0RGB1555 (legacy default), 1 = XRGB8888, 2 = RGB565 */
static int g_pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;

static int16_t g_audio_buf[MAX_AUDIO_FRAMES * 2];
static size_t g_audio_frames = 0;

/* Indexed by RETRO_DEVICE_ID_JOYPAD_*; JS writes into this directly. */
static int16_t g_buttons[16];

/* These paths are handed to the core; JS is responsible for making sure
 * they exist inside the mounted IDBFS filesystem before ROM load. */
static const char *g_save_dir = "/data/saves";
static const char *g_system_dir = "/data/system";

static double g_audio_rate = 32768.0; /* overwritten from retro_get_system_av_info after load */

static bool env_cb(unsigned cmd, void *data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            g_pixel_format = *(const int *)data;
            return true;
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            *(const char **)data = g_save_dir;
            return true;
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
            *(const char **)data = g_system_dir;
            return true;
        default:
            /* Anything we don't explicitly support: tell the core "unavailable".
             * mGBA's libretro core tolerates this for everything else it needs. */
            return false;
    }
}

static void video_cb(const void *data, unsigned width, unsigned height, size_t pitch) {
    /* The core passes data == NULL to mean "duplicate frame, nothing changed" */
    if (!data) return;
    size_t bytes = pitch * height;
    if (bytes > MAX_VIDEO_BYTES) bytes = MAX_VIDEO_BYTES;
    memcpy(g_video_buf, data, bytes);
    g_video_width = width;
    g_video_height = height;
    g_video_pitch = pitch;
}

static void audio_sample_cb(int16_t left, int16_t right) {
    if (g_audio_frames < MAX_AUDIO_FRAMES) {
        g_audio_buf[g_audio_frames * 2] = left;
        g_audio_buf[g_audio_frames * 2 + 1] = right;
        g_audio_frames++;
    }
}

static size_t audio_batch_cb(const int16_t *data, size_t frames) {
    size_t room = MAX_AUDIO_FRAMES - g_audio_frames;
    if (frames > room) frames = room;
    memcpy(&g_audio_buf[g_audio_frames * 2], data, frames * 2 * sizeof(int16_t));
    g_audio_frames += frames;
    return frames;
}

static void input_poll_cb(void) {
    /* no-op: JS calls mgba_web_set_button() directly before mgba_web_run_frame() */
}

static int16_t input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    (void)index;
    if (port != 0 || device != RETRO_DEVICE_JOYPAD || id >= 16) return 0;
    return g_buttons[id];
}

EMSCRIPTEN_KEEPALIVE
int mgba_web_init(void) {
    retro_set_environment(env_cb);
    retro_set_video_refresh(video_cb);
    retro_set_audio_sample(audio_sample_cb);
    retro_set_audio_sample_batch(audio_batch_cb);
    retro_set_input_poll(input_poll_cb);
    retro_set_input_state(input_state_cb);
    retro_init();
    return 0;
}

/* path is used by the core to sniff the extension (.gba vs .gb/.gbc) to
 * decide which internal core (GBA vs GB/GBC) to run. Pass e.g. "game.gba". */
EMSCRIPTEN_KEEPALIVE
int mgba_web_load_rom(uint8_t *data, int size, const char *path) {
    struct retro_game_info game;
    game.path = path;
    game.data = data;
    game.size = (size_t)size;
    game.meta = NULL;

    if (!retro_load_game(&game)) return 0;

    struct retro_system_av_info av;
    retro_get_system_av_info(&av);
    g_video_width = av.geometry.base_width;
    g_video_height = av.geometry.base_height;
    g_audio_rate = av.timing.sample_rate;
    return 1;
}

EMSCRIPTEN_KEEPALIVE
void mgba_web_run_frame(void) {
    g_audio_frames = 0;
    retro_run();
}

EMSCRIPTEN_KEEPALIVE
uint8_t *mgba_web_get_video_buffer(void) { return g_video_buf; }
EMSCRIPTEN_KEEPALIVE
unsigned mgba_web_get_video_width(void) { return g_video_width; }
EMSCRIPTEN_KEEPALIVE
unsigned mgba_web_get_video_height(void) { return g_video_height; }
EMSCRIPTEN_KEEPALIVE
unsigned mgba_web_get_video_pitch(void) { return (unsigned)g_video_pitch; }
EMSCRIPTEN_KEEPALIVE
int mgba_web_get_pixel_format(void) { return g_pixel_format; }

EMSCRIPTEN_KEEPALIVE
int16_t *mgba_web_get_audio_buffer(void) { return g_audio_buf; }
EMSCRIPTEN_KEEPALIVE
unsigned mgba_web_get_audio_frames(void) { return (unsigned)g_audio_frames; }
EMSCRIPTEN_KEEPALIVE
int mgba_web_get_audio_rate(void) { return (int)g_audio_rate; }

EMSCRIPTEN_KEEPALIVE
void mgba_web_set_button(int id, int pressed) {
    if (id >= 0 && id < 16) g_buttons[id] = pressed ? 1 : 0;
}

/* Save states via the core's own serialize API (separate from the .sav
 * battery-save files it writes to g_save_dir). Returns byte count written,
 * or the negative of the required size if buf was too small. */
EMSCRIPTEN_KEEPALIVE
int mgba_web_save_state(uint8_t *buf, int max_len) {
    size_t need = retro_serialize_size();
    if ((int)need > max_len) return -(int)need;
    return retro_serialize(buf, need) ? (int)need : 0;
}

EMSCRIPTEN_KEEPALIVE
int mgba_web_load_state(const uint8_t *buf, int len) {
    return retro_unserialize(buf, (size_t)len) ? 1 : 0;
}

/* Battery save RAM (the actual in-game "save file" data). Most libretro
 * cores, mGBA included, don't write this to disk themselves — the
 * frontend is expected to pull it out and persist it. */
EMSCRIPTEN_KEEPALIVE
uint8_t *mgba_web_get_sram_ptr(void) {
    return (uint8_t *)retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
}
EMSCRIPTEN_KEEPALIVE
unsigned mgba_web_get_sram_size(void) {
    return (unsigned)retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
}
