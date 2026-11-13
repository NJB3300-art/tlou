// TLOU.Uwp: the launcher (drawn with Direct2D, driven by the controller) and the host of
// gow3-runtime.dll, which runs the game in this app's CoreWindow.
#include "pch.h"
#include "Host.h"
#include "Stage.h"

using namespace winrt;
using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::ApplicationModel::Core;

using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::ViewManagement;
namespace fs = std::filesystem;

namespace {

// ICoreWindowInterop (corewindow.h), declared here: some SDK headers keep it desktop-only.
struct __declspec(uuid("45D64A29-A63E-4CB6-B498-5781D298CB4F")) ICoreWindowInteropLocal : ::IUnknown {
    virtual HRESULT STDMETHODCALLTYPE get_WindowHandle(HWND* hwnd) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_MessageHandled(unsigned char value) = 0;
};

constexpr wchar_t AppTitle[] = L"The Last of Us Remastered — UWP";

enum class Item { Play, Location, MotionBlur, ContactShadows, BackgroundShaders, DrawPipe, FrameStats, VulkanDriver, Exit, Count };

// One controller button press per edge, with auto-repeat for the d-pad.
struct PadNav {
    uint32_t last = 0;
    std::chrono::steady_clock::time_point repeat_at{};
    uint32_t Pressed(uint32_t buttons) {
        constexpr uint32_t Repeating = GOW3_UWP_PAD_DPAD_UP | GOW3_UWP_PAD_DPAD_DOWN |
                                       GOW3_UWP_PAD_DPAD_LEFT | GOW3_UWP_PAD_DPAD_RIGHT;
        const auto now = std::chrono::steady_clock::now();
        uint32_t pressed = buttons & ~last;
        if (buttons & last & Repeating) {
            if (now >= repeat_at) {
                pressed |= buttons & last & Repeating;
                repeat_at = now + std::chrono::milliseconds(120);
            }
        } else if (pressed) {
            repeat_at = now + std::chrono::milliseconds(400);
        }
        last = buttons;
        return pressed;
    }
};

class Launcher {
public:
    bool Create(const CoreWindow& window) {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, ARRAYSIZE(levels),
                                     D3D11_SDK_VERSION, &device, nullptr, &context))) {
            return false;
        }
        d3d = device;
        d3d_context = context;
        ComPtr<IDXGIDevice1> dxgi_device;
        d3d.As(&dxgi_device);
        ComPtr<IDXGIAdapter> adapter;
        dxgi_device->GetAdapter(&adapter);
        ComPtr<IDXGIFactory2> factory;
        adapter->GetParent(IID_PPV_ARGS(&factory));

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.Scaling = DXGI_SCALING_STRETCH;
        if (FAILED(factory->CreateSwapChainForCoreWindow(d3d.Get(), winrt::get_unknown(window), &desc, nullptr,
                                                         &swap_chain))) {
            return false;
        }
        D2D1_FACTORY_OPTIONS options{};
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory3), &options,
                                     reinterpret_cast<void**>(d2d_factory.GetAddressOf())))) {
            return false;
        }
        ComPtr<ID2D1Device2> d2d_device;
        d2d_factory->CreateDevice(dxgi_device.Get(), &d2d_device);
        d2d_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2d);
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                            reinterpret_cast<IUnknown**>(dwrite.GetAddressOf()));
        dwrite->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
                                 DWRITE_FONT_STRETCH_NORMAL, 46.0f, L"en-us", &title_font);
        dwrite->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                 DWRITE_FONT_STRETCH_NORMAL, 28.0f, L"en-us", &item_font);
        dwrite->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                 DWRITE_FONT_STRETCH_NORMAL, 17.0f, L"en-us", &small_font);
        d2d->CreateSolidColorBrush(D2D1::ColorF(0.92f, 0.94f, 0.90f), &text_brush);
        d2d->CreateSolidColorBrush(D2D1::ColorF(0.55f, 0.60f, 0.55f), &dim_brush);
        d2d->CreateSolidColorBrush(D2D1::ColorF(0.33f, 0.62f, 0.38f), &accent_brush);
        d2d->CreateSolidColorBrush(D2D1::ColorF(0.86f, 0.42f, 0.32f), &warn_brush);
        d2d->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.07f), &panel_brush);
        return Target();
    }

    void Destroy() {
        target.Reset();
        if (d2d) d2d->SetTarget(nullptr);
        text_brush.Reset(); dim_brush.Reset(); accent_brush.Reset(); warn_brush.Reset(); panel_brush.Reset();
        title_font.Reset(); item_font.Reset(); small_font.Reset(); dwrite.Reset();
        d2d.Reset(); d2d_factory.Reset();
        swap_chain.Reset();
        if (d3d_context) {
            d3d_context->ClearState();
            d3d_context->Flush();
        }
        d3d_context.Reset();
        d3d.Reset();
    }

    bool Valid() const { return swap_chain != nullptr; }

    struct View {
        int selected = 0;
        std::vector<std::pair<std::wstring, std::wstring>> items; // label, value
        std::vector<std::wstring> info;                           // right panel
        std::vector<std::wstring> log;                            // bottom panel
        std::wstring status;
        bool status_warn = false;
    };

    void Draw(const View& view) {
        if (!d2d || !target) return;
        d2d->BeginDraw();
        d2d->Clear(D2D1::ColorF(0.055f, 0.07f, 0.06f));
        const D2D1_SIZE_F size = d2d->GetSize();
        const float margin = size.width * 0.06f, top = size.height * 0.07f;
        Text(AppTitle, title_font.Get(), text_brush.Get(), margin, top, size.width - 2 * margin, 70);
        d2d->FillRectangle(D2D1::RectF(margin, top + 72, margin + 220, top + 76), accent_brush.Get());

        const float list_w = size.width * 0.46f;
        float y = top + 110;
        for (size_t i = 0; i < view.items.size(); ++i) {
            const bool on = int(i) == view.selected;
            if (on) {
                d2d->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(margin - 14, y - 4, margin + list_w, y + 46), 8, 8),
                                          accent_brush.Get());
            }
            Text(view.items[i].first, item_font.Get(), text_brush.Get(), margin, y, list_w * 0.55f, 44);
            Text(view.items[i].second, item_font.Get(), on ? text_brush.Get() : dim_brush.Get(),
                 margin + list_w * 0.55f, y, list_w * 0.45f - 10, 44);
            y += 56;
        }
        const float px = margin + list_w + 40, pw = size.width - px - margin;
        d2d->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(px, top + 106, px + pw, y), 10, 10), panel_brush.Get());
        float iy = top + 122;
        for (const auto& line : view.info) {
            Text(line, small_font.Get(), dim_brush.Get(), px + 18, iy, pw - 36, 24);
            iy += 26;
        }
        if (!view.status.empty()) {
            Text(view.status, item_font.Get(), view.status_warn ? warn_brush.Get() : accent_brush.Get(), margin, y + 16,
                 size.width - 2 * margin, 40);
        }
        float ly = y + 70;
        for (const auto& line : view.log) {
            if (ly > size.height - 40) break;
            Text(line, small_font.Get(), dim_brush.Get(), margin, ly, size.width - 2 * margin, 22);
            ly += 22;
        }
        Text(L"A  select / change      ← →  change      Menu  play", small_font.Get(), dim_brush.Get(), margin,
             size.height - 34, size.width - 2 * margin, 24);
        if (d2d->EndDraw() == D2DERR_RECREATE_TARGET) {
            target.Reset();
        }
        swap_chain->Present(1, 0);
    }

