/*
 * lhp2_d3d9_fix.c
 *
 * Quiet release proxy for LEGO Harry Potter: Years 5-7.
 *
 * It combines two fixes:
 *   1. Forward D3D9 to DXVK's x32 d3d9.dll, renamed to dxvk_d3d9.dll.
 *   2. Cap the game's broken main frame-limiter Sleep callsite so complex
 *      areas cannot spiral into long artificial stalls.
 *
 * Expected game directory layout:
 *   d3d9.dll       = this DLL
 *   dxvk_d3d9.dll  = DXVK x32 d3d9.dll
 *   lhp2_fix.ini   = optional config
 */

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <intrin.h>

#define CHAIN_DLL_NAME "dxvk_d3d9.dll"
#define CONFIG_FILE_NAME "lhp2_fix.ini"
#define DEBUG_LOG_FILE_NAME "lhp2_fix.log"

/* Return address immediately after harry2.exe's main frame-limiter Sleep call. */
#define FRAME_LIMITER_RETADDR 0x004B43FC

typedef struct Config {
    int enabled;
    int frame_limiter_sleep_cap_ms;
    int debug_log;
} Config;

static HINSTANCE g_self = NULL;
static HMODULE g_real_d3d9 = NULL;
static char g_game_dir[MAX_PATH];
static char g_chain_path[MAX_PATH];
static char g_log_path[MAX_PATH];
static FILE *g_log = NULL;
static volatile LONG g_hooks_applied = 0;
static Config g_cfg = { 1, 2, 0 };

typedef void* (__stdcall *pfn_Direct3DCreate9)(UINT);
typedef HRESULT (__stdcall *pfn_Direct3DCreate9Ex)(UINT, void**);
typedef int (__stdcall *pfn_D3DPERF_BeginEvent)(DWORD, LPCWSTR);
typedef int (__stdcall *pfn_D3DPERF_EndEvent)(void);
typedef DWORD (__stdcall *pfn_D3DPERF_GetStatus)(void);
typedef BOOL (__stdcall *pfn_D3DPERF_QueryRepeatFrame)(void);
typedef void (__stdcall *pfn_D3DPERF_SetMarker)(DWORD, LPCWSTR);
typedef void (__stdcall *pfn_D3DPERF_SetOptions)(DWORD);
typedef void (__stdcall *pfn_D3DPERF_SetRegion)(DWORD, LPCWSTR);

static pfn_Direct3DCreate9 real_Direct3DCreate9 = NULL;
static pfn_Direct3DCreate9Ex real_Direct3DCreate9Ex = NULL;
static pfn_D3DPERF_BeginEvent real_D3DPERF_BeginEvent = NULL;
static pfn_D3DPERF_EndEvent real_D3DPERF_EndEvent = NULL;
static pfn_D3DPERF_GetStatus real_D3DPERF_GetStatus = NULL;
static pfn_D3DPERF_QueryRepeatFrame real_D3DPERF_QueryRepeatFrame = NULL;
static pfn_D3DPERF_SetMarker real_D3DPERF_SetMarker = NULL;
static pfn_D3DPERF_SetOptions real_D3DPERF_SetOptions = NULL;
static pfn_D3DPERF_SetRegion real_D3DPERF_SetRegion = NULL;

static void (WINAPI *g_orig_Sleep)(DWORD) = NULL;

static void log_debug(const char *fmt, ...) {
    va_list ap;
    if (!g_log) return;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fflush(g_log);
}

static char *trim(char *s) {
    char *end;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        *--end = '\0';
    }
    return s;
}

static int parse_boolish(const char *v) {
    if (!v) return 0;
    if (_stricmp(v, "1") == 0) return 1;
    if (_stricmp(v, "true") == 0) return 1;
    if (_stricmp(v, "yes") == 0) return 1;
    if (_stricmp(v, "on") == 0) return 1;
    return 0;
}

static void set_option(const char *key, const char *value) {
    if (_stricmp(key, "enabled") == 0) {
        g_cfg.enabled = parse_boolish(value);
    } else if (_stricmp(key, "frame_limiter_sleep_cap_ms") == 0) {
        g_cfg.frame_limiter_sleep_cap_ms = atoi(value);
    } else if (_stricmp(key, "debug_log") == 0) {
        g_cfg.debug_log = parse_boolish(value);
    }

    if (g_cfg.frame_limiter_sleep_cap_ms < 0) g_cfg.frame_limiter_sleep_cap_ms = 0;
    if (g_cfg.frame_limiter_sleep_cap_ms > 50) g_cfg.frame_limiter_sleep_cap_ms = 50;
}

