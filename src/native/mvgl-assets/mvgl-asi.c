// Independent asset-only adapter. No CLR, Reloaded interfaces or archive rewriting.
// DSTS byte signatures: RyoTune/DSTS.ModLoader (MIT, attribution beside this file).
#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "assets.h"
#include "vendor/minhook/include/MinHook.h"
static MvglAssets assets;
static LONG started;
static LONG hooks_ready;
static LONG first_replacement;
static LONG first_size_override;
typedef unsigned char (*ReadAsset)(const char *, void *, unsigned int);
typedef intptr_t (*SizeAsset)(intptr_t, const char *, int64_t *, intptr_t, int);
static ReadAsset original_read;
static SizeAsset original_size;
static unsigned char redirect_read(const char *path, void *buffer, unsigned int size) {
    if (InterlockedCompareExchange(&hooks_ready, 0, 0) && mvgl_read(&assets, path, buffer, size)) {
        if (!InterlockedCompareExchange(&first_replacement, 1, 0)) {
            char message[MVGL_PATH_MAX+80];
            snprintf(message, sizeof(message), "Bepis MVGL replaced asset: %s (%u bytes)\n", path, size);
            OutputDebugStringA(message);
        }
        return 1;
    }
    return original_read(path, buffer, size);
}
static intptr_t redirect_size(intptr_t a, const char *path, int64_t *size, intptr_t b, int c) {
    const MvglAsset *asset = InterlockedCompareExchange(&hooks_ready, 0, 0) ? mvgl_find(&assets, path) : NULL;
    if (asset && size) {
        *size = (int64_t)asset->size;
        if (!InterlockedCompareExchange(&first_size_override, 1, 0)) OutputDebugStringA("Bepis MVGL replacement size reported.\n");
        return 1;
    }
    return original_size(a, path, size, b, c);
}
static void report(const char *message) { OutputDebugStringA(message); }

static int hash_matches(const char *path, const char *expected) {
    if (strlen(expected) != 64) return 0;
    BCRYPT_ALG_HANDLE algorithm = NULL; BCRYPT_HASH_HANDLE hash = NULL;
    DWORD object_size = 0, result_size = 0; unsigned char digest[32], buffer[65536];
    void *object = NULL; HANDLE file = INVALID_HANDLE_VALUE; int ok = 0;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&object_size, sizeof(object_size), &result_size, 0) < 0) goto done;
    object = HeapAlloc(GetProcessHeap(), 0, object_size); if (!object) goto done;
    if (BCryptCreateHash(algorithm, &hash, object, object_size, NULL, 0, 0) < 0) goto done;
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) goto done;
    for (;;) {
        DWORD count = 0;
        if (!ReadFile(file, buffer, sizeof(buffer), &count, NULL)) goto done;
        if (!count) break;
        if (BCryptHashData(hash, buffer, count, 0) < 0) goto done;
    }
    if (BCryptFinishHash(hash, digest, sizeof(digest), 0) < 0) goto done;
    for (size_t i = 0; i < 32; i++) {
        char hex[3]; snprintf(hex, sizeof(hex), "%02x", digest[i]);
        if (expected[i*2] != hex[0] || expected[i*2+1] != hex[1]) goto done;
    }
    ok = 1;
done:
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (hash) BCryptDestroyHash(hash);
    if (object) HeapFree(GetProcessHeap(), 0, object);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    return ok;
}

// Snapshot the selected staging root before installing hooks; size and bytes
// remain consistent even if source files change or disappear during play.
static int snapshot(const char *root, const char *relative, unsigned depth) {
    if (depth > 32) return 0;
    char folder[4096], glob[4096];
    if (snprintf(folder, sizeof(folder), "%s%s%s", root, *relative ? "\\" : "", relative) >= (int)sizeof(folder) ||
        snprintf(glob, sizeof(glob), "%s\\*", folder) >= (int)sizeof(glob)) return 0;
    WIN32_FIND_DATAA info; HANDLE search = FindFirstFileA(glob, &info);
    if (search == INVALID_HANDLE_VALUE) return 0;
    int ok = 1;
    do {
        if (!strcmp(info.cFileName, ".") || !strcmp(info.cFileName, "..")) continue;
        char key[MVGL_PATH_MAX], full[4096], normalized[MVGL_PATH_MAX];
        if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ||
            snprintf(key, sizeof(key), "%s%s%s", relative, *relative ? "/" : "", info.cFileName) >= (int)sizeof(key) ||
            !mvgl_normalize(key, normalized) ||
            snprintf(full, sizeof(full), "%s\\%s", root, key) >= (int)sizeof(full)) { ok = 0; break; }
        if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { if (!snapshot(root, key, depth+1)) { ok = 0; break; } continue; }
        HANDLE file = CreateFileA(full, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) { ok = 0; break; }
        LARGE_INTEGER size; DWORD count = 0; unsigned char *bytes = NULL;
        if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= 64*1024*1024 &&
            (uint64_t)size.QuadPart <= MVGL_MAX_BYTES - assets.total) bytes = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)size.QuadPart);
        if (!bytes || !ReadFile(file, bytes, (DWORD)size.QuadPart, &count, NULL) || count != size.QuadPart ||
            !mvgl_add(&assets, key, bytes, count)) ok = 0;
        if (bytes) HeapFree(GetProcessHeap(), 0, bytes);
        CloseHandle(file); if (!ok) break;
    } while (FindNextFileA(search, &info));
    if (ok && GetLastError() != ERROR_NO_MORE_FILES) ok = 0;
    FindClose(search); return ok;
}

