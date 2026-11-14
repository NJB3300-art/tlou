// gow3 UWP: the SDL3 stand-in (SDL3/SDL.h in this folder) over the UWP app's callbacks
// (gow3_uwp_host.h). One window (the app's CoreWindow), up to four Windows.Gaming.Input
// gamepads, the CoreWindow keyboard and XAudio2 voices behind the audio streams.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <variant>

#include <SDL3/SDL.h>
#include "../gow3_uwp_host.h"

extern "C" const gow3_uwp_host* gow3_uwp_host_get(void);

namespace {

const gow3_uwp_host* Host() {
    return gow3_uwp_host_get();
}

std::mutex g_mutex;
std::string g_error = "no error";
SDL_InitFlags g_initialized = 0;

void SetError(const char* message) {
    std::scoped_lock lock{g_mutex};
    g_error = message;
}

// ---- properties ----
using Value = std::variant<Sint64, bool, std::string, void*>;
std::map<SDL_PropertiesID, std::map<std::string, Value>> g_properties;
SDL_PropertiesID g_next_properties = 1;

// ---- window ----
struct WindowData {
    SDL_PropertiesID properties = 0;
    std::string title;
    bool text_input = false;
    int last_w = 0, last_h = 0;
    bool closed_sent = false;
};
WindowData* g_window = nullptr;
constexpr SDL_WindowID WindowId = 1;

void HostWindowSize(int* w, int* h) {
    int32_t width = 1920, height = 1080;
    if (const auto* host = Host(); host && host->window_size) {
        host->window_size(host->ctx, &width, &height);
    }
    if (w) *w = width;
    if (h) *h = height;
}

// ---- gamepads ----
struct PadSnapshot {
    gow3_uwp_gamepad state{};
    std::chrono::steady_clock::time_point when{};
};
PadSnapshot g_pads[4];
SDL_Gamepad* PadHandle(int index) {
    return reinterpret_cast<SDL_Gamepad*>(static_cast<uintptr_t>(index + 1));
}
int PadIndex(SDL_Gamepad* gamepad) {
    const auto value = reinterpret_cast<uintptr_t>(gamepad);
    return value >= 1 && value <= 4 ? int(value - 1) : -1;
}
// Snapshots are refreshed at most every millisecond: the pad code asks for every button.
gow3_uwp_gamepad ReadPad(int index) {
    gow3_uwp_gamepad out{};
    if (index < 0 || index > 3) return out;
    const auto now = std::chrono::steady_clock::now();
    std::scoped_lock lock{g_mutex};
    PadSnapshot& snapshot = g_pads[index];
    if (now - snapshot.when > std::chrono::milliseconds(1)) {
        gow3_uwp_gamepad state{};
        if (const auto* host = Host(); host && host->gamepad && host->gamepad(host->ctx, index, &state)) {
            snapshot.state = state;
        } else {
            snapshot.state = gow3_uwp_gamepad{};
        }
        snapshot.when = now;
    }
    return snapshot.state;
}

bool ButtonDown(const gow3_uwp_gamepad& pad, int button) {
    const uint32_t b = pad.buttons;
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return b & GOW3_UWP_PAD_A;
    case SDL_GAMEPAD_BUTTON_EAST: return b & GOW3_UWP_PAD_B;
    case SDL_GAMEPAD_BUTTON_WEST: return b & GOW3_UWP_PAD_X;
    case SDL_GAMEPAD_BUTTON_NORTH: return b & GOW3_UWP_PAD_Y;
    case SDL_GAMEPAD_BUTTON_BACK: return b & GOW3_UWP_PAD_VIEW;
    case SDL_GAMEPAD_BUTTON_START: return b & GOW3_UWP_PAD_MENU;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return b & GOW3_UWP_PAD_LEFT_THUMB;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return b & GOW3_UWP_PAD_RIGHT_THUMB;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return b & GOW3_UWP_PAD_LEFT_SHOULDER;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return b & GOW3_UWP_PAD_RIGHT_SHOULDER;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return b & GOW3_UWP_PAD_DPAD_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return b & GOW3_UWP_PAD_DPAD_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return b & GOW3_UWP_PAD_DPAD_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return b & GOW3_UWP_PAD_DPAD_RIGHT;
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return b & GOW3_UWP_PAD_PADDLE1;
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return b & GOW3_UWP_PAD_PADDLE2;
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return b & GOW3_UWP_PAD_PADDLE3;
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return b & GOW3_UWP_PAD_PADDLE4;
    default: return false;
    }
}

