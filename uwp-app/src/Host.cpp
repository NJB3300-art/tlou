#include "pch.h"
#include "Host.h"

using namespace winrt;
using namespace winrt::Windows::Gaming::Input;
using winrt::Windows::System::VirtualKey;

namespace tlou {

// ---- input ----

Input::Input() {
    thread = std::thread([this] { Run(); });
}

Input::~Input() {
    stop = true;
    if (thread.joinable()) thread.join();
}

void Input::Run() {
    // An MTA thread of its own: Windows.Gaming.Input is read here, never on the runtime's threads.
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    std::vector<winrt::Windows::Gaming::Input::Gamepad> list;
    auto refreshed = std::chrono::steady_clock::time_point{};
    while (!stop) {
        const auto now = std::chrono::steady_clock::now();
        if (now - refreshed > std::chrono::milliseconds(500)) {
            list.clear();
            try {
                for (const auto& pad : winrt::Windows::Gaming::Input::Gamepad::Gamepads()) {
                    if (list.size() < 4) list.push_back(pad);
                }
            } catch (...) {
            }
            refreshed = now;
        }
        gow3_uwp_gamepad read[4]{};
        for (size_t i = 0; i < list.size(); ++i) {
            try {
                const GamepadReading r = list[i].GetCurrentReading();
                read[i].connected = 1;
                read[i].buttons = static_cast<uint32_t>(r.Buttons);
                read[i].left_x = float(r.LeftThumbstickX);
                read[i].left_y = float(r.LeftThumbstickY);
                read[i].right_x = float(r.RightThumbstickX);
                read[i].right_y = float(r.RightThumbstickY);
                read[i].left_trigger = float(r.LeftTrigger);
                read[i].right_trigger = float(r.RightTrigger);
            } catch (...) {
                read[i].connected = 0;
            }
        }
        RumbleRequest requests[4];
        {
            std::scoped_lock lock{mutex};
            std::copy(std::begin(read), std::end(read), std::begin(pads));
            for (int i = 0; i < 4; ++i) {
                requests[i] = rumble[i];
                rumble[i].pending = false;
                if (rumble[i].low + rumble[i].high > 0 && now >= rumble[i].until) {
                    rumble[i] = RumbleRequest{0, 0, {}, true}; // the duration ran out
                }
            }
        }
        for (size_t i = 0; i < list.size(); ++i) {
            if (!requests[i].pending) continue;
            try {
                GamepadVibration v{};
                v.LeftMotor = requests[i].low;
                v.RightMotor = requests[i].high;
                list[i].Vibration(v);
            } catch (...) {
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

bool Input::Gamepad(int index, gow3_uwp_gamepad& out) {
    if (index < 0 || index > 3) return false;
    std::scoped_lock lock{mutex};
    out = pads[index];
    return out.connected != 0;
}

void Input::Rumble(int index, float low, float high, uint32_t milliseconds) {
    if (index < 0 || index > 3) return;
    std::scoped_lock lock{mutex};
    rumble[index].low = std::clamp(low, 0.0f, 1.0f);
    rumble[index].high = std::clamp(high, 0.0f, 1.0f);
    rumble[index].until = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(milliseconds ? milliseconds : 1000);
    rumble[index].pending = true;
}

int32_t ScancodeOf(VirtualKey key) {
    const int vk = static_cast<int>(key);
    if (vk >= 'A' && vk <= 'Z') return 4 + (vk - 'A');
    if (vk >= '1' && vk <= '9') return 30 + (vk - '1');
    if (vk == '0') return 39;
    if (vk >= 0x70 && vk <= 0x7B) return 58 + (vk - 0x70); // F1-F12
    switch (vk) {
    case 0x0D: return 40;  // Enter
    case 0x1B: return 41;  // Escape
    case 0x08: return 42;  // Back
    case 0x09: return 43;  // Tab
    case 0x20: return 44;  // Space
    case 0xBD: return 45;  // -
    case 0xBB: return 46;  // =
    case 0xDB: return 47;  // [
    case 0xDD: return 48;  // ]
    case 0xDC: return 49;  // backslash
    case 0xBA: return 51;  // ;
    case 0xDE: return 52;  // '
    case 0xC0: return 53;  // `
    case 0xBC: return 54;  // ,
    case 0xBE: return 55;  // .
    case 0xBF: return 56;  // /
    case 0x14: return 57;  // CapsLock
    case 0x2D: return 73;  // Insert
    case 0x24: return 74;  // Home
    case 0x21: return 75;  // PageUp
    case 0x2E: return 76;  // Delete
    case 0x23: return 77;  // End
    case 0x22: return 78;  // PageDown
    case 0x27: return 79;  // Right
    case 0x25: return 80;  // Left
    case 0x28: return 81;  // Down
    case 0x26: return 82;  // Up
    case 0x11: case 0xA2: return 224; // Ctrl, Left Ctrl
    case 0x10: case 0xA0: return 225; // Shift, Left Shift
    case 0x12: case 0xA4: return 226; // Alt, Left Alt
    case 0xA3: return 228; // Right Ctrl
    case 0xA1: return 229; // Right Shift
    case 0xA5: return 230; // Right Alt
    default: return 0;     // the gamepad's own VirtualKeys (0xC3+) come through Gaming.Input
    }
}

void Input::PushKey(VirtualKey key, bool down) {
    const int32_t code = ScancodeOf(key);
    if (!code) return;
    std::scoped_lock lock{mutex};
    if (keys.size() < 256) keys.emplace_back(code, down ? 1 : 0);
}

bool Input::PopKey(int32_t& scancode, int32_t& down) {
    std::scoped_lock lock{mutex};
    if (keys.empty()) return false;
    scancode = keys.front().first;
    down = keys.front().second;
    keys.pop_front();
    return true;
}

void Input::ClearKeys() {
    std::scoped_lock lock{mutex};
    keys.clear();
}

// ---- audio ----

namespace {
// ksmedia.h's subformats, without that header.
constexpr GUID SubtypePcm{0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
constexpr GUID SubtypeFloat{0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
constexpr DWORD Speakers51 = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER |
                             SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
constexpr DWORD Speakers71 = Speakers51 | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
} // namespace

struct Audio::Voice : IXAudio2VoiceCallback {
    IXAudio2SourceVoice* source = nullptr;
    std::atomic<int64_t> queued{0};

    struct Buffer {
        std::vector<uint8_t> bytes;
    };

    void STDMETHODCALLTYPE OnBufferEnd(void* context) noexcept override {
        auto* buffer = static_cast<Buffer*>(context);
        queued -= int64_t(buffer->bytes.size());
        delete buffer;
    }
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) noexcept override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() noexcept override {}
    void STDMETHODCALLTYPE OnStreamEnd() noexcept override {}
    void STDMETHODCALLTYPE OnBufferStart(void*) noexcept override {}
    void STDMETHODCALLTYPE OnLoopEnd(void*) noexcept override {}
    void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) noexcept override {}
};

Audio::Audio() {
    if (FAILED(XAudio2Create(engine.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR))) {
        engine.Reset();
        return;
    }
    if (FAILED(engine->CreateMasteringVoice(&master, XAUDIO2_DEFAULT_CHANNELS, 48000))) {
        master = nullptr;
        engine.Reset();
        return;
    }
    XAUDIO2_VOICE_DETAILS details{};
    master->GetVoiceDetails(&details);
    device_channels = int32_t(details.InputChannels);
}

Audio::~Audio() {
    {
        std::scoped_lock lock{mutex};
        for (auto& [handle, voice] : voices) {
            if (voice->source) voice->source->DestroyVoice();
        }
        voices.clear();
    }
    if (master) master->DestroyVoice();
    engine.Reset();
}

int32_t Audio::Open(int32_t channels, bool is_float, int32_t rate) {
    if (!engine || channels <= 0 || channels > 8) return 0;
    WAVEFORMATEXTENSIBLE format{};
    format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    format.Format.nChannels = WORD(channels);
    format.Format.nSamplesPerSec = DWORD(rate > 0 ? rate : 48000);
    format.Format.wBitsPerSample = is_float ? 32 : 16;
    format.Format.nBlockAlign = WORD(channels * format.Format.wBitsPerSample / 8);
    format.Format.nAvgBytesPerSec = format.Format.nSamplesPerSec * format.Format.nBlockAlign;
    format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    format.Samples.wValidBitsPerSample = format.Format.wBitsPerSample;
    // SDL's channel orders, which the runtime writes: FL FR (FC LFE) (BL BR) (SL SR).
    switch (channels) {
    case 1: format.dwChannelMask = SPEAKER_FRONT_CENTER; break;
    case 2: format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT; break;
    case 6: format.dwChannelMask = Speakers51; break;
    case 8: format.dwChannelMask = Speakers71; break;
    default: format.dwChannelMask = 0; break;
    }
    format.SubFormat = is_float ? SubtypeFloat : SubtypePcm;
    auto voice = std::make_unique<Voice>();
    if (FAILED(engine->CreateSourceVoice(&voice->source, &format.Format, 0, 2.0f, voice.get()))) {
        return 0;
    }
    voice->source->Start(0);
    std::scoped_lock lock{mutex};
    const int32_t handle = next++;
    voices[handle] = std::move(voice);
    return handle;
}

int32_t Audio::Queued(int32_t handle) {
    std::scoped_lock lock{mutex};
    const auto it = voices.find(handle);
    return it == voices.end() ? 0 : int32_t(std::max<int64_t>(0, it->second->queued.load()));
}

void Audio::Put(int32_t handle, const void* data, int32_t bytes) {
    if (!data || bytes <= 0) return;
    std::scoped_lock lock{mutex};
    const auto it = voices.find(handle);
    if (it == voices.end()) return;
    auto* buffer = new Voice::Buffer;
    buffer->bytes.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + bytes);
    XAUDIO2_BUFFER submit{};
    submit.AudioBytes = UINT32(buffer->bytes.size());
    submit.pAudioData = buffer->bytes.data();
    submit.pContext = buffer;
    it->second->queued += bytes;
    if (FAILED(it->second->source->SubmitSourceBuffer(&submit))) {
        it->second->queued -= bytes;
        delete buffer;
    }
}

void Audio::Close(int32_t handle) {
    std::unique_ptr<Voice> voice;
    {
        std::scoped_lock lock{mutex};
        const auto it = voices.find(handle);
        if (it == voices.end()) return;
        voice = std::move(it->second);
        voices.erase(it);
    }
    // DestroyVoice waits for the voice's callbacks; the queued buffers are released with it.
    if (voice->source) {
        voice->source->Stop(0);
        voice->source->FlushSourceBuffers();
        voice->source->DestroyVoice();
    }
}

// ---- the callback table ----

namespace {
struct HostContext {
    WindowState* window;
    Input* input;
    Audio* audio;
};
HostContext g_context;
} // namespace

gow3_uwp_host MakeHostTable(WindowState& window, Input& input, Audio& audio) {
    g_context = HostContext{&window, &input, &audio};
    gow3_uwp_host host{};
    host.version = GOW3_UWP_HOST_VERSION;
    host.ctx = &g_context;
    host.core_window = window.core_window;
    host.hwnd = window.hwnd;
    host.display_width = window.display_width;
    host.display_height = window.display_height;
    host.refresh_rate = window.refresh_rate;
    host.window_size = [](void* ctx, int32_t* w, int32_t* h) {
        auto* c = static_cast<HostContext*>(ctx);
        if (w) *w = c->window->width;
        if (h) *h = c->window->height;
    };
    host.window_state = [](void* ctx) -> uint32_t {
        auto* c = static_cast<HostContext*>(ctx);
        return (c->window->focused ? GOW3_UWP_WINDOW_FOCUSED : 0u) |
               (c->window->visible ? GOW3_UWP_WINDOW_VISIBLE : 0u) |
               (c->window->closed ? GOW3_UWP_WINDOW_CLOSED : 0u);
    };
    host.gamepad = [](void* ctx, int32_t index, gow3_uwp_gamepad* out) -> int32_t {
        auto* c = static_cast<HostContext*>(ctx);
        return out && c->input->Gamepad(index, *out) ? 1 : 0;
    };
    host.rumble = [](void* ctx, int32_t index, float low, float high, uint32_t ms) {
        static_cast<HostContext*>(ctx)->input->Rumble(index, low, high, ms);
    };
    host.key_event = [](void* ctx, int32_t* scancode, int32_t* down) -> int32_t {
        int32_t s = 0, d = 0;
        if (!static_cast<HostContext*>(ctx)->input->PopKey(s, d)) return 0;
        if (scancode) *scancode = s;
        if (down) *down = d;
        return 1;
    };
    host.audio_open = [](void* ctx, int32_t channels, int32_t is_float, int32_t rate) -> int32_t {
        return static_cast<HostContext*>(ctx)->audio->Open(channels, is_float != 0, rate);
    };
    host.audio_queued = [](void* ctx, int32_t handle) -> int32_t {
        return static_cast<HostContext*>(ctx)->audio->Queued(handle);
    };
    host.audio_put = [](void* ctx, int32_t handle, const void* data, int32_t bytes) {
        static_cast<HostContext*>(ctx)->audio->Put(handle, data, bytes);
    };
    host.audio_close = [](void* ctx, int32_t handle) { static_cast<HostContext*>(ctx)->audio->Close(handle); };
    host.audio_device_channels = [](void* ctx) -> int32_t {
        return static_cast<HostContext*>(ctx)->audio->DeviceChannels();
    };
    host.log = [](void*, const char* line) {
        if (line) {
            OutputDebugStringA(line);
            OutputDebugStringA("\n");
        }
    };
    return host;
}

} // namespace tlou
