#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "assets.h"
#include "vendor/minhook/include/MinHook.h"
// Compile and exercise the production callbacks, not duplicate replacements.
#include "mvgl-asi.c"
typedef ReadAsset Reader;
typedef SizeAsset Sizer;
static volatile int vanilla_reads, vanilla_sizes;
__declspec(noinline) unsigned char fake_read(const char *path, void *buffer, unsigned int size) {
    (void)path; vanilla_reads++; if (size) ((char *)buffer)[0] = 'V'; return 7;
}
__declspec(noinline) intptr_t fake_size(intptr_t a, const char *path, int64_t *size, intptr_t b, int c) {
    (void)a; (void)path; (void)b; (void)c; vanilla_sizes++; *size = 9; return 7;
}
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d\n", __LINE__); return 1; } } while(0)
int main(void) {
    InitializeASI(); // A synthetic executable must fail the production activation gate.
    CHECK(assets.count == 0 && original_read == NULL && original_size == NULL);
    char resolved[4096];
    CHECK(!resolve_asset_root("missing-root", resolved));
    CHECK(CreateDirectoryA("asset-root", NULL));
    CHECK(resolve_asset_root("asset-root", resolved));
    CHECK(CreateDirectoryA("asset-root\\app_0", NULL));
    CHECK(CreateDirectoryA("asset-root\\app_0\\images", NULL));
    HANDLE file = CreateFileA("asset-root\\app_0\\images\\eyes.dds", GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(file != INVALID_HANDLE_VALUE);
    DWORD written;
    CHECK(WriteFile(file, "DDS!", 4, &written, NULL) && written == 4); CloseHandle(file);
    CHECK(snapshot(resolved, "", 0) && assets.count == 1);
    file = CreateFileA("asset-root\\app_0\\images\\eyes.dds", GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(file != INVALID_HANDLE_VALUE);
    CHECK(WriteFile(file, "changed", 7, &written, NULL) && written == 7); CloseHandle(file);
    CHECK(MH_Initialize() == MH_OK);
    CHECK(MH_CreateHook(fake_read, redirect_read, (void **)&original_read) == MH_OK);
    CHECK(MH_CreateHook(fake_size, redirect_size, (void **)&original_size) == MH_OK);
    CHECK(MH_QueueEnableHook(fake_read) == MH_OK && MH_QueueEnableHook(fake_size) == MH_OK);
    CHECK(MH_ApplyQueued() == MH_OK);
    InterlockedExchange(&hooks_ready, 1);
    Reader volatile read = fake_read; Sizer volatile size = fake_size;
    int64_t length = 0; unsigned char bytes[] = {0xaa,0,0,0,0,0xbb};
    CHECK(size(0, "app_0/images/eyes.img", &length, 0, 0) == 1 && length == 4);
    CHECK(read("APP_0\\images\\eyes.img", bytes+1, 4) == 1);
    CHECK(!memcmp(bytes+1, "DDS!", 4) && bytes[0] == 0xaa && bytes[5] == 0xbb);
    CHECK(vanilla_reads == 0 && vanilla_sizes == 0);
    CHECK(size(0, "missing", &length, 0, 0) == 7 && length == 9);
    CHECK(read("missing", bytes+1, 4) == 7 && bytes[1] == 'V');
    CHECK(vanilla_reads == 1 && vanilla_sizes == 1);
    InterlockedExchange(&hooks_ready, 0);
    CHECK(read("app_0/images/eyes.img", bytes+1, 4) == 7); // Residual detours are inert on failure.
    CHECK(MH_DisableHook(MH_ALL_HOOKS) == MH_OK);
    CHECK(read("app_0/images/eyes.img", bytes+1, 4) == 7);
    CHECK(MH_Uninitialize() == MH_OK);
    mvgl_free(&assets);
    puts("MVGL native x64 inline hooks: snapshot stability, size/read replacement, vanilla fallback and removal passed");
    return 0;
}