Sint16 Stick(float v, bool invert) {
    v = std::clamp(invert ? -v : v, -1.0f, 1.0f);
    return Sint16(std::lround(v * (v < 0 ? 32768.0f : 32767.0f)));
}
Sint16 AxisValue(const gow3_uwp_gamepad& pad, int axis) {
    switch (axis) {
    case SDL_GAMEPAD_AXIS_LEFTX: return Stick(pad.left_x, false);
    case SDL_GAMEPAD_AXIS_LEFTY: return Stick(pad.left_y, true); // SDL: down is positive
    case SDL_GAMEPAD_AXIS_RIGHTX: return Stick(pad.right_x, false);
    case SDL_GAMEPAD_AXIS_RIGHTY: return Stick(pad.right_y, true);
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: return Sint16(std::lround(std::clamp(pad.left_trigger, 0.0f, 1.0f) * 32767.0f));
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return Sint16(std::lround(std::clamp(pad.right_trigger, 0.0f, 1.0f) * 32767.0f));
    default: return 0;
    }
}

const char* const ButtonNames[SDL_GAMEPAD_BUTTON_COUNT] = {
    "a", "b", "x", "y", "back", "guide", "start", "leftstick", "rightstick", "leftshoulder",
    "rightshoulder", "dpup", "dpdown", "dpleft", "dpright", "misc1", "paddle1", "paddle2",
    "paddle3", "paddle4", "touchpad", "misc2", "misc3", "misc4", "misc5", "misc6"};

// ---- keyboard ----
bool g_keys[SDL_SCANCODE_COUNT];

struct ScancodeName {
    SDL_Scancode code;
    const char* name;
};
const ScancodeName ScancodeNames[] = {
    {SDL_SCANCODE_RETURN, "Return"}, {SDL_SCANCODE_ESCAPE, "Escape"},
    {SDL_SCANCODE_BACKSPACE, "Backspace"}, {SDL_SCANCODE_TAB, "Tab"}, {SDL_SCANCODE_SPACE, "Space"},
    {SDL_SCANCODE_MINUS, "-"}, {SDL_SCANCODE_EQUALS, "="}, {SDL_SCANCODE_LEFTBRACKET, "["},
    {SDL_SCANCODE_RIGHTBRACKET, "]"}, {SDL_SCANCODE_BACKSLASH, "\\"}, {SDL_SCANCODE_SEMICOLON, ";"},
    {SDL_SCANCODE_APOSTROPHE, "'"}, {SDL_SCANCODE_GRAVE, "`"}, {SDL_SCANCODE_COMMA, ","},
    {SDL_SCANCODE_PERIOD, "."}, {SDL_SCANCODE_SLASH, "/"}, {SDL_SCANCODE_CAPSLOCK, "CapsLock"},
    {SDL_SCANCODE_F1, "F1"}, {SDL_SCANCODE_F2, "F2"}, {SDL_SCANCODE_F3, "F3"}, {SDL_SCANCODE_F4, "F4"},
    {SDL_SCANCODE_F5, "F5"}, {SDL_SCANCODE_F6, "F6"}, {SDL_SCANCODE_F7, "F7"}, {SDL_SCANCODE_F8, "F8"},
    {SDL_SCANCODE_F9, "F9"}, {SDL_SCANCODE_F10, "F10"}, {SDL_SCANCODE_F11, "F11"}, {SDL_SCANCODE_F12, "F12"},
    {SDL_SCANCODE_INSERT, "Insert"}, {SDL_SCANCODE_HOME, "Home"}, {SDL_SCANCODE_PAGEUP, "PageUp"},
    {SDL_SCANCODE_DELETE, "Delete"}, {SDL_SCANCODE_END, "End"}, {SDL_SCANCODE_PAGEDOWN, "PageDown"},
    {SDL_SCANCODE_RIGHT, "Right"}, {SDL_SCANCODE_LEFT, "Left"}, {SDL_SCANCODE_DOWN, "Down"},
    {SDL_SCANCODE_UP, "Up"}, {SDL_SCANCODE_KP_ENTER, "Keypad Enter"},
    {SDL_SCANCODE_LCTRL, "Left Ctrl"}, {SDL_SCANCODE_LSHIFT, "Left Shift"}, {SDL_SCANCODE_LALT, "Left Alt"},
    {SDL_SCANCODE_LGUI, "Left GUI"}, {SDL_SCANCODE_RCTRL, "Right Ctrl"}, {SDL_SCANCODE_RSHIFT, "Right Shift"},
    {SDL_SCANCODE_RALT, "Right Alt"}, {SDL_SCANCODE_RGUI, "Right GUI"},
};

