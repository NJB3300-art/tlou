/* gow3 UWP: a stand-in for the part of SDL3 the port uses.
 *
 * SDL3 has no UWP backend (it dropped WinRT), and an app container cannot create Win32 windows.
 * In the UWP build this header takes the place of <SDL3/SDL.h>: the same names, types and the
 * values that the port stores or shifts (gamepad buttons, scancodes, keycodes), implemented in
 * uwp_sdl.cpp over the callbacks the UWP app provides (gow3_uwp_host.h). Only what the port calls
 * is here. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t Uint8;
typedef int16_t Sint16;
typedef uint16_t Uint16;
typedef int32_t Sint32;
typedef uint32_t Uint32;
typedef int64_t Sint64;
typedef uint64_t Uint64;

/* ---- init, errors, hints, misc ---- */
typedef Uint32 SDL_InitFlags;
#define SDL_INIT_AUDIO 0x00000010u
#define SDL_INIT_VIDEO 0x00000020u
#define SDL_INIT_GAMEPAD 0x00002000u
bool SDL_InitSubSystem(SDL_InitFlags flags);
SDL_InitFlags SDL_WasInit(SDL_InitFlags flags);
const char *SDL_GetError(void);
bool SDL_SetHint(const char *name, const char *value);
#define SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES "SDL_AUDIO_DEVICE_SAMPLE_FRAMES"
#define SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS"
#define SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS "SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS"
#define SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN "SDL_WINDOW_ACTIVATE_WHEN_SHOWN"
void SDL_Delay(Uint32 ms);
void SDL_free(void *memory);
int SDL_strcasecmp(const char *a, const char *b);

/* ---- properties ---- */
typedef Uint32 SDL_PropertiesID;
SDL_PropertiesID SDL_CreateProperties(void);
void SDL_DestroyProperties(SDL_PropertiesID props);
bool SDL_SetStringProperty(SDL_PropertiesID props, const char *name, const char *value);
bool SDL_SetNumberProperty(SDL_PropertiesID props, const char *name, Sint64 value);
bool SDL_SetBooleanProperty(SDL_PropertiesID props, const char *name, bool value);
Sint64 SDL_GetNumberProperty(SDL_PropertiesID props, const char *name, Sint64 fallback);
void *SDL_GetPointerProperty(SDL_PropertiesID props, const char *name, void *fallback);
#define SDL_PROP_WINDOW_CREATE_TITLE_STRING "SDL.window.create.title"
#define SDL_PROP_WINDOW_CREATE_X_NUMBER "SDL.window.create.x"
#define SDL_PROP_WINDOW_CREATE_Y_NUMBER "SDL.window.create.y"
#define SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER "SDL.window.create.width"
#define SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER "SDL.window.create.height"
#define SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN "SDL.window.create.resizable"
#define SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN "SDL.window.create.vulkan"
#define SDL_PROP_WINDOW_WIN32_HWND_POINTER "SDL.window.win32.hwnd"
#define SDL_PROP_WINDOW_X11_DISPLAY_POINTER "SDL.window.x11.display"
#define SDL_PROP_WINDOW_X11_WINDOW_NUMBER "SDL.window.x11.window"
#define SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER "SDL.window.wayland.display"
#define SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER "SDL.window.wayland.surface"
/* Not SDL's: the CoreWindow (IUnknown*) of the UWP app, for a Vulkan driver that takes one. */
#define SDL_PROP_WINDOW_WINRT_WINDOW_POINTER "SDL.window.winrt.window"

