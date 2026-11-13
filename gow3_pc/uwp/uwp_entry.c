/* gow3 UWP: what gow3-runtime.dll exports to the UWP app (gow3_uwp_host.h).
 *
 * The desktop build is gow3-probe.exe, started by tools/probe_tlou.py with a command line and an
 * environment. A packaged app cannot start processes, so the same program is a DLL here: the app
 * sets the environment with gow3_uwp_setenv and calls gow3_uwp_run with the command line the PC
 * staging step wrote (tools/probe_tlou.py --stage). probe.c's main is compiled as
 * gow3_probe_main for this build (CMakeLists.txt). */
#if defined(_WIN32) && defined(GOW3_UWP)
#include <windows.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gow3_uwp_host.h"

#define EXPORT __declspec(dllexport)

int gow3_probe_main(int argc, char **argv);

static gow3_uwp_host host_table;
static int host_set;

const gow3_uwp_host *gow3_uwp_host_get(void) { return host_set ? &host_table : NULL; }

EXPORT int32_t gow3_uwp_api_version(void) { return GOW3_UWP_HOST_VERSION; }

EXPORT void gow3_uwp_set_host(const gow3_uwp_host *host) {
    if (!host || host->version != GOW3_UWP_HOST_VERSION) return;
    host_table = *host;
    host_set = 1;
}

/* Both the CRT's environment (getenv) and the process's (GetEnvironmentVariable): the runtime
 * reads the first, Vulkan drivers and libraries may read the second. */
EXPORT int32_t gow3_uwp_setenv(const char *name, const char *value) {
    if (!name || !*name) return -1;
    if (_putenv_s(name, value ? value : "")) return -1;
    wchar_t wname[256], wvalue[4096];
    if (MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, 256) &&
        MultiByteToWideChar(CP_UTF8, 0, value ? value : "", -1, wvalue, 4096))
        SetEnvironmentVariableW(wname, value && *value ? wvalue : NULL);
    return 0;
}

EXPORT int32_t gow3_uwp_run(int32_t argc, const char *const *argv, const char *log_path) {
    if (argc < 2 || !argv) return 2;
    /* No console in an app: the game's output goes to the log the app shows on failure. */
    if (log_path && *log_path) {
        if (freopen(log_path, "w", stdout)) {
            setvbuf(stdout, NULL, _IOLBF, 0);
            /* stderr into the same file, unbuffered, as `> boot.log 2>&1` does on the PC. */
            _dup2(_fileno(stdout), _fileno(stderr));
            setvbuf(stderr, NULL, _IONBF, 0);
        }
    }
    printf("UWP: gow3-runtime starting (%d arguments)\n", argc);
    char **copy = calloc((size_t)argc + 1, sizeof(*copy));
    if (!copy) return 3;
    for (int i = 0; i < argc; ++i) copy[i] = _strdup(argv[i] ? argv[i] : "");
    int code = gow3_probe_main(argc, copy);
    printf("UWP: gow3-runtime returned %d\n", code);
    fflush(stdout);
    return code;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)instance; (void)reason; (void)reserved;
    return TRUE;
}
#endif