const char* ScancodeNameOf(int code) {
    static char letters[SDL_SCANCODE_0 + 1][2];
    if (code >= SDL_SCANCODE_A && code <= SDL_SCANCODE_Z) {
        letters[code][0] = char('A' + (code - SDL_SCANCODE_A));
        return letters[code];
    }
    if (code >= SDL_SCANCODE_1 && code <= SDL_SCANCODE_0) {
        letters[code][0] = code == SDL_SCANCODE_0 ? '0' : char('1' + (code - SDL_SCANCODE_1));
        return letters[code];
    }
    for (const auto& entry : ScancodeNames) {
        if (entry.code == code) return entry.name;
    }
    return "";
}

SDL_Keycode KeycodeOf(int code) {
    if (code >= SDL_SCANCODE_A && code <= SDL_SCANCODE_Z) return SDL_Keycode('a' + (code - SDL_SCANCODE_A));
    if (code >= SDL_SCANCODE_1 && code <= SDL_SCANCODE_9) return SDL_Keycode('1' + (code - SDL_SCANCODE_1));
    switch (code) {
    case SDL_SCANCODE_0: return '0';
    case SDL_SCANCODE_RETURN: return SDLK_RETURN;
    case SDL_SCANCODE_ESCAPE: return SDLK_ESCAPE;
    case SDL_SCANCODE_BACKSPACE: return SDLK_BACKSPACE;
    case SDL_SCANCODE_TAB: return SDLK_TAB;
    case SDL_SCANCODE_SPACE: return SDLK_SPACE;
    case SDL_SCANCODE_DELETE: return SDLK_DELETE;
    case SDL_SCANCODE_MINUS: return '-';
    case SDL_SCANCODE_EQUALS: return '=';
    case SDL_SCANCODE_LEFTBRACKET: return '[';
    case SDL_SCANCODE_RIGHTBRACKET: return ']';
    case SDL_SCANCODE_BACKSLASH: return '\\';
    case SDL_SCANCODE_SEMICOLON: return ';';
    case SDL_SCANCODE_APOSTROPHE: return '\'';
    case SDL_SCANCODE_GRAVE: return '`';
    case SDL_SCANCODE_COMMA: return ',';
    case SDL_SCANCODE_PERIOD: return '.';
    case SDL_SCANCODE_SLASH: return '/';
    case SDL_SCANCODE_UNKNOWN: return SDLK_UNKNOWN;
    default: return SDL_SCANCODE_TO_KEYCODE(code);
    }
}

SDL_Keymod Modifiers() {
    SDL_Keymod mod = SDL_KMOD_NONE;
    if (g_keys[SDL_SCANCODE_LSHIFT]) mod |= SDL_KMOD_LSHIFT;
    if (g_keys[SDL_SCANCODE_RSHIFT]) mod |= SDL_KMOD_RSHIFT;
    if (g_keys[SDL_SCANCODE_LCTRL]) mod |= SDL_KMOD_LCTRL;
    if (g_keys[SDL_SCANCODE_RCTRL]) mod |= SDL_KMOD_RCTRL;
    if (g_keys[SDL_SCANCODE_LALT]) mod |= SDL_KMOD_LALT;
    if (g_keys[SDL_SCANCODE_RALT]) mod |= SDL_KMOD_RALT;
    return mod;
}

// ---- events: the event pump compares the first gamepad with the last state it reported ----
std::deque<SDL_Event> g_events;
gow3_uwp_gamepad g_event_pad{};
Sint16 g_event_triggers[2]{};