static unsigned char *find_code(HMODULE module, const unsigned char *pattern, const char *mask, size_t size) {
    unsigned char *base = (unsigned char *)module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return NULL;
    IMAGE_NT_HEADERS64 *pe = (IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) return NULL;
    IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(pe); unsigned char *found = NULL;
    for (unsigned i = 0; i < pe->FileHeader.NumberOfSections; i++, section++) {
        if (!(section->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        size_t offset = section->VirtualAddress, length = section->Misc.VirtualSize;
        if (offset > pe->OptionalHeader.SizeOfImage || length > pe->OptionalHeader.SizeOfImage - offset) return NULL;
        size_t match;
        size_t count = mvgl_match_count(base + offset, length, pattern, mask, size, &match);
        if (count > 1) return NULL;
        if (!count) continue;
        if (found) return NULL;
        found = base + offset + match;
    }
    return found;
}

__declspec(dllexport) void InitializeASI(void) {
    if (InterlockedCompareExchange(&started, 1, 0)) return;
    char root[4096], hash[65], executable[4096];
    DWORD root_length = GetEnvironmentVariableA("BEPIS_MVGL_ASSET_ROOT", root, sizeof(root));
    DWORD hash_length = GetEnvironmentVariableA("BEPIS_MVGL_EXE_SHA256", hash, sizeof(hash));
    HMODULE module = GetModuleHandleA(NULL);
    DWORD exe_length = GetModuleFileNameA(module, executable, sizeof(executable));
    if (!root_length || root_length >= sizeof(root) || hash_length != 64 || !exe_length || exe_length >= sizeof(executable)) {
        report("Bepis MVGL blocked: missing or oversized activation fields.\n"); return;
    }
    const char *name = strrchr(executable, '\\'); name = name ? name+1 : executable;
    if (
        root[1] != ':' || (root[2] != '\\' && root[2] != '/') ||
        GetFileAttributesA(root) == INVALID_FILE_ATTRIBUTES || (GetFileAttributesA(root) & FILE_ATTRIBUTE_REPARSE_POINT) ||
        strcmp(name, "Digimon Story Time Stranger.exe") || !hash_matches(executable, hash)) {
        report("Bepis MVGL blocked: explicit asset root and exact x64 DSTS executable hash required.\n"); return;
    }
    // Factual x64 instruction signatures from MIT-licensed DSTS adapter.
    static const unsigned char read_pattern[] = {0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0xb0,0x01,0,0};
    static const unsigned char size_pattern[] = {0x48,0x89,0x5c,0x24,0,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xac,0x24,0,0,0,0,0x48,0x81,0xec,0xc0,0x02,0,0};
    void *read_address = find_code(module, read_pattern, "xxxxxxxxxxxxx", sizeof(read_pattern));
    void *size_address = find_code(module, size_pattern, "xxxx?xxxxxxxxxxxxxxx????xxxxxxx", sizeof(size_pattern));
    if (!read_address || !size_address || read_address == size_address || !snapshot(root, "", 0) || !assets.count) goto blocked;
    if (MH_Initialize() != MH_OK) goto blocked;
    if (MH_CreateHook(read_address, redirect_read, (void **)&original_read) != MH_OK ||
        MH_CreateHook(size_address, redirect_size, (void **)&original_size) != MH_OK ||
        MH_QueueEnableHook(read_address) != MH_OK || MH_QueueEnableHook(size_address) != MH_OK || MH_ApplyQueued() != MH_OK) {
        MH_STATUS read_disabled = MH_DisableHook(read_address);
        MH_STATUS size_disabled = MH_DisableHook(size_address);
        if ((read_disabled == MH_OK || read_disabled == MH_ERROR_DISABLED || read_disabled == MH_ERROR_NOT_CREATED) &&
            (size_disabled == MH_OK || size_disabled == MH_ERROR_DISABLED || size_disabled == MH_ERROR_NOT_CREATED)) {
            MH_RemoveHook(read_address); MH_RemoveHook(size_address); MH_Uninitialize();
        }
        // If detour removal fails, keep valid original trampolines. hooks_ready
        // remains zero so any residual callback delegates directly to vanilla.
        goto blocked;
    }
    InterlockedExchange(&hooks_ready, 1);
    report("Bepis MVGL asset hooks active (experimental; no gameplay attestation).\n"); return;
blocked:
    mvgl_free(&assets); report("Bepis MVGL blocked: asset snapshot or unique hook validation failed.\n");
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) { (void)instance; (void)reason; (void)reserved; return TRUE; }