/* ---- video ---- */
typedef struct SDL_Window SDL_Window;
typedef Uint32 SDL_WindowID;
typedef Uint32 SDL_DisplayID;
typedef Uint64 SDL_WindowFlags;
#define SDL_WINDOW_FULLSCREEN 0x0000000000000001ull
#define SDL_WINDOW_MINIMIZED 0x0000000000000040ull
#define SDL_WINDOW_INPUT_FOCUS 0x0000000000000200ull
#define SDL_WINDOWPOS_CENTERED 0x2FFF0000u
typedef struct SDL_Rect { int x, y, w, h; } SDL_Rect;
typedef Uint32 SDL_PixelFormat;
typedef struct SDL_DisplayMode {
    SDL_DisplayID displayID;
    SDL_PixelFormat format;
    int w, h;
    float pixel_density;
    float refresh_rate;
    int refresh_rate_numerator, refresh_rate_denominator;
    void *internal;
} SDL_DisplayMode;
typedef struct SDL_Surface SDL_Surface;

SDL_Window *SDL_CreateWindowWithProperties(SDL_PropertiesID props);
void SDL_DestroyWindow(SDL_Window *window);
SDL_PropertiesID SDL_GetWindowProperties(SDL_Window *window);
SDL_Window *SDL_GetWindowFromID(SDL_WindowID id);
bool SDL_GetWindowSizeInPixels(SDL_Window *window, int *w, int *h);
float SDL_GetWindowPixelDensity(SDL_Window *window);
SDL_WindowFlags SDL_GetWindowFlags(SDL_Window *window);
bool SDL_SetWindowFullscreen(SDL_Window *window, bool fullscreen);
bool SDL_SetWindowFullscreenMode(SDL_Window *window, const SDL_DisplayMode *mode);
bool SDL_SyncWindow(SDL_Window *window);
bool SDL_SetWindowSize(SDL_Window *window, int w, int h);
bool SDL_SetWindowPosition(SDL_Window *window, int x, int y);
bool SDL_RaiseWindow(SDL_Window *window);
bool SDL_SetWindowTitle(SDL_Window *window, const char *title);
bool SDL_SetWindowIcon(SDL_Window *window, SDL_Surface *icon);
bool SDL_SetWindowRelativeMouseMode(SDL_Window *window, bool enabled);
SDL_DisplayID SDL_GetDisplayForWindow(SDL_Window *window);
SDL_DisplayID SDL_GetPrimaryDisplay(void);
bool SDL_GetDisplayUsableBounds(SDL_DisplayID display, SDL_Rect *rect);
const SDL_DisplayMode *SDL_GetCurrentDisplayMode(SDL_DisplayID display);
const SDL_DisplayMode *SDL_GetDesktopDisplayMode(SDL_DisplayID display);
const char *SDL_GetCurrentVideoDriver(void);
SDL_Surface *SDL_LoadBMP(const char *file);
void SDL_DestroySurface(SDL_Surface *surface);
bool SDL_HideCursor(void);
bool SDL_StartTextInput(SDL_Window *window);
bool SDL_StopTextInput(SDL_Window *window);
bool SDL_TextInputActive(SDL_Window *window);

