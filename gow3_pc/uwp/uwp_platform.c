/* gow3 UWP: the app-container versions of the Win32 calls the runtime makes (uwp_platform.h). */
#if defined(_WIN32) && defined(GOW3_UWP)
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "uwp_platform.h"

/* The *FromApp functions live in kernelbase (api-ms-win-core-memory-l1-1-*); not every MinGW
 * import library has all of them, so they are looked up once at run time. */
typedef PVOID (WINAPI *VirtualAllocFromAppFn)(PVOID, SIZE_T, ULONG, ULONG);
typedef BOOL (WINAPI *VirtualProtectFromAppFn)(PVOID, SIZE_T, ULONG, PULONG);
typedef PVOID (WINAPI *VirtualAlloc2FromAppFn)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER *, ULONG);
typedef PVOID (WINAPI *MapViewOfFile3FromAppFn)(HANDLE, HANDLE, PVOID, ULONG64, SIZE_T, ULONG, ULONG,
                                                MEM_EXTENDED_PARAMETER *, ULONG);
typedef HANDLE (WINAPI *CreateFileMappingFromAppFn)(HANDLE, PSECURITY_ATTRIBUTES, ULONG, ULONG64, PCWSTR);
typedef HMODULE (WINAPI *LoadPackagedLibraryFn)(LPCWSTR, DWORD);

static struct {
    VirtualAllocFromAppFn virtual_alloc;
    VirtualProtectFromAppFn virtual_protect;
    VirtualAlloc2FromAppFn virtual_alloc2;
    MapViewOfFile3FromAppFn map_view3;
    CreateFileMappingFromAppFn create_file_mapping;
    LoadPackagedLibraryFn load_packaged;
} api;
static INIT_ONCE api_once = INIT_ONCE_STATIC_INIT;

static FARPROC lookup(const char *name) {
    static const wchar_t *const modules[] = {L"kernelbase.dll", L"kernel32.dll"};
    for (size_t i = 0; i < sizeof(modules) / sizeof(*modules); ++i) {
        HMODULE module = GetModuleHandleW(modules[i]);
        FARPROC p = module ? GetProcAddress(module, name) : NULL;
        if (p) return p;
    }
    fprintf(stderr, "UWP: %s is not available\n", name);
    return NULL;
}
static BOOL CALLBACK resolve(PINIT_ONCE once, PVOID parameter, PVOID *context) {
    (void)once; (void)parameter; (void)context;
    api.virtual_alloc = (VirtualAllocFromAppFn)(void (*)(void))lookup("VirtualAllocFromApp");
    api.virtual_protect = (VirtualProtectFromAppFn)(void (*)(void))lookup("VirtualProtectFromApp");
    api.virtual_alloc2 = (VirtualAlloc2FromAppFn)(void (*)(void))lookup("VirtualAlloc2FromApp");
    api.map_view3 = (MapViewOfFile3FromAppFn)(void (*)(void))lookup("MapViewOfFile3FromApp");
    api.create_file_mapping = (CreateFileMappingFromAppFn)(void (*)(void))lookup("CreateFileMappingFromApp");
    api.load_packaged = (LoadPackagedLibraryFn)(void (*)(void))lookup("LoadPackagedLibrary");
    return TRUE;
}
static void need_api(void) { InitOnceExecuteOnce(&api_once, resolve, NULL, NULL); }