private:
    bool Target() {
        ComPtr<IDXGISurface> surface;
        if (FAILED(swap_chain->GetBuffer(0, IID_PPV_ARGS(&surface)))) return false;
        const D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
        if (FAILED(d2d->CreateBitmapFromDxgiSurface(surface.Get(), &props, &target))) return false;
        d2d->SetTarget(target.Get());
        return true;
    }
    void Text(const std::wstring& text, IDWriteTextFormat* font, ID2D1Brush* brush, float x, float y, float w, float h) {
        d2d->DrawText(text.c_str(), UINT32(text.size()), font, D2D1::RectF(x, y, x + w, y + h), brush,
                      D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    ComPtr<ID3D11Device> d3d;
    ComPtr<ID3D11DeviceContext> d3d_context;
    ComPtr<IDXGISwapChain1> swap_chain;
    ComPtr<ID2D1Factory3> d2d_factory;
    ComPtr<ID2D1DeviceContext2> d2d;
    ComPtr<ID2D1Bitmap1> target;
    ComPtr<IDWriteFactory> dwrite;
    ComPtr<IDWriteTextFormat> title_font, item_font, small_font;
    ComPtr<ID2D1SolidColorBrush> text_brush, dim_brush, accent_brush, warn_brush, panel_brush;
};

struct App : implements<App, IFrameworkViewSource, IFrameworkView> {
    IFrameworkView CreateView() { return *this; }

    void Initialize(const CoreApplicationView& view) {
        view.Activated([](const auto&, const IActivatedEventArgs&) { CoreWindow::GetForCurrentThread().Activate(); });
        // The whole TV, in real pixels (Xbox scales apps to 1080p and keeps a TV-safe margin otherwise).
        try {
            ApplicationViewScaling::TrySetDisableLayoutScaling(true);
        } catch (...) {
        }
    }

    void SetWindow(const CoreWindow& window) {
        core_window = window;
        try {
            ApplicationView::GetForCurrentView().SetDesiredBoundsMode(ApplicationViewBoundsMode::UseCoreWindow);
        } catch (...) {
        }
        window.KeyDown([this](const auto&, const KeyEventArgs& e) {
            if (game_started) input->PushKey(e.VirtualKey(), true);
            else pending_keys.push_back(e.VirtualKey());
            e.Handled(true);
        });
        window.KeyUp([this](const auto&, const KeyEventArgs& e) {
            if (game_started) input->PushKey(e.VirtualKey(), false);
            e.Handled(true);
        });
        window.SizeChanged([this](const auto&, const WindowSizeChangedEventArgs&) { UpdateSize(); });
        window.VisibilityChanged([this](const auto&, const VisibilityChangedEventArgs& e) { state.visible = e.Visible(); });
        window.Activated([this](const auto&, const WindowActivatedEventArgs& e) {
            state.focused = e.WindowActivationState() != CoreWindowActivationState::Deactivated;
        });
        window.Closed([this](const auto&, const auto&) {
            state.closed = true;
            closing = true;
        });
        // B on the controller is the system's Back: the game uses B, so it never leaves the app.
        winrt::Windows::UI::Core::SystemNavigationManager::GetForCurrentView().BackRequested(
            [](const auto&, const BackRequestedEventArgs& e) { e.Handled(true); });

        state.core_window = winrt::get_abi(window);
        if (auto unknown = winrt::get_unknown(window)) {
            ComPtr<ICoreWindowInteropLocal> interop;
            if (SUCCEEDED(unknown->QueryInterface(IID_PPV_ARGS(&interop)))) {
                HWND hwnd = nullptr;
                if (SUCCEEDED(interop->get_WindowHandle(&hwnd))) state.hwnd = hwnd;
            }
        }
        try {
            const auto display = DisplayInformation::GetForCurrentView();
            state.display_width = int32_t(display.ScreenWidthInRawPixels());
            state.display_height = int32_t(display.ScreenHeightInRawPixels());
        } catch (...) {
        }
        try {
            const auto hdmi = winrt::Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
            if (hdmi) state.refresh_rate = float(hdmi.GetCurrentDisplayMode().RefreshRate());
        } catch (...) {
        }
        UpdateSize();
    }

    void Load(const hstring&) {}

    void Uninitialize() {}

    void Run() {
        local_state = fs::path(std::wstring(winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path()));
        install_dir = fs::path(std::wstring(Package::Current().InstalledLocation().Path()));
        user_dir = local_state / L"user";
        std::error_code ignored;
        fs::create_directories(user_dir / L"cache", ignored);
        fs::create_directories(local_state / L"TLOU", ignored);
        settings = tlou::Settings::Load(local_state / L"launcher.json");
        input = std::make_unique<tlou::Input>();
        audio = std::make_unique<tlou::Audio>();
        FindGame();

        if (!launcher.Create(core_window)) status = L"Direct3D could not start the launcher";
        const auto dispatcher = core_window.Dispatcher();
        while (!closing) {
            dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
            if (game_started) {
                // The game owns the window now; events still have to be pumped on this thread.
                if (game_done) {
                    game_started = false;
                    AfterGame();
                    continue;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(4));
                continue;
            }
            HandleInput();
            if (!game_started && launcher.Valid()) launcher.Draw(MakeView());
            else std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

private:
    void UpdateSize() {
        if (!core_window) return;
        const auto bounds = core_window.Bounds();
        float scale = 1.0f;
        try {
            scale = float(DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel());
        } catch (...) {
        }
        state.width = int32_t(bounds.Width * scale + 0.5f);
        state.height = int32_t(bounds.Height * scale + 0.5f);
    }

    void FindGame() {
        roots.clear();
        for (const auto& candidate : tlou::CandidateRoots(local_state)) {
            std::error_code error;
            if (fs::exists(candidate, error)) roots.push_back(candidate);
        }
        root_index = 0;
        if (!settings.root.empty()) {
            for (size_t i = 0; i < roots.size(); ++i) {
                if (roots[i].wstring() == settings.root) root_index = int(i);
            }
        }
        // Prefer a folder that is complete.
        if (settings.root.empty()) {
            for (size_t i = 0; i < roots.size(); ++i) {
                const auto info = tlou::ReadStage(roots[i]);
                if (info.has_game && info.has_stage) {
                    root_index = int(i);
                    break;
                }
            }
        }
        stage = roots.empty() ? tlou::StageInfo{} : tlou::ReadStage(roots[root_index]);
    }

    std::wstring OnOff(bool value) { return value ? L"On" : L"Off"; }

    Launcher::View MakeView() {
        Launcher::View v;
        v.selected = selected;
        for (int i = 0; i < int(Item::Count); ++i) {
            switch (Item(i)) {
            case Item::Play: v.items.push_back({played ? L"Restart the app to play again" : L"Play", L""}); break;
            case Item::Location:
                v.items.push_back({L"Game folder", roots.empty() ? L"not found" : roots[root_index].wstring()});
                break;
            case Item::MotionBlur: v.items.push_back({L"Motion blur", OnOff(settings.motion_blur)}); break;
            case Item::ContactShadows: v.items.push_back({L"Align contact shadows", OnOff(settings.quarter_depth)}); break;
            case Item::BackgroundShaders: v.items.push_back({L"Shaders in background", OnOff(settings.background_shaders)}); break;
            case Item::DrawPipe: v.items.push_back({L"Draw pipeline", OnOff(settings.draw_pipe)}); break;
            case Item::FrameStats: v.items.push_back({L"Frame statistics", OnOff(settings.frame_stats)}); break;
            case Item::VulkanDriver:
                v.items.push_back({L"Vulkan driver", settings.vulkan_driver ? L"Mesa Dozen (vulkan_dzn.dll)" : L"vulkan-1.dll"});
                break;
            case Item::Exit: v.items.push_back({L"Exit", L""}); break;
            default: break;
            }
        }
        auto yes = [](bool b) { return b ? L"found" : L"missing"; };
        v.info.push_back(L"Folder layout:  <drive>:\\TLOU  or  LocalState\\TLOU");
        v.info.push_back(std::wstring(L"  CUSA00552 (game) ........ ") + yes(stage.has_game));
        v.info.push_back(std::wstring(L"  CUSA00552-patch (1.11) .. ") + yes(stage.has_update));
        v.info.push_back(std::wstring(L"  stage\\boot.json ......... ") + yes(stage.has_stage));
        if (stage.has_stage) {
            v.info.push_back(L"  staged " + tlou::Wide(stage.made) + L", game " + tlou::Wide(stage.game_version));
        } else if (!stage.error.empty()) {
            v.info.push_back(L"  " + tlou::Wide(stage.error));
        }
        v.info.push_back(L"");
        v.info.push_back(L"Saves, settings, shader cache, boot.log:");
        v.info.push_back(L"  " + local_state.wstring());
        v.info.push_back(std::wstring(L"Window handle for Vulkan: ") + (state.hwnd ? L"yes" : L"no"));
        v.status = status;
        v.status_warn = status_warn;
        v.log = log_tail;
        return v;
    }

    void Change(Item item, int direction) {
        switch (item) {
        case Item::Location:
            if (!roots.empty()) {
                root_index = int((root_index + direction + int(roots.size())) % int(roots.size()));
                settings.root = roots[root_index].wstring();
                stage = tlou::ReadStage(roots[root_index]);
            } else {
                FindGame();
            }
            break;
        case Item::MotionBlur: settings.motion_blur = !settings.motion_blur; break;
        case Item::ContactShadows: settings.quarter_depth = !settings.quarter_depth; break;
        case Item::BackgroundShaders: settings.background_shaders = !settings.background_shaders; break;
        case Item::DrawPipe: settings.draw_pipe = !settings.draw_pipe; break;
        case Item::FrameStats: settings.frame_stats = !settings.frame_stats; break;
        case Item::VulkanDriver: settings.vulkan_driver = settings.vulkan_driver ? 0 : 1; break;
        default: return;
        }
        settings.Save(local_state / L"launcher.json");
    }

    void Activate(Item item) {
        if (item == Item::Play) Play();
        else if (item == Item::Exit) CoreApplication::Exit();
        else Change(item, 1);
    }

    void HandleInput() {
        using winrt::Windows::System::VirtualKey;
        const int count = int(Item::Count);
        for (const VirtualKey key : pending_keys) {
            switch (key) {
            case VirtualKey::Up: selected = (selected + count - 1) % count; break;
            case VirtualKey::Down: selected = (selected + 1) % count; break;
            case VirtualKey::Left: Change(Item(selected), -1); break;
            case VirtualKey::Right: Change(Item(selected), 1); break;
            case VirtualKey::Enter: case VirtualKey::Space: Activate(Item(selected)); break;
            default: break;
            }
        }
        pending_keys.clear();
        gow3_uwp_gamepad pad{};
        uint32_t buttons = 0;
        for (int i = 0; i < 4; ++i) {
            if (input->Gamepad(i, pad)) {
                buttons |= pad.buttons;
                if (pad.left_y > 0.6f) buttons |= GOW3_UWP_PAD_DPAD_UP;
                if (pad.left_y < -0.6f) buttons |= GOW3_UWP_PAD_DPAD_DOWN;
                if (pad.left_x < -0.6f) buttons |= GOW3_UWP_PAD_DPAD_LEFT;
                if (pad.left_x > 0.6f) buttons |= GOW3_UWP_PAD_DPAD_RIGHT;
            }
        }
        const uint32_t pressed = nav.Pressed(buttons);
        if (pressed & GOW3_UWP_PAD_DPAD_UP) selected = (selected + count - 1) % count;
        if (pressed & GOW3_UWP_PAD_DPAD_DOWN) selected = (selected + 1) % count;
        if (pressed & GOW3_UWP_PAD_DPAD_LEFT) Change(Item(selected), -1);
        if (pressed & GOW3_UWP_PAD_DPAD_RIGHT) Change(Item(selected), 1);
        if (pressed & GOW3_UWP_PAD_A) Activate(Item(selected));
        if (pressed & GOW3_UWP_PAD_MENU) Activate(Item::Play);
    }

    void Fail(const std::wstring& message) {
        status = message;
        status_warn = true;
    }

    void Play() {
        if (played) {
            Fail(L"The game ran once in this session: close the app (or restart it) to play again.");
            return;
        }
        if (roots.empty() || !stage.has_game) {
            Fail(L"Copy the game to <drive>:\\TLOU\\CUSA00552 (README) and choose that folder.");
            return;
        }
        if (!stage.has_stage) {
            Fail(L"No stage\\boot.json: run Stage-TLOU-Xbox.cmd on the PC and copy the stage folder.");
            return;
        }
        runtime = LoadPackagedLibrary(L"gow3-runtime.dll", 0);
        if (!runtime) {
            Fail(L"gow3-runtime.dll could not be loaded (error " + std::to_wstring(GetLastError()) +
                 L"; 126 = one of its DLLs is missing or refused on this system).");
            return;
        }
        auto version = reinterpret_cast<gow3_uwp_api_version_fn>(GetProcAddress(runtime, "gow3_uwp_api_version"));
        set_host = reinterpret_cast<gow3_uwp_set_host_fn>(GetProcAddress(runtime, "gow3_uwp_set_host"));
        set_env = reinterpret_cast<gow3_uwp_setenv_fn>(GetProcAddress(runtime, "gow3_uwp_setenv"));
        run = reinterpret_cast<gow3_uwp_run_fn>(GetProcAddress(runtime, "gow3_uwp_run"));
        if (!version || !set_host || !set_env || !run || version() != GOW3_UWP_HOST_VERSION) {
            Fail(L"gow3-runtime.dll does not match this app (rebuild both from the same commit).");
            return;
        }
        launch = tlou::BuildLaunch(stage, settings, user_dir, install_dir);
        for (const auto& [name, value] : launch.env) set_env(name.c_str(), value.c_str());
        log_path = tlou::Utf8((local_state / L"boot.log").wstring());

        // The launcher's swap chain lets go of the window before the game's Vulkan surface takes it.
        launcher.Destroy();
        input->ClearKeys();
        host = tlou::MakeHostTable(state, *input, *audio);
        set_host(&host);
        played = true;
        game_done = false;
        game_started = true;
        // The guest main thread runs on this thread's stack: 16 MiB, as gow3-probe.exe's.
        HANDLE thread = CreateThread(nullptr, 16u << 20, &App::GameThread, this, 0, nullptr);
        if (!thread) {
            game_started = false;
            launcher.Create(core_window);
            Fail(L"The game thread could not start (error " + std::to_wstring(GetLastError()) + L").");
            return;
        }
        CloseHandle(thread);
    }

    static DWORD WINAPI GameThread(void* parameter) {
        auto* self = static_cast<App*>(parameter);
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        std::vector<const char*> argv;
        for (const auto& arg : self->launch.args) argv.push_back(arg.c_str());
        self->exit_code = self->run(int32_t(argv.size()), argv.data(), self->log_path.c_str());
        self->game_done = true;
        return 0;
    }

    void AfterGame() {
        launcher.Create(core_window);
        status = L"The game stopped (code " + std::to_wstring(exit_code) + L"). The end of boot.log:";
        status_warn = exit_code != 0;
        log_tail.clear();
        for (const auto& line : tlou::TailLines(local_state / L"boot.log", 18)) log_tail.push_back(tlou::Wide(line));
    }

    CoreWindow core_window{nullptr};
    tlou::WindowState state;
    std::unique_ptr<tlou::Input> input;
    std::unique_ptr<tlou::Audio> audio;
    Launcher launcher;
    PadNav nav;
    std::vector<winrt::Windows::System::VirtualKey> pending_keys;
    int selected = 0;
    bool closing = false;
    std::atomic<bool> game_started{false}, game_done{false};
    bool played = false;
    int32_t exit_code = 0;
    std::wstring status;
    bool status_warn = false;
    std::vector<std::wstring> log_tail;

    fs::path local_state, install_dir, user_dir;
    tlou::Settings settings;
    std::vector<fs::path> roots;
    int root_index = 0;
    tlou::StageInfo stage;

    HMODULE runtime = nullptr;
    gow3_uwp_set_host_fn set_host = nullptr;
    gow3_uwp_setenv_fn set_env = nullptr;
    gow3_uwp_run_fn run = nullptr;
    gow3_uwp_host host{};
    tlou::Launch launch;
    std::string log_path;
};

} // namespace

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    winrt::init_apartment();
    CoreApplication::Run(make<App>());
    return 0;
}