/* ---- keyboard (USB HID scancodes and SDL3 keycodes) ---- */
typedef enum SDL_Scancode {
    SDL_SCANCODE_UNKNOWN = 0,
    SDL_SCANCODE_A = 4, SDL_SCANCODE_B = 5, SDL_SCANCODE_C = 6, SDL_SCANCODE_D = 7, SDL_SCANCODE_E = 8,
    SDL_SCANCODE_F = 9, SDL_SCANCODE_G = 10, SDL_SCANCODE_H = 11, SDL_SCANCODE_I = 12, SDL_SCANCODE_J = 13,
    SDL_SCANCODE_K = 14, SDL_SCANCODE_L = 15, SDL_SCANCODE_M = 16, SDL_SCANCODE_N = 17, SDL_SCANCODE_O = 18,
    SDL_SCANCODE_P = 19, SDL_SCANCODE_Q = 20, SDL_SCANCODE_R = 21, SDL_SCANCODE_S = 22, SDL_SCANCODE_T = 23,
    SDL_SCANCODE_U = 24, SDL_SCANCODE_V = 25, SDL_SCANCODE_W = 26, SDL_SCANCODE_X = 27, SDL_SCANCODE_Y = 28,
    SDL_SCANCODE_Z = 29,
    SDL_SCANCODE_1 = 30, SDL_SCANCODE_2 = 31, SDL_SCANCODE_3 = 32, SDL_SCANCODE_4 = 33, SDL_SCANCODE_5 = 34,
    SDL_SCANCODE_6 = 35, SDL_SCANCODE_7 = 36, SDL_SCANCODE_8 = 37, SDL_SCANCODE_9 = 38, SDL_SCANCODE_0 = 39,
    SDL_SCANCODE_RETURN = 40, SDL_SCANCODE_ESCAPE = 41, SDL_SCANCODE_BACKSPACE = 42, SDL_SCANCODE_TAB = 43,
    SDL_SCANCODE_SPACE = 44, SDL_SCANCODE_MINUS = 45, SDL_SCANCODE_EQUALS = 46,
    SDL_SCANCODE_LEFTBRACKET = 47, SDL_SCANCODE_RIGHTBRACKET = 48, SDL_SCANCODE_BACKSLASH = 49,
    SDL_SCANCODE_SEMICOLON = 51, SDL_SCANCODE_APOSTROPHE = 52, SDL_SCANCODE_GRAVE = 53,
    SDL_SCANCODE_COMMA = 54, SDL_SCANCODE_PERIOD = 55, SDL_SCANCODE_SLASH = 56, SDL_SCANCODE_CAPSLOCK = 57,
    SDL_SCANCODE_F1 = 58, SDL_SCANCODE_F2 = 59, SDL_SCANCODE_F3 = 60, SDL_SCANCODE_F4 = 61,
    SDL_SCANCODE_F5 = 62, SDL_SCANCODE_F6 = 63, SDL_SCANCODE_F7 = 64, SDL_SCANCODE_F8 = 65,
    SDL_SCANCODE_F9 = 66, SDL_SCANCODE_F10 = 67, SDL_SCANCODE_F11 = 68, SDL_SCANCODE_F12 = 69,
    SDL_SCANCODE_INSERT = 73, SDL_SCANCODE_HOME = 74, SDL_SCANCODE_PAGEUP = 75, SDL_SCANCODE_DELETE = 76,
    SDL_SCANCODE_END = 77, SDL_SCANCODE_PAGEDOWN = 78, SDL_SCANCODE_RIGHT = 79, SDL_SCANCODE_LEFT = 80,
    SDL_SCANCODE_DOWN = 81, SDL_SCANCODE_UP = 82, SDL_SCANCODE_KP_ENTER = 88,
    SDL_SCANCODE_LCTRL = 224, SDL_SCANCODE_LSHIFT = 225, SDL_SCANCODE_LALT = 226, SDL_SCANCODE_LGUI = 227,
    SDL_SCANCODE_RCTRL = 228, SDL_SCANCODE_RSHIFT = 229, SDL_SCANCODE_RALT = 230, SDL_SCANCODE_RGUI = 231,
    SDL_SCANCODE_COUNT = 512
} SDL_Scancode;

