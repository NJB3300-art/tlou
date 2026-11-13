// The app side of gow3_uwp_host.h: window state, Windows.Gaming.Input gamepads, the CoreWindow
// keyboard and XAudio2, for the launcher and for the runtime.
#pragma once
#include "pch.h"
#include "gow3_uwp_host.h"

namespace tlou {

class Input {
public:
    Input();
    ~Input();
    // Gamepad `index` as last polled (every 2 ms on the input thread).
    bool Gamepad(int index, gow3_uwp_gamepad& out);
    void Rumble(int index, float low, float high, uint32_t milliseconds);
    // CoreWindow keys, in USB HID scancodes (UI thread pushes, the runtime pops).
    void PushKey(winrt::Windows::System::VirtualKey key, bool down);
    bool PopKey(int32_t& scancode, int32_t& down);
    void ClearKeys();

private:
    void Run();
    std::mutex mutex;
    gow3_uwp_gamepad pads[4]{};
    struct RumbleRequest {
        float low = 0, high = 0;
        std::chrono::steady_clock::time_point until{};
        bool pending = false;
    } rumble[4];
    std::deque<std::pair<int32_t, int32_t>> keys;
    std::atomic<bool> stop{false};
    std::thread thread;
};

class Audio {
public:
    Audio();
    ~Audio();
    int32_t Open(int32_t channels, bool is_float, int32_t rate);
    int32_t Queued(int32_t handle);
    void Put(int32_t handle, const void* data, int32_t bytes);
    void Close(int32_t handle);
    int32_t DeviceChannels() const { return device_channels; }

private:
    struct Voice;
    ComPtr<IXAudio2> engine;
    IXAudio2MasteringVoice* master = nullptr;
    int32_t device_channels = 2;
    std::mutex mutex;
    std::map<int32_t, std::unique_ptr<Voice>> voices;
    int32_t next = 1;
};

// Window facts the runtime reads from its threads.
struct WindowState {
    std::atomic<int32_t> width{1920}, height{1080};
    std::atomic<bool> focused{true}, visible{true}, closed{false};
    void* core_window = nullptr;
    void* hwnd = nullptr;
    int32_t display_width = 1920, display_height = 1080;
    float refresh_rate = 60.0f;
};

// The callback table handed to gow3_uwp_set_host.
gow3_uwp_host MakeHostTable(WindowState& window, Input& input, Audio& audio);

// HID scancode of a VirtualKey, 0 for keys the runtime does not know.
int32_t ScancodeOf(winrt::Windows::System::VirtualKey key);

} // namespace tlou
