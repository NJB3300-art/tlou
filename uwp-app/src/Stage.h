// The game on the console: where the TLOU folder is, what the PC staging step wrote
// (TLOU/stage/boot.json, tools/probe_tlou.py --stage) and the launcher's saved switches.
#pragma once
#include "pch.h"

namespace tlou {

struct Settings {
    bool motion_blur = false;
    bool background_shaders = true;
    bool draw_pipe = true;
    bool frame_stats = true;
    bool quarter_depth = true; // v0.2.2: align contact shadows with object edges
    int vulkan_driver = 0; // 0: auto (Mesa Dozen when the package has it), 1: the Khronos loader (vulkan-1.dll)
    std::wstring root;     // the chosen TLOU folder; empty: the first one found

    static Settings Load(const std::filesystem::path& file);
    void Save(const std::filesystem::path& file) const;
};

// A PS4 param.sfo: what game, which version, base game ("gd") or update ("gp").
struct SfoInfo {
    bool ok = false;
    std::string title_id, app_ver, version, category;
};
SfoInfo ReadSfo(const std::filesystem::path& file);

struct StageInfo {
    std::filesystem::path root;     // ...\TLOU
    bool has_game = false, has_update = false, has_stage = false;
    std::string game_version, made;
    std::vector<std::string> args;  // with ${...} placeholders
    std::map<std::string, std::string> env;
    std::string error;              // why boot.json could not be read
    SfoInfo game_sfo, update_sfo;   // sce_sys/param.sfo of the game and the update
    std::vector<std::string> problems; // what the file check found wrong (empty: all good)
};

// Folders that may hold TLOU: the app's LocalState, then the drives' roots (the external drive
// is usually E:).
std::vector<std::filesystem::path> CandidateRoots(const std::filesystem::path& local_state);
StageInfo ReadStage(const std::filesystem::path& root);

// The runtime's command line and environment for this stage and these switches.
struct Launch {
    std::vector<std::string> args;
    std::vector<std::pair<std::string, std::string>> env;
};
Launch BuildLaunch(const StageInfo& stage, const Settings& settings, const std::filesystem::path& user_dir,
                   const std::filesystem::path& install_dir, void* core_window);

std::string Utf8(const std::wstring& text);
std::wstring Wide(const std::string& text);
std::vector<std::string> TailLines(const std::filesystem::path& file, size_t count);

} // namespace tlou