typedef Uint32 SDL_Keycode;
#define SDLK_SCANCODE_MASK (1u << 30)
#define SDL_SCANCODE_TO_KEYCODE(X) ((SDL_Keycode)(X) | SDLK_SCANCODE_MASK)
#define SDLK_UNKNOWN 0x00000000u
#define SDLK_RETURN 0x0000000du
#define SDLK_ESCAPE 0x0000001bu
#define SDLK_BACKSPACE 0x00000008u
#define SDLK_TAB 0x00000009u
#define SDLK_SPACE 0x00000020u
#define SDLK_DELETE 0x0000007fu
#define SDLK_E 0x00000065u
#define SDLK_Q 0x00000071u
#define SDLK_S 0x00000073u
#define SDLK_W 0x00000077u
#define SDLK_F11 SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F11)
#define SDLK_INSERT SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_INSERT)
#define SDLK_HOME SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_HOME)
#define SDLK_PAGEUP SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_PAGEUP)
#define SDLK_END SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_END)
#define SDLK_PAGEDOWN SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_PAGEDOWN)
#define SDLK_RIGHT SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RIGHT)
#define SDLK_LEFT SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LEFT)
#define SDLK_DOWN SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_DOWN)
#define SDLK_UP SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_UP)
#define SDLK_KP_ENTER SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_ENTER)
#define SDLK_LCTRL SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LCTRL)
#define SDLK_LSHIFT SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LSHIFT)
#define SDLK_LALT SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LALT)
#define SDLK_RCTRL SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RCTRL)
#define SDLK_RSHIFT SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RSHIFT)
#define SDLK_RALT SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RALT)

typedef Uint16 SDL_Keymod;
#define SDL_KMOD_NONE 0x0000u
#define SDL_KMOD_LSHIFT 0x0001u
#define SDL_KMOD_RSHIFT 0x0002u
#define SDL_KMOD_LCTRL 0x0040u
#define SDL_KMOD_RCTRL 0x0080u
#define SDL_KMOD_LALT 0x0100u
#define SDL_KMOD_RALT 0x0200u
#define SDL_KMOD_CTRL (SDL_KMOD_LCTRL | SDL_KMOD_RCTRL)
#define SDL_KMOD_SHIFT (SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT)
#define SDL_KMOD_ALT (SDL_KMOD_LALT | SDL_KMOD_RALT)

const bool *SDL_GetKeyboardState(int *numkeys);
SDL_Keycode SDL_GetKeyFromName(const char *name);
SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode scancode, SDL_Keymod modstate, bool key_event);
const char *SDL_GetKeyName(SDL_Keycode key);
SDL_Scancode SDL_GetScancodeFromName(const char *name);
const char *SDL_GetScancodeName(SDL_Scancode scancode);

/* ---- gamepads (SDL3's enum values: the port keeps them in bit masks) ---- */
typedef Uint32 SDL_JoystickID;
typedef struct SDL_Gamepad SDL_Gamepad;
typedef enum SDL_GamepadType {
    SDL_GAMEPAD_TYPE_UNKNOWN = 0, SDL_GAMEPAD_TYPE_STANDARD, SDL_GAMEPAD_TYPE_XBOX360,
    SDL_GAMEPAD_TYPE_XBOXONE, SDL_GAMEPAD_TYPE_PS3, SDL_GAMEPAD_TYPE_PS4, SDL_GAMEPAD_TYPE_PS5,
    SDL_GAMEPAD_TYPE_COUNT = 11
} SDL_GamepadType;
typedef enum SDL_GamepadButton {
    SDL_GAMEPAD_BUTTON_INVALID = -1,
    SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
    SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE, SDL_GAMEPAD_BUTTON_START,
    SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT,
    SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_MISC1,
    SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
    SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,
    SDL_GAMEPAD_BUTTON_TOUCHPAD, SDL_GAMEPAD_BUTTON_MISC2, SDL_GAMEPAD_BUTTON_MISC3,
    SDL_GAMEPAD_BUTTON_MISC4, SDL_GAMEPAD_BUTTON_MISC5, SDL_GAMEPAD_BUTTON_MISC6,
    SDL_GAMEPAD_BUTTON_COUNT
} SDL_GamepadButton;
typedef enum SDL_GamepadButtonLabel {
    SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN, SDL_GAMEPAD_BUTTON_LABEL_A, SDL_GAMEPAD_BUTTON_LABEL_B,
    SDL_GAMEPAD_BUTTON_LABEL_X, SDL_GAMEPAD_BUTTON_LABEL_Y, SDL_GAMEPAD_BUTTON_LABEL_CROSS,
    SDL_GAMEPAD_BUTTON_LABEL_CIRCLE, SDL_GAMEPAD_BUTTON_LABEL_SQUARE, SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE
} SDL_GamepadButtonLabel;
typedef enum SDL_GamepadAxis {
    SDL_GAMEPAD_AXIS_INVALID = -1, SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY,
    SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY, SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
    SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_GAMEPAD_AXIS_COUNT
} SDL_GamepadAxis;