static PVOID VirtualAllocFromApp_(PVOID a, SIZE_T s, ULONG t, ULONG p) {
    need_api();
    if (!api.virtual_alloc) { SetLastError(ERROR_PROC_NOT_FOUND); return NULL; }
    return api.virtual_alloc(a, s, t, p);
}
static BOOL VirtualProtectFromApp_(PVOID a, SIZE_T s, ULONG p, PULONG old) {
    need_api();
    if (!api.virtual_protect) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
    return api.virtual_protect(a, s, p, old);
}
static PVOID VirtualAlloc2FromApp_(HANDLE h, PVOID a, SIZE_T s, ULONG t, ULONG p, MEM_EXTENDED_PARAMETER *x, ULONG n) {
    need_api();
    if (!api.virtual_alloc2) { SetLastError(ERROR_PROC_NOT_FOUND); return NULL; }
    return api.virtual_alloc2(h, a, s, t, p, x, n);
}
static PVOID MapViewOfFile3FromApp_(HANDLE f, HANDLE h, PVOID a, ULONG64 o, SIZE_T s, ULONG t, ULONG p,
                                    MEM_EXTENDED_PARAMETER *x, ULONG n) {
    need_api();
    if (!api.map_view3) { SetLastError(ERROR_PROC_NOT_FOUND); return NULL; }
    return api.map_view3(f, h, a, o, s, t, p, x, n);
}
static HANDLE CreateFileMappingFromApp_(HANDLE f, PSECURITY_ATTRIBUTES sa, ULONG p, ULONG64 m, PCWSTR name) {
    need_api();
    if (!api.create_file_mapping) { SetLastError(ERROR_PROC_NOT_FOUND); return NULL; }
    return api.create_file_mapping(f, sa, p, m, name);
}
static HMODULE LoadPackagedLibrary_(LPCWSTR name, DWORD reserved) {
    need_api();
    /* Outside a package (a desktop test of this DLL) LoadPackagedLibrary fails: LoadLibraryExW then. */
    HMODULE m = api.load_packaged ? api.load_packaged(name, reserved) : NULL;
    return m ? m : LoadLibraryExW(name, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
}
#define VirtualAllocFromApp VirtualAllocFromApp_
#define VirtualProtectFromApp VirtualProtectFromApp_
#define VirtualAlloc2FromApp VirtualAlloc2FromApp_
#define MapViewOfFile3FromApp MapViewOfFile3FromApp_
#define CreateFileMappingFromApp CreateFileMappingFromApp_
#define LoadPackagedLibrary LoadPackagedLibrary_

/* Write and execute at once is what an app container is most likely to refuse, even with
 * codeGeneration: such requests are retried as execute-read, and reported once. */
static void report_wx(const char *what, DWORD error) {
    static volatile LONG reported;
    if (!InterlockedExchange(&reported, 1))
        fprintf(stderr, "UWP: %s refused write+execute pages (error %lu): using execute-read\n", what, error);
}
static ULONG without_write(ULONG protect) {
    switch (protect & 0xff) {
    case PAGE_EXECUTE_READWRITE: case PAGE_EXECUTE_WRITECOPY:
        return (protect & ~0xffu) | PAGE_EXECUTE_READ;
    default: return protect;
    }
}

PVOID gow3_uwp_virtual_alloc(PVOID address, SIZE_T size, DWORD type, DWORD protect) {
    PVOID p = VirtualAllocFromApp(address, size, type, protect);
    if (!p && without_write(protect) != protect) {
        report_wx("VirtualAllocFromApp", GetLastError());
        p = VirtualAllocFromApp(address, size, type, without_write(protect));
    }
    return p;
}

BOOL gow3_uwp_virtual_protect(PVOID address, SIZE_T size, DWORD protect, PDWORD old) {
    ULONG previous = 0;
    BOOL ok = VirtualProtectFromApp(address, size, protect, &previous);
    if (!ok && without_write(protect) != protect) {
        report_wx("VirtualProtectFromApp", GetLastError());
        ok = VirtualProtectFromApp(address, size, without_write(protect), &previous);
    }
    if (old) *old = previous;
    return ok;
}

PVOID gow3_uwp_virtual_alloc2(HANDLE process, PVOID base, SIZE_T size, ULONG type, ULONG protect,
                              MEM_EXTENDED_PARAMETER *parameters, ULONG count) {
    PVOID p = VirtualAlloc2FromApp(process, base, size, type, protect, parameters, count);
    if (!p && without_write(protect) != protect) {
        report_wx("VirtualAlloc2FromApp", GetLastError());
        p = VirtualAlloc2FromApp(process, base, size, type, without_write(protect), parameters, count);
    }
    return p;
}

PVOID gow3_uwp_map_view3(HANDLE section, HANDLE process, PVOID base, ULONG64 offset, SIZE_T size,
                         ULONG type, ULONG protect, MEM_EXTENDED_PARAMETER *parameters, ULONG count) {
    PVOID p = MapViewOfFile3FromApp(section, process, base, offset, size, type, protect, parameters, count);
    if (!p && without_write(protect) != protect) {
        report_wx("MapViewOfFile3FromApp", GetLastError());
        p = MapViewOfFile3FromApp(section, process, base, offset, size, type, without_write(protect),
                                  parameters, count);
    }
    return p;
}

/* The desktop build asks CreateFileMapping2 for a pagefile-backed SEC_RESERVE section. The app
 * version has no extended parameters: the allocation attributes go with the page protection. */
HANDLE gow3_uwp_create_file_mapping2(HANDLE file, SECURITY_ATTRIBUTES *attributes, ULONG access,
                                     ULONG protect, ULONG allocation, ULONG64 maximum, PCWSTR name,
                                     MEM_EXTENDED_PARAMETER *parameters, ULONG count) {
    (void)access; (void)parameters; (void)count;
    HANDLE h = CreateFileMappingFromApp(file, attributes, protect | allocation, maximum, name);
    if (!h && (protect & 0xff) == PAGE_EXECUTE_READWRITE) {
        /* Guest code runs from the loaded image, not from direct memory: a read-write section
         * still serves the game's data. */
        fprintf(stderr, "UWP: executable section refused (error %lu): guest memory is read-write only\n",
                GetLastError());
        h = CreateFileMappingFromApp(file, attributes, PAGE_READWRITE | allocation, maximum, name);
    }
    return h;
}

/* Packaged apps load DLLs from their package only: a bare file name, looked up in the package. */
HMODULE gow3_uwp_load_library_w(LPCWSTR name) {
    if (!name) return NULL;
    const wchar_t *base = name;
    for (const wchar_t *c = name; *c; ++c)
        if (*c == L'\\' || *c == L'/') base = c + 1;
    /* vulkan-1.dll: a driver the package carries (GOW3_VULKAN_DLL, Mesa's vulkan_dzn.dll) comes first;
     * the Khronos loader needs system DLLs a console may not have. */
    if (!_wcsicmp(base, L"vulkan-1.dll")) {
        wchar_t chosen[MAX_PATH];
        if (GetEnvironmentVariableW(L"GOW3_VULKAN_DLL", chosen, MAX_PATH) && chosen[0]) {
            HMODULE driver = LoadPackagedLibrary(chosen, 0);
            if (driver) {
                fprintf(stderr, "UWP: Vulkan from %ls\n", chosen);
                return driver;
            }
            fprintf(stderr, "UWP: %ls could not be loaded (error %lu): trying vulkan-1.dll\n", chosen, GetLastError());
        }
    }
    return LoadPackagedLibrary(base, 0);
}

HMODULE gow3_uwp_load_library_a(LPCSTR name) {
    if (!name) return NULL;
    wchar_t wide[MAX_PATH];
    if (!MultiByteToWideChar(CP_UTF8, 0, name, -1, wide, MAX_PATH)) return NULL;
    return gow3_uwp_load_library_w(wide);
}

/* GetProcAddress for C++ files that keep <windows.h> out (vk_platform.cpp). */
void *gow3_uwp_get_proc(void *module, const char *name) {
    return module && name ? (void *)GetProcAddress((HMODULE)module, name) : NULL;
}

extern IMAGE_DOS_HEADER __ImageBase;
HMODULE gow3_uwp_self_module(void) { return (HMODULE)&__ImageBase; }
#endif
