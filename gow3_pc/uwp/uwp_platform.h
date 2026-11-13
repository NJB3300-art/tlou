/* gow3 UWP: Win32 calls the app container refuses, routed to the ones it allows.
 *
 * Include after <windows.h>, only in builds with GOW3_UWP. The memory calls go to the *FromApp
 * variants (the package declares the codeGeneration capability, which is what lets them map
 * executable pages); calls with no app-container equivalent (process creation, the multimedia
 * timer, performance counters) become no-ops that report failure. */
#pragma once
#if defined(_WIN32) && defined(GOW3_UWP)
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

PVOID gow3_uwp_virtual_alloc(PVOID address, SIZE_T size, DWORD type, DWORD protect);
BOOL gow3_uwp_virtual_protect(PVOID address, SIZE_T size, DWORD protect, PDWORD old);
PVOID gow3_uwp_virtual_alloc2(HANDLE process, PVOID base, SIZE_T size, ULONG type, ULONG protect,
                              MEM_EXTENDED_PARAMETER *parameters, ULONG count);
PVOID gow3_uwp_map_view3(HANDLE section, HANDLE process, PVOID base, ULONG64 offset, SIZE_T size,
                         ULONG type, ULONG protect, MEM_EXTENDED_PARAMETER *parameters, ULONG count);
HANDLE gow3_uwp_create_file_mapping2(HANDLE file, SECURITY_ATTRIBUTES *attributes, ULONG access,
                                     ULONG protect, ULONG allocation, ULONG64 maximum, PCWSTR name,
                                     MEM_EXTENDED_PARAMETER *parameters, ULONG count);
HMODULE gow3_uwp_load_library_w(LPCWSTR name);
HMODULE gow3_uwp_load_library_a(LPCSTR name);
/* The runtime DLL itself, where the desktop build asked for the executable (GetModuleHandle(NULL)). */
HMODULE gow3_uwp_self_module(void);
void *gow3_uwp_get_proc(void *module, const char *name);

#ifdef __cplusplus
}
#endif

#define VirtualAlloc gow3_uwp_virtual_alloc
#define VirtualProtect gow3_uwp_virtual_protect
#define VirtualAlloc2 gow3_uwp_virtual_alloc2
#define MapViewOfFile3 gow3_uwp_map_view3
#define CreateFileMapping2 gow3_uwp_create_file_mapping2
#undef LoadLibraryW
#undef LoadLibraryA
#define LoadLibraryW gow3_uwp_load_library_w
#define LoadLibraryA gow3_uwp_load_library_a
#endif