static void init_paths(void) {
    char *slash;
    DWORD n = GetModuleFileNameA(g_self, g_game_dir, (DWORD)sizeof(g_game_dir));
    if (n == 0 || n >= sizeof(g_game_dir)) {
        g_game_dir[0] = '\0';
        return;
    }
    slash = strrchr(g_game_dir, '\\');
    if (slash) slash[1] = '\0';

    _snprintf(g_chain_path, sizeof(g_chain_path), "%s%s", g_game_dir, CHAIN_DLL_NAME);
    g_chain_path[sizeof(g_chain_path) - 1] = '\0';

    _snprintf(g_log_path, sizeof(g_log_path), "%s%s", g_game_dir, DEBUG_LOG_FILE_NAME);
    g_log_path[sizeof(g_log_path) - 1] = '\0';
}

static void load_config(void) {
    char cfg[MAX_PATH];
    FILE *f;
    char line[512];

    if (!g_game_dir[0]) return;
    _snprintf(cfg, sizeof(cfg), "%s%s", g_game_dir, CONFIG_FILE_NAME);
    cfg[sizeof(cfg) - 1] = '\0';

    f = fopen(cfg, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        char *p = trim(line);
        char *eq;
        if (!*p || *p == '#' || *p == ';') continue;
        eq = strchr(p, '=');
        if (!eq) continue;
        *eq++ = '\0';
        set_option(trim(p), trim(eq));
    }
    fclose(f);
}

static void* patch_iat(HMODULE hMod, const char *dll, const char *func, void *hook) {
    BYTE *base;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_IMPORT_DESCRIPTOR *imp;
    DWORD rva;

    if (!hMod) return NULL;
    base = (BYTE *)hMod;
    dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;

    nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;

    rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!rva) return NULL;

    imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + rva);
    for (; imp->Name; imp++) {
        const char *import_name = (const char *)(base + imp->Name);
        IMAGE_THUNK_DATA *pINT;
        IMAGE_THUNK_DATA *pIAT;

        if (_stricmp(import_name, dll) != 0) continue;

        pINT = (IMAGE_THUNK_DATA *)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        pIAT = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        for (; pINT->u1.AddressOfData; pINT++, pIAT++) {
            IMAGE_IMPORT_BY_NAME *name;
            DWORD oldProtect;
            void *orig;

            if (pINT->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
            name = (IMAGE_IMPORT_BY_NAME *)(base + pINT->u1.AddressOfData);
            if (strcmp((const char *)name->Name, func) != 0) continue;

            orig = (void *)(uintptr_t)pIAT->u1.Function;
            VirtualProtect(&pIAT->u1.Function, sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &oldProtect);
            pIAT->u1.Function = (uintptr_t)hook;
            VirtualProtect(&pIAT->u1.Function, sizeof(uintptr_t), oldProtect, &oldProtect);
            return orig;
        }
    }

    return NULL;
}

static void WINAPI hook_Sleep(DWORD ms) {
    DWORD actual_ms = ms;
    uintptr_t retaddr = (uintptr_t)_ReturnAddress();

    if (g_cfg.enabled &&
        retaddr == FRAME_LIMITER_RETADDR &&
        ms > (DWORD)g_cfg.frame_limiter_sleep_cap_ms) {
        actual_ms = (DWORD)g_cfg.frame_limiter_sleep_cap_ms;
        log_debug("Sleep cap: requested=%lu actual=%lu ret=0x%08X\n",
                  (unsigned long)ms,
                  (unsigned long)actual_ms,
                  (unsigned int)retaddr);
    }

    g_orig_Sleep(actual_ms);
}

static void apply_hooks(void) {
    HMODULE hMain;
    void *orig;

    if (InterlockedExchange(&g_hooks_applied, 1) != 0) return;

    hMain = GetModuleHandleA(NULL);
    orig = patch_iat(hMain, "KERNEL32.dll", "Sleep", (void *)hook_Sleep);
    if (!orig) orig = patch_iat(hMain, "kernel32.dll", "Sleep", (void *)hook_Sleep);

    if (orig) {
        g_orig_Sleep = (void (WINAPI *)(DWORD))orig;
        log_debug("Sleep hook installed, cap=%dms\n", g_cfg.frame_limiter_sleep_cap_ms);
    } else {
        g_orig_Sleep = Sleep;
        log_debug("Sleep hook failed; using direct Sleep fallback\n");
    }
}