SDL_JoystickID *SDL_GetGamepads(int *count);
SDL_Gamepad *SDL_OpenGamepad(SDL_JoystickID id);
void SDL_CloseGamepad(SDL_Gamepad *gamepad);
bool SDL_GamepadConnected(SDL_Gamepad *gamepad);
const char *SDL_GetGamepadName(SDL_Gamepad *gamepad);
SDL_GamepadType SDL_GetGamepadType(SDL_Gamepad *gamepad);
SDL_GamepadType SDL_GetGamepadTypeForID(SDL_JoystickID id);
bool SDL_GetGamepadButton(SDL_Gamepad *gamepad, SDL_GamepadButton button);
Sint16 SDL_GetGamepadAxis(SDL_Gamepad *gamepad, SDL_GamepadAxis axis);
SDL_GamepadButton SDL_GetGamepadButtonFromString(const char *name);
const char *SDL_GetGamepadStringForButton(SDL_GamepadButton button);
SDL_GamepadButtonLabel SDL_GetGamepadButtonLabel(SDL_Gamepad *gamepad, SDL_GamepadButton button);
SDL_GamepadButtonLabel SDL_GetGamepadButtonLabelForType(SDL_GamepadType type, SDL_GamepadButton button);
int SDL_GetNumGamepadTouchpads(SDL_Gamepad *gamepad);
int SDL_GetNumGamepadTouchpadFingers(SDL_Gamepad *gamepad, int touchpad);
bool SDL_GetGamepadTouchpadFinger(SDL_Gamepad *gamepad, int touchpad, int finger, bool *down,
                                  float *x, float *y, float *pressure);
bool SDL_RumbleGamepad(SDL_Gamepad *gamepad, Uint16 low, Uint16 high, Uint32 duration_ms);
bool SDL_SetGamepadLED(SDL_Gamepad *gamepad, Uint8 red, Uint8 green, Uint8 blue);

/* ---- mouse ---- */
#define SDL_BUTTON_LEFT 1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT 3
typedef Uint32 SDL_MouseID;

