#include "pch.h"
#include "Stage.h"

using namespace winrt;
using namespace winrt::Windows::Data::Json;
namespace fs = std::filesystem;

namespace tlou {

std::string Utf8(const std::wstring& text) {
    return winrt::to_string(text);
}

std::wstring Wide(const std::string& text) {
    return std::wstring(winrt::to_hstring(text));
}

namespace {

std::optional<std::string> ReadText(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

bool Exists(const fs::path& path) {
    std::error_code error;
    return fs::exists(path, error);
}

std::string Replace(std::string text, const std::string& from, const std::string& to) {
    for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
        text.replace(at, from.size(), to);
    }
    return text;
}

} // namespace

Settings Settings::Load(const fs::path& file) {
    Settings s;
    const auto text = ReadText(file);
    if (!text) return s;
    JsonObject json{nullptr};
    if (!JsonObject::TryParse(winrt::to_hstring(*text), json)) return s;
    s.motion_blur = json.GetNamedBoolean(L"motion_blur", s.motion_blur);
    s.background_shaders = json.GetNamedBoolean(L"background_shaders", s.background_shaders);
    s.draw_pipe = json.GetNamedBoolean(L"draw_pipe", s.draw_pipe);
    s.frame_stats = json.GetNamedBoolean(L"frame_stats", s.frame_stats);
    s.vulkan_driver = int(json.GetNamedNumber(L"vulkan_driver", s.vulkan_driver));
    s.root = std::wstring(json.GetNamedString(L"root", L""));
    return s;
}

void Settings::Save(const fs::path& file) const {
    JsonObject json;
    json.SetNamedValue(L"motion_blur", JsonValue::CreateBooleanValue(motion_blur));
    json.SetNamedValue(L"background_shaders", JsonValue::CreateBooleanValue(background_shaders));
    json.SetNamedValue(L"draw_pipe", JsonValue::CreateBooleanValue(draw_pipe));
    json.SetNamedValue(L"frame_stats", JsonValue::CreateBooleanValue(frame_stats));
    json.SetNamedValue(L"vulkan_driver", JsonValue::CreateNumberValue(vulkan_driver));
    json.SetNamedValue(L"root", JsonValue::CreateStringValue(root));
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << winrt::to_string(json.Stringify());
}

std::vector<fs::path> CandidateRoots(const fs::path& local_state) {
    std::vector<fs::path> roots{local_state / L"TLOU"};
    for (wchar_t drive = L'D'; drive <= L'Z'; ++drive) {
        roots.emplace_back(std::wstring{drive, L':', L'\\', L'T', L'L', L'O', L'U'});
    }
    return roots;
}

StageInfo ReadStage(const fs::path& root) {
    StageInfo info;
    info.root = root;
    info.has_game = Exists(root / L"CUSA00552" / L"eboot.bin");
    info.has_update = Exists(root / L"CUSA00552-patch" / L"eboot.bin");
    const fs::path boot = root / L"stage" / L"boot.json";
    if (!Exists(boot)) {
        info.error = "stage/boot.json is missing: run Stage-TLOU-Xbox.cmd on the PC";
        return info;
    }
    const auto text = ReadText(boot);
    JsonObject json{nullptr};
    if (!text || !JsonObject::TryParse(winrt::to_hstring(*text), json)) {
        info.error = "stage/boot.json could not be read";
        return info;
    }
    try {
        info.game_version = winrt::to_string(json.GetNamedString(L"game_version", L"?"));
        info.made = winrt::to_string(json.GetNamedString(L"made", L""));
        for (const auto& value : json.GetNamedArray(L"args")) {
            info.args.push_back(winrt::to_string(value.GetString()));
        }
        for (const auto& pair : json.GetNamedObject(L"env")) {
            info.env[winrt::to_string(pair.Key())] = winrt::to_string(pair.Value().GetString());
        }
        info.has_stage = info.args.size() >= 2;
        if (!info.has_stage) info.error = "stage/boot.json has no command line";
    } catch (const winrt::hresult_error& e) {
        info.error = "stage/boot.json: " + winrt::to_string(e.message());
    }
    return info;
}

Launch BuildLaunch(const StageInfo& stage, const Settings& settings, const fs::path& user_dir,
                   const fs::path& install_dir) {
    const std::map<std::string, std::string> places{
        {"${STAGE}", Utf8((stage.root / L"stage").wstring())},
        {"${GAME}", Utf8((stage.root / L"CUSA00552").wstring())},
        {"${UPDATE}", Utf8((stage.root / L"CUSA00552-patch").wstring())},
        {"${USER}", Utf8(user_dir.wstring())},
    };
    auto fill = [&](std::string text) {
        for (const auto& [token, path] : places) text = Replace(std::move(text), token, path);
        return text;
    };
    Launch launch;
    for (const auto& arg : stage.args) launch.args.push_back(fill(arg));

    std::map<std::string, std::string> env;
    for (const auto& [name, value] : stage.env) env[name] = fill(value);
    // Always the app's own storage for saves, settings and the shader cache.
    env["GOW3_GPU_USER_DIR"] = Utf8(user_dir.wstring());
    env["GOW3_CONFIG"] = Utf8((user_dir / L"tlou.ini").wstring());
    env["GOW3_GPU_CACHE_DIR"] = Utf8((user_dir / L"cache").wstring());
    // The launcher's switches (launcher/tlou_launcher.pyw game_env on the PC).
    env["GOW3_NO_MOTION_BLUR"] = settings.motion_blur ? "0" : "1";
    if (settings.background_shaders) {
        env["GOW3_ASYNC_SHADERS"] = "1";
        env["GOW3_ASYNC_SHADERS_LOCKED"] = "0";
    }
    env["GOW3_DRAW_PIPE"] = settings.draw_pipe ? "1" : "0";
    env["GOW3_FRAME_STATS"] = settings.frame_stats ? "1" : "0";
    // A packaged app has no installed Vulkan driver: the package's (README).
    if (settings.vulkan_driver == 1) env["GOW3_VULKAN_DLL"] = "vulkan_dzn.dll";
    if (Exists(install_dir / L"vulkan_icd.json")) {
        env["VK_DRIVER_FILES"] = Utf8((install_dir / L"vulkan_icd.json").wstring());
        env["VK_ICD_FILENAMES"] = env["VK_DRIVER_FILES"];
    }
    for (auto& entry : env) launch.env.emplace_back(entry.first, entry.second);
    return launch;
}

std::vector<std::string> TailLines(const fs::path& file, size_t count) {
    std::vector<std::string> lines;
    std::ifstream in(file, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
        if (lines.size() > count) lines.erase(lines.begin());
    }
    return lines;
}

} // namespace tlou