static void load_real_d3d9(void) {
    if (g_real_d3d9) return;

    g_real_d3d9 = LoadLibraryA(g_chain_path);
    if (!g_real_d3d9) {
        MessageBoxA(NULL,
            "LHP2 performance fix could not find dxvk_d3d9.dll next to the game.\n\n"
            "Expected files:\n"
            "  d3d9.dll = LHP2 fix proxy\n"
            "  dxvk_d3d9.dll = DXVK x32 d3d9.dll",
            "LHP2 Performance Fix", MB_ICONERROR | MB_OK);
        return;
    }

    real_Direct3DCreate9 = (pfn_Direct3DCreate9)GetProcAddress(g_real_d3d9, "Direct3DCreate9");
    real_Direct3DCreate9Ex = (pfn_Direct3DCreate9Ex)GetProcAddress(g_real_d3d9, "Direct3DCreate9Ex");
    real_D3DPERF_BeginEvent = (pfn_D3DPERF_BeginEvent)GetProcAddress(g_real_d3d9, "D3DPERF_BeginEvent");
    real_D3DPERF_EndEvent = (pfn_D3DPERF_EndEvent)GetProcAddress(g_real_d3d9, "D3DPERF_EndEvent");
    real_D3DPERF_GetStatus = (pfn_D3DPERF_GetStatus)GetProcAddress(g_real_d3d9, "D3DPERF_GetStatus");
    real_D3DPERF_QueryRepeatFrame = (pfn_D3DPERF_QueryRepeatFrame)GetProcAddress(g_real_d3d9, "D3DPERF_QueryRepeatFrame");
    real_D3DPERF_SetMarker = (pfn_D3DPERF_SetMarker)GetProcAddress(g_real_d3d9, "D3DPERF_SetMarker");
    real_D3DPERF_SetOptions = (pfn_D3DPERF_SetOptions)GetProcAddress(g_real_d3d9, "D3DPERF_SetOptions");
    real_D3DPERF_SetRegion = (pfn_D3DPERF_SetRegion)GetProcAddress(g_real_d3d9, "D3DPERF_SetRegion");

    log_debug("Loaded chained DXVK d3d9: %s\n", g_chain_path);
}

__declspec(dllexport) void* __stdcall Direct3DCreate9(UINT sdk) {
    load_real_d3d9();
    apply_hooks();
    return real_Direct3DCreate9 ? real_Direct3DCreate9(sdk) : NULL;
}

__declspec(dllexport) HRESULT __stdcall Direct3DCreate9Ex(UINT sdk, void **out) {
    load_real_d3d9();
    apply_hooks();
    return real_Direct3DCreate9Ex ? real_Direct3DCreate9Ex(sdk, out) : E_FAIL;
}

__declspec(dllexport) int __stdcall D3DPERF_BeginEvent(DWORD col, LPCWSTR name) {
    load_real_d3d9();
    return real_D3DPERF_BeginEvent ? real_D3DPERF_BeginEvent(col, name) : 0;
}

__declspec(dllexport) int __stdcall D3DPERF_EndEvent(void) {
    load_real_d3d9();
    return real_D3DPERF_EndEvent ? real_D3DPERF_EndEvent() : 0;
}

__declspec(dllexport) DWORD __stdcall D3DPERF_GetStatus(void) {
    load_real_d3d9();
    return real_D3DPERF_GetStatus ? real_D3DPERF_GetStatus() : 0;
}

__declspec(dllexport) BOOL __stdcall D3DPERF_QueryRepeatFrame(void) {
    load_real_d3d9();
    return real_D3DPERF_QueryRepeatFrame ? real_D3DPERF_QueryRepeatFrame() : FALSE;
}

__declspec(dllexport) void __stdcall D3DPERF_SetMarker(DWORD col, LPCWSTR name) {
    load_real_d3d9();
    if (real_D3DPERF_SetMarker) real_D3DPERF_SetMarker(col, name);
}

__declspec(dllexport) void __stdcall D3DPERF_SetOptions(DWORD opts) {
    load_real_d3d9();
    if (real_D3DPERF_SetOptions) real_D3DPERF_SetOptions(opts);
}

__declspec(dllexport) void __stdcall D3DPERF_SetRegion(DWORD col, LPCWSTR name) {
    load_real_d3d9();
    if (real_D3DPERF_SetRegion) real_D3DPERF_SetRegion(col, name);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH) {
        g_self = inst;
        DisableThreadLibraryCalls(inst);
        init_paths();
        load_config();
        if (g_cfg.debug_log) {
            g_log = fopen(g_log_path, "w");
            log_debug("LHP2 fix loaded: enabled=%d cap=%dms chain=%s\n",
                      g_cfg.enabled,
                      g_cfg.frame_limiter_sleep_cap_ms,
                      g_chain_path);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_log) {
            log_debug("LHP2 fix unloading\n");
            fclose(g_log);
            g_log = NULL;
        }
        if (g_real_d3d9) {
            FreeLibrary(g_real_d3d9);
            g_real_d3d9 = NULL;
        }
    }

    return TRUE;
}