void QueueEvents() {
    // Window size and close.
    if (g_window) {
        int w = 0, h = 0;
        HostWindowSize(&w, &h);
        if (w != g_window->last_w || h != g_window->last_h) {
            g_window->last_w = w;
            g_window->last_h = h;
            SDL_Event e{};
            e.window.type = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
            e.window.windowID = WindowId;
            e.window.data1 = w;
            e.window.data2 = h;
            g_events.push_back(e);
        }
        const auto* host = Host();
        if (host && host->window_state && (host->window_state(host->ctx) & GOW3_UWP_WINDOW_CLOSED) &&
            !g_window->closed_sent) {
            g_window->closed_sent = true;
            SDL_Event e{};
            e.window.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
            e.window.windowID = WindowId;
            g_events.push_back(e);
        }
    }
    // Keyboard.
    if (const auto* host = Host(); host && host->key_event) {
        int32_t code = 0, down = 0;
        for (int n = 0; n < 64 && host->key_event(host->ctx, &code, &down); ++n) {
            if (code <= 0 || code >= SDL_SCANCODE_COUNT) continue;
            const bool repeat = down && g_keys[code];
            g_keys[code] = down != 0;
            SDL_Event e{};
            e.key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            e.key.windowID = WindowId;
            e.key.scancode = SDL_Scancode(code);
            e.key.key = KeycodeOf(code);
            e.key.mod = Modifiers();
            e.key.down = down != 0;
            e.key.repeat = repeat;
            g_events.push_back(e);
        }
    }
    // The first gamepad's buttons and triggers.
    const gow3_uwp_gamepad pad = ReadPad(0);
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b) {
        const bool now = ButtonDown(pad, b), before = ButtonDown(g_event_pad, b);
        if (now != before) {
            SDL_Event e{};
            e.gbutton.type = now ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
            e.gbutton.which = 1;
            e.gbutton.button = Uint8(b);
            e.gbutton.down = now;
            g_events.push_back(e);
        }
    }
    for (int t = 0; t < 2; ++t) {
        const int axis = t ? SDL_GAMEPAD_AXIS_RIGHT_TRIGGER : SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
        const Sint16 value = AxisValue(pad, axis);
        if (std::abs(value - g_event_triggers[t]) > 256) {
            g_event_triggers[t] = value;
            SDL_Event e{};
            e.gaxis.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
            e.gaxis.which = 1;
            e.gaxis.axis = Uint8(axis);
            e.gaxis.value = value;
            g_events.push_back(e);
        }
    }
    g_event_pad = pad;
}

// ---- audio ----
struct AudioStreamData {
    int32_t handle = 0;
};

const SDL_DisplayMode* DisplayMode() {
    static SDL_DisplayMode mode{};
    const auto* host = Host();
    mode.displayID = 1;
    mode.w = host && host->display_width > 0 ? host->display_width : 1920;
    mode.h = host && host->display_height > 0 ? host->display_height : 1080;
    mode.pixel_density = 1.0f;
    mode.refresh_rate = host && host->refresh_rate > 0 ? host->refresh_rate : 60.0f;
    return &mode;
}

} // namespace

