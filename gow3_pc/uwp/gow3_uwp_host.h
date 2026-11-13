/* gow3 UWP: the C interface between the UWP app (TLOU.Uwp, C++/WinRT, built with MSVC) and the
 * runtime DLL (gow3-runtime.dll, built with MSYS2 CLANG64 and -DGOW3_UWP).
 *
 * The app owns everything the app container lets it own: the CoreWindow, Windows.Gaming.Input
 * gamepads, the keyboard and XAudio2. It hands the runtime a table of callbacks; the runtime's
 * SDL3 stand-in (uwp/sdl) answers the SDL calls the port makes with them. Every callback may be
 * called from any runtime thread and must be thread-safe.
 *
 * Plain C, no Windows headers: both compilers see the same layout. */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GOW3_UWP_HOST_VERSION 1

/* Windows.Gaming.Input.GamepadButtons bits, as the app reads them. */
enum {
    GOW3_UWP_PAD_MENU = 0x1, GOW3_UWP_PAD_VIEW = 0x2, GOW3_UWP_PAD_A = 0x4, GOW3_UWP_PAD_B = 0x8,
    GOW3_UWP_PAD_X = 0x10, GOW3_UWP_PAD_Y = 0x20, GOW3_UWP_PAD_DPAD_UP = 0x40,
    GOW3_UWP_PAD_DPAD_DOWN = 0x80, GOW3_UWP_PAD_DPAD_LEFT = 0x100, GOW3_UWP_PAD_DPAD_RIGHT = 0x200,
    GOW3_UWP_PAD_LEFT_SHOULDER = 0x400, GOW3_UWP_PAD_RIGHT_SHOULDER = 0x800,
    GOW3_UWP_PAD_LEFT_THUMB = 0x1000, GOW3_UWP_PAD_RIGHT_THUMB = 0x2000,
    GOW3_UWP_PAD_PADDLE1 = 0x4000, GOW3_UWP_PAD_PADDLE2 = 0x8000, GOW3_UWP_PAD_PADDLE3 = 0x10000,
    GOW3_UWP_PAD_PADDLE4 = 0x20000,
};

typedef struct gow3_uwp_gamepad {
    int32_t connected;
    uint32_t buttons;            /* GOW3_UWP_PAD_* */
    float left_x, left_y;        /* -1..1, up is positive (Windows.Gaming.Input) */
    float right_x, right_y;
    float left_trigger, right_trigger; /* 0..1 */
} gow3_uwp_gamepad;

/* Window state bits. */
enum { GOW3_UWP_WINDOW_FOCUSED = 1, GOW3_UWP_WINDOW_VISIBLE = 2, GOW3_UWP_WINDOW_CLOSED = 4 };

typedef struct gow3_uwp_host {
    uint32_t version;            /* GOW3_UWP_HOST_VERSION */
    void *ctx;                   /* passed back to every callback */
    void *core_window;           /* ABI::Windows::UI::Core::ICoreWindow* (IUnknown) */
    void *hwnd;                  /* ICoreWindowInterop::get_WindowHandle; may be NULL */
    int32_t display_width, display_height; /* the TV's mode in pixels */
    float refresh_rate;          /* Hz, 0 when unknown */

    void (*window_size)(void *ctx, int32_t *width, int32_t *height);
    uint32_t (*window_state)(void *ctx);
    /* Gamepad `index` (0..3): returns 0 when there is none. */
    int32_t (*gamepad)(void *ctx, int32_t index, gow3_uwp_gamepad *out);
    void (*rumble)(void *ctx, int32_t index, float low, float high, uint32_t milliseconds);
    /* Keyboard events in USB HID scancodes (SDL's): returns 0 when the queue is empty. */
    int32_t (*key_event)(void *ctx, int32_t *scancode, int32_t *down);
    /* Audio output: handle > 0, or 0 when it failed. Bytes are interleaved 48 kHz frames. */
    int32_t (*audio_open)(void *ctx, int32_t channels, int32_t is_float, int32_t rate);
    int32_t (*audio_queued)(void *ctx, int32_t handle);
    void (*audio_put)(void *ctx, int32_t handle, const void *data, int32_t bytes);
    void (*audio_close)(void *ctx, int32_t handle);
    int32_t (*audio_device_channels)(void *ctx);
    /* A line for the app's log view (UTF-8). */
    void (*log)(void *ctx, const char *line);
} gow3_uwp_host;

/* Exported by gow3-runtime.dll. */
typedef int32_t (*gow3_uwp_api_version_fn)(void);
typedef void (*gow3_uwp_set_host_fn)(const gow3_uwp_host *host);
typedef int32_t (*gow3_uwp_setenv_fn)(const char *name, const char *value);
/* Runs the game on the calling thread (give it a 16 MiB stack). log_path: stdout/stderr go there. */
typedef int32_t (*gow3_uwp_run_fn)(int32_t argc, const char *const *argv, const char *log_path);

#ifdef __cplusplus
}
#endif