/* ---- events ---- */
typedef enum SDL_EventType {
    SDL_EVENT_FIRST = 0, SDL_EVENT_QUIT = 0x100,
    SDL_EVENT_WINDOW_RESIZED = 0x206, SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED = 0x207,
    SDL_EVENT_WINDOW_CLOSE_REQUESTED = 0x210,
    SDL_EVENT_KEY_DOWN = 0x300, SDL_EVENT_KEY_UP = 0x301, SDL_EVENT_TEXT_INPUT = 0x303,
    SDL_EVENT_MOUSE_MOTION = 0x400, SDL_EVENT_MOUSE_BUTTON_DOWN = 0x401,
    SDL_EVENT_MOUSE_BUTTON_UP = 0x402, SDL_EVENT_MOUSE_WHEEL = 0x403,
    SDL_EVENT_GAMEPAD_AXIS_MOTION = 0x650, SDL_EVENT_GAMEPAD_BUTTON_DOWN = 0x651,
    SDL_EVENT_GAMEPAD_BUTTON_UP = 0x652
} SDL_EventType;
typedef struct SDL_CommonEvent { Uint32 type; Uint32 reserved; Uint64 timestamp; } SDL_CommonEvent;
typedef struct SDL_KeyboardEvent {
    Uint32 type; Uint32 reserved; Uint64 timestamp; SDL_WindowID windowID; Uint32 which;
    SDL_Scancode scancode; SDL_Keycode key; SDL_Keymod mod; Uint16 raw; bool down; bool repeat;
} SDL_KeyboardEvent;
typedef struct SDL_TextInputEvent {
    Uint32 type; Uint32 reserved; Uint64 timestamp; SDL_WindowID windowID; const char *text;
} SDL_TextInputEvent;
typedef struct SDL_MouseMotionEvent {
    Uint32 type; Uint32 reserved; Uint64 timestamp; SDL_WindowID windowID; SDL_MouseID which;
    Uint32 state; float x, y, xrel, yrel;
} SDL_MouseMotionEvent;
typedef struct SDL_MouseButtonEvent {
    Uint32 type; Uint32 reserved; Uint64 timestamp; SDL_WindowID windowID; SDL_MouseID which;
    Uint8 button; bool down; Uint8 clicks; Uint8 padding; float x, y;
} SDL_MouseButtonEvent;
typedef struct SDL_MouseWheelEvent {
    Uint32 type; Uint32 reserved; Uint64 timestamp; SDL_WindowID windowID; SDL_MouseID which;
    float x, y; Uint32 direction; float mouse_x, mouse_y;
} SDL_MouseWheelEvent;
typedef struct SDL_GamepadAxisEvent {
    Uint32 type; Uint32 reserved; Uint64 timestamp; SDL_JoystickID which; Uint8 axis;
    Uint8 padding1, padding2, padding3; Sint16 value; Uint16 padding4;
} SDL_GamepadAxisEvent;
typedef struct SDL_GamepadButtonEvent {
    Uint32 type; Uint32 reserved; Uint64 timestamp; SDL_JoystickID which; Uint8 button; bool down;
    Uint8 padding1, padding2;
} SDL_GamepadButtonEvent;
typedef struct SDL_WindowEvent {
    Uint32 type; Uint32 reserved; Uint64 timestamp; SDL_WindowID windowID; Sint32 data1, data2;
} SDL_WindowEvent;
typedef union SDL_Event {
    Uint32 type;
    SDL_CommonEvent common;
    SDL_WindowEvent window;
    SDL_KeyboardEvent key;
    SDL_TextInputEvent text;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_MouseWheelEvent wheel;
    SDL_GamepadAxisEvent gaxis;
    SDL_GamepadButtonEvent gbutton;
    Uint8 padding[128];
} SDL_Event;
bool SDL_PollEvent(SDL_Event *event);

/* ---- audio ---- */
typedef Uint32 SDL_AudioDeviceID;
typedef Uint32 SDL_AudioFormat;
#define SDL_AUDIO_S16 0x8010u
#define SDL_AUDIO_F32 0x8120u
#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK ((SDL_AudioDeviceID)0xFFFFFFFFu)
typedef struct SDL_AudioSpec { SDL_AudioFormat format; int channels; int freq; } SDL_AudioSpec;
typedef struct SDL_AudioStream SDL_AudioStream;
typedef void (*SDL_AudioStreamCallback)(void *userdata, SDL_AudioStream *stream, int additional, int total);
SDL_AudioStream *SDL_OpenAudioDeviceStream(SDL_AudioDeviceID device, const SDL_AudioSpec *spec,
                                           SDL_AudioStreamCallback callback, void *userdata);
bool SDL_ResumeAudioStreamDevice(SDL_AudioStream *stream);
void SDL_DestroyAudioStream(SDL_AudioStream *stream);
int SDL_GetAudioStreamQueued(SDL_AudioStream *stream);
bool SDL_PutAudioStreamData(SDL_AudioStream *stream, const void *buf, int len);
bool SDL_GetAudioDeviceFormat(SDL_AudioDeviceID device, SDL_AudioSpec *spec, int *sample_frames);
const char *SDL_GetCurrentAudioDriver(void);

#ifdef __cplusplus
}
#endif