extern "C" {

// ---- init, errors, hints, misc ----
bool SDL_InitSubSystem(SDL_InitFlags flags) {
    if (!Host()) {
        SetError("the UWP app did not register its callbacks (gow3_uwp_set_host)");
        return false;
    }
    std::scoped_lock lock{g_mutex};
    g_initialized |= flags;
    return true;
}
SDL_InitFlags SDL_WasInit(SDL_InitFlags flags) {
    std::scoped_lock lock{g_mutex};
    return flags ? (g_initialized & flags) : g_initialized;
}
const char* SDL_GetError(void) {
    static thread_local std::string copy;
    std::scoped_lock lock{g_mutex};
    copy = g_error;
    return copy.c_str();
}
bool SDL_SetHint(const char*, const char*) { return true; }
void SDL_Delay(Uint32 ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
void SDL_free(void* memory) { std::free(memory); }
int SDL_strcasecmp(const char* a, const char* b) {
    if (!a || !b) return a == b ? 0 : (a ? 1 : -1);
    for (;; ++a, ++b) {
        const int x = std::tolower(static_cast<unsigned char>(*a));
        const int y = std::tolower(static_cast<unsigned char>(*b));
        if (x != y || !x) return x - y;
    }
}

// ---- properties ----
SDL_PropertiesID SDL_CreateProperties(void) {
    std::scoped_lock lock{g_mutex};
    const SDL_PropertiesID id = g_next_properties++;
    g_properties[id];
    return id;
}
void SDL_DestroyProperties(SDL_PropertiesID props) {
    std::scoped_lock lock{g_mutex};
    g_properties.erase(props);
}
static bool SetProperty(SDL_PropertiesID props, const char* name, Value value) {
    if (!name) return false;
    std::scoped_lock lock{g_mutex};
    const auto it = g_properties.find(props);
    if (it == g_properties.end()) return false;
    it->second[name] = std::move(value);
    return true;
}
bool SDL_SetStringProperty(SDL_PropertiesID props, const char* name, const char* value) {
    return SetProperty(props, name, std::string(value ? value : ""));
}
bool SDL_SetNumberProperty(SDL_PropertiesID props, const char* name, Sint64 value) {
    return SetProperty(props, name, value);
}
bool SDL_SetBooleanProperty(SDL_PropertiesID props, const char* name, bool value) {
    return SetProperty(props, name, value);
}
Sint64 SDL_GetNumberProperty(SDL_PropertiesID props, const char* name, Sint64 fallback) {
    std::scoped_lock lock{g_mutex};
    const auto it = g_properties.find(props);
    if (it == g_properties.end() || !name) return fallback;
    const auto value = it->second.find(name);
    if (value == it->second.end()) return fallback;
    if (const auto* number = std::get_if<Sint64>(&value->second)) return *number;
    return fallback;
}
void* SDL_GetPointerProperty(SDL_PropertiesID props, const char* name, void* fallback) {
    std::scoped_lock lock{g_mutex};
    const auto it = g_properties.find(props);
    if (it == g_properties.end() || !name) return fallback;
    const auto value = it->second.find(name);
    if (value == it->second.end()) return fallback;
    if (auto* const* pointer = std::get_if<void*>(&value->second)) return *pointer;
    return fallback;
}

// ---- video ----
SDL_Window* SDL_CreateWindowWithProperties(SDL_PropertiesID props) {
    const auto* host = Host();
    if (!host) {
        SetError("no UWP host");
        return nullptr;
    }
    if (g_window) {
        SetError("the UWP app has one window");
        return nullptr;
    }
    auto* window = new WindowData;
    {
        std::scoped_lock lock{g_mutex};
        const auto it = g_properties.find(props);
        if (it != g_properties.end()) {
            const auto title = it->second.find(SDL_PROP_WINDOW_CREATE_TITLE_STRING);
            if (title != it->second.end()) {
                if (const auto* text = std::get_if<std::string>(&title->second)) window->title = *text;
            }
        }
    }
    window->properties = SDL_CreateProperties();
    // Mesa's CoreWindow mode (MESA_WSI_COREWINDOW) does not use the surface's HWND; it must not be null.
    SetProperty(window->properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER,
                Value{std::in_place_type<void*>, host->hwnd ? host->hwnd : host->core_window});
    SetProperty(window->properties, SDL_PROP_WINDOW_WINRT_WINDOW_POINTER, Value{std::in_place_type<void*>, host->core_window});
    HostWindowSize(&window->last_w, &window->last_h);
    g_window = window;
    std::fprintf(stderr, "UWP: window %dx%d, CoreWindow %p, HWND %p\n", window->last_w, window->last_h,
                 host->core_window, host->hwnd);
    return reinterpret_cast<SDL_Window*>(window);
}
void SDL_DestroyWindow(SDL_Window* window) {
    auto* data = reinterpret_cast<WindowData*>(window);
    if (!data || data != g_window) return;
    SDL_DestroyProperties(data->properties);
    g_window = nullptr;
    delete data;
}
SDL_PropertiesID SDL_GetWindowProperties(SDL_Window* window) {
    auto* data = reinterpret_cast<WindowData*>(window);
    return data ? data->properties : 0;
}
SDL_Window* SDL_GetWindowFromID(SDL_WindowID id) {
    return id == WindowId ? reinterpret_cast<SDL_Window*>(g_window) : nullptr;
}
bool SDL_GetWindowSizeInPixels(SDL_Window*, int* w, int* h) {
    HostWindowSize(w, h);
    return true;
}
float SDL_GetWindowPixelDensity(SDL_Window*) { return 1.0f; }
SDL_WindowFlags SDL_GetWindowFlags(SDL_Window*) {
    const auto* host = Host();
    const uint32_t state = host && host->window_state ? host->window_state(host->ctx)
                                                      : GOW3_UWP_WINDOW_FOCUSED | GOW3_UWP_WINDOW_VISIBLE;
    SDL_WindowFlags flags = SDL_WINDOW_FULLSCREEN;
    if (state & GOW3_UWP_WINDOW_FOCUSED) flags |= SDL_WINDOW_INPUT_FOCUS;
    if (!(state & GOW3_UWP_WINDOW_VISIBLE)) flags |= SDL_WINDOW_MINIMIZED;
    return flags;
}
// The CoreWindow always fills the TV: window placement and modes have nothing to change.
bool SDL_SetWindowFullscreen(SDL_Window*, bool) { return true; }
bool SDL_SetWindowFullscreenMode(SDL_Window*, const SDL_DisplayMode*) { return true; }
bool SDL_SyncWindow(SDL_Window*) { return true; }
bool SDL_SetWindowSize(SDL_Window*, int, int) { return true; }
bool SDL_SetWindowPosition(SDL_Window*, int, int) { return true; }
bool SDL_RaiseWindow(SDL_Window*) { return true; }
bool SDL_SetWindowTitle(SDL_Window* window, const char* title) {
    if (auto* data = reinterpret_cast<WindowData*>(window)) data->title = title ? title : "";
    return true;
}
bool SDL_SetWindowIcon(SDL_Window*, SDL_Surface*) { return true; }
bool SDL_SetWindowRelativeMouseMode(SDL_Window*, bool) { return true; }
SDL_DisplayID SDL_GetDisplayForWindow(SDL_Window*) { return 1; }
SDL_DisplayID SDL_GetPrimaryDisplay(void) { return 1; }
bool SDL_GetDisplayUsableBounds(SDL_DisplayID, SDL_Rect* rect) {
    if (!rect) return false;
    const SDL_DisplayMode* mode = DisplayMode();
    *rect = SDL_Rect{0, 0, mode->w, mode->h};
    return true;
}
const SDL_DisplayMode* SDL_GetCurrentDisplayMode(SDL_DisplayID) { return DisplayMode(); }
const SDL_DisplayMode* SDL_GetDesktopDisplayMode(SDL_DisplayID) { return DisplayMode(); }
// "windows": window.cpp then hands the Vulkan surface the window handle (ICoreWindowInterop).
const char* SDL_GetCurrentVideoDriver(void) { return "windows"; }
SDL_Surface* SDL_LoadBMP(const char*) { return nullptr; }
void SDL_DestroySurface(SDL_Surface*) {}
bool SDL_HideCursor(void) { return true; }
bool SDL_StartTextInput(SDL_Window* window) {
    if (auto* data = reinterpret_cast<WindowData*>(window)) data->text_input = true;
    return true;
}
bool SDL_StopTextInput(SDL_Window* window) {
    if (auto* data = reinterpret_cast<WindowData*>(window)) data->text_input = false;
    return true;
}
bool SDL_TextInputActive(SDL_Window* window) {
    auto* data = reinterpret_cast<WindowData*>(window);
    return data && data->text_input;
}

// ---- keyboard ----
const bool* SDL_GetKeyboardState(int* numkeys) {
    if (numkeys) *numkeys = SDL_SCANCODE_COUNT;
    return g_keys;
}
SDL_Scancode SDL_GetScancodeFromName(const char* name) {
    if (!name || !*name) return SDL_SCANCODE_UNKNOWN;
    for (int code = 1; code < SDL_SCANCODE_COUNT; ++code) {
        const char* known = ScancodeNameOf(code);
        if (*known && !SDL_strcasecmp(known, name)) return SDL_Scancode(code);
    }
    return SDL_SCANCODE_UNKNOWN;
}
const char* SDL_GetScancodeName(SDL_Scancode scancode) { return ScancodeNameOf(scancode); }
SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode scancode, SDL_Keymod, bool) { return KeycodeOf(scancode); }
SDL_Keycode SDL_GetKeyFromName(const char* name) {
    const SDL_Scancode code = SDL_GetScancodeFromName(name);
    return code == SDL_SCANCODE_UNKNOWN ? SDLK_UNKNOWN : KeycodeOf(code);
}
const char* SDL_GetKeyName(SDL_Keycode key) {
    if (key == SDLK_UNKNOWN) return "";
    for (int code = 1; code < SDL_SCANCODE_COUNT; ++code) {
        if (KeycodeOf(code) == key && *ScancodeNameOf(code)) return ScancodeNameOf(code);
    }
    return "";
}

// ---- gamepads ----
SDL_JoystickID* SDL_GetGamepads(int* count) {
    auto* ids = static_cast<SDL_JoystickID*>(std::calloc(5, sizeof(SDL_JoystickID)));
    int n = 0;
    for (int i = 0; i < 4 && ids; ++i) {
        if (ReadPad(i).connected) ids[n++] = SDL_JoystickID(i + 1);
    }
    if (count) *count = n;
    return ids;
}
SDL_Gamepad* SDL_OpenGamepad(SDL_JoystickID id) {
    return id >= 1 && id <= 4 ? PadHandle(int(id) - 1) : nullptr;
}
void SDL_CloseGamepad(SDL_Gamepad*) {}
bool SDL_GamepadConnected(SDL_Gamepad* gamepad) {
    const int index = PadIndex(gamepad);
    return index >= 0 && ReadPad(index).connected;
}
const char* SDL_GetGamepadName(SDL_Gamepad* gamepad) {
    return PadIndex(gamepad) >= 0 ? "Xbox Controller" : nullptr;
}
SDL_GamepadType SDL_GetGamepadType(SDL_Gamepad* gamepad) {
    return PadIndex(gamepad) >= 0 ? SDL_GAMEPAD_TYPE_XBOXONE : SDL_GAMEPAD_TYPE_UNKNOWN;
}
SDL_GamepadType SDL_GetGamepadTypeForID(SDL_JoystickID id) {
    return id >= 1 && id <= 4 ? SDL_GAMEPAD_TYPE_XBOXONE : SDL_GAMEPAD_TYPE_UNKNOWN;
}
bool SDL_GetGamepadButton(SDL_Gamepad* gamepad, SDL_GamepadButton button) {
    const int index = PadIndex(gamepad);
    return index >= 0 && ButtonDown(ReadPad(index), button);
}
Sint16 SDL_GetGamepadAxis(SDL_Gamepad* gamepad, SDL_GamepadAxis axis) {
    const int index = PadIndex(gamepad);
    return index >= 0 ? AxisValue(ReadPad(index), axis) : 0;
}
SDL_GamepadButton SDL_GetGamepadButtonFromString(const char* name) {
    if (!name) return SDL_GAMEPAD_BUTTON_INVALID;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b) {
        if (!SDL_strcasecmp(name, ButtonNames[b])) return SDL_GamepadButton(b);
    }
    return SDL_GAMEPAD_BUTTON_INVALID;
}
const char* SDL_GetGamepadStringForButton(SDL_GamepadButton button) {
    return button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT ? ButtonNames[button] : nullptr;
}
SDL_GamepadButtonLabel SDL_GetGamepadButtonLabelForType(SDL_GamepadType type, SDL_GamepadButton button) {
    const bool playstation = type == SDL_GAMEPAD_TYPE_PS3 || type == SDL_GAMEPAD_TYPE_PS4 || type == SDL_GAMEPAD_TYPE_PS5;
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return playstation ? SDL_GAMEPAD_BUTTON_LABEL_CROSS : SDL_GAMEPAD_BUTTON_LABEL_A;
    case SDL_GAMEPAD_BUTTON_EAST: return playstation ? SDL_GAMEPAD_BUTTON_LABEL_CIRCLE : SDL_GAMEPAD_BUTTON_LABEL_B;
    case SDL_GAMEPAD_BUTTON_WEST: return playstation ? SDL_GAMEPAD_BUTTON_LABEL_SQUARE : SDL_GAMEPAD_BUTTON_LABEL_X;
    case SDL_GAMEPAD_BUTTON_NORTH: return playstation ? SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE : SDL_GAMEPAD_BUTTON_LABEL_Y;
    default: return SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
    }
}
SDL_GamepadButtonLabel SDL_GetGamepadButtonLabel(SDL_Gamepad* gamepad, SDL_GamepadButton button) {
    return SDL_GetGamepadButtonLabelForType(SDL_GetGamepadType(gamepad), button);
}
int SDL_GetNumGamepadTouchpads(SDL_Gamepad*) { return 0; }
int SDL_GetNumGamepadTouchpadFingers(SDL_Gamepad*, int) { return 0; }
bool SDL_GetGamepadTouchpadFinger(SDL_Gamepad*, int, int, bool* down, float*, float*, float*) {
    if (down) *down = false;
    return false;
}
bool SDL_RumbleGamepad(SDL_Gamepad* gamepad, Uint16 low, Uint16 high, Uint32 duration_ms) {
    const int index = PadIndex(gamepad);
    const auto* host = Host();
    if (index < 0 || !host || !host->rumble) return false;
    host->rumble(host->ctx, index, low / 65535.0f, high / 65535.0f, duration_ms);
    return true;
}
bool SDL_SetGamepadLED(SDL_Gamepad*, Uint8, Uint8, Uint8) { return false; }

// ---- events ----
bool SDL_PollEvent(SDL_Event* event) {
    if (g_events.empty()) {
        QueueEvents();
    }
    if (g_events.empty()) return false;
    if (event) *event = g_events.front();
    g_events.pop_front();
    return true;
}

// ---- audio ----
SDL_AudioStream* SDL_OpenAudioDeviceStream(SDL_AudioDeviceID, const SDL_AudioSpec* spec,
                                           SDL_AudioStreamCallback, void*) {
    const auto* host = Host();
    if (!spec || !host || !host->audio_open) {
        SetError("no UWP audio");
        return nullptr;
    }
    const int32_t handle = host->audio_open(host->ctx, spec->channels, spec->format == SDL_AUDIO_F32,
                                            spec->freq);
    if (handle <= 0) {
        SetError("XAudio2 voice could not be created");
        return nullptr;
    }
    auto* stream = new AudioStreamData;
    stream->handle = handle;
    return reinterpret_cast<SDL_AudioStream*>(stream);
}
bool SDL_ResumeAudioStreamDevice(SDL_AudioStream*) { return true; }
void SDL_DestroyAudioStream(SDL_AudioStream* stream) {
    auto* data = reinterpret_cast<AudioStreamData*>(stream);
    if (!data) return;
    if (const auto* host = Host(); host && host->audio_close) host->audio_close(host->ctx, data->handle);
    delete data;
}
int SDL_GetAudioStreamQueued(SDL_AudioStream* stream) {
    auto* data = reinterpret_cast<AudioStreamData*>(stream);
    const auto* host = Host();
    return data && host && host->audio_queued ? host->audio_queued(host->ctx, data->handle) : 0;
}
bool SDL_PutAudioStreamData(SDL_AudioStream* stream, const void* buf, int len) {
    auto* data = reinterpret_cast<AudioStreamData*>(stream);
    const auto* host = Host();
    if (!data || !host || !host->audio_put || !buf || len <= 0) return false;
    host->audio_put(host->ctx, data->handle, buf, len);
    return true;
}
bool SDL_GetAudioDeviceFormat(SDL_AudioDeviceID, SDL_AudioSpec* spec, int* sample_frames) {
    const auto* host = Host();
    if (!host) return false;
    if (spec) {
        spec->format = SDL_AUDIO_F32;
        spec->channels = host->audio_device_channels ? host->audio_device_channels(host->ctx) : 2;
        spec->freq = 48000;
    }
    if (sample_frames) *sample_frames = 480;
    return true;
}
const char* SDL_GetCurrentAudioDriver(void) { return "xaudio2"; }

} // extern "C"
