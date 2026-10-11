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
static int audio_calls, reject_audio;
static const void *last_audio;
static void *last_binder, *last_work;
static const char *last_path;
static int last_audio_length, last_work_size;
__declspec(noinline) void *fake_audio(void *data,int length,void *binder,const char *path,void *work,int work_size) {
    audio_calls++;last_audio=data;last_audio_length=length;last_binder=binder;last_path=path;last_work=work;last_work_size=work_size;
    if(reject_audio && length==11) return NULL;
    return (void *)(uintptr_t)123;
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
    char audio_hash[65], audio_key[96];
    CHECK(memory_sha256("original-audio",14,audio_hash));
    snprintf(audio_key,sizeof(audio_key),"_bepis_audio/%s.acb",audio_hash);
    CHECK(mvgl_add(&assets,audio_key,"replacement",11));
    CHECK(MH_Initialize() == MH_OK);
    CHECK(MH_CreateHook(fake_audio,redirect_audio_bank,(void **)&original_audio_bank)==MH_OK);
    CHECK(MH_QueueEnableHook(fake_audio)==MH_OK);
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
    InterlockedExchange(&hooks_ready,1);
    LoadAudioBank volatile load=fake_audio;
    CHECK(load("original-audio",14,(void *)1,"vanilla",(void *)2,100)==(void *)(uintptr_t)123);
    CHECK(audio_calls==1 && last_audio_length==11 && !memcmp(last_audio,"replacement",11) && !last_binder && !last_path && !last_work && !last_work_size);
    reject_audio=1;
    CHECK(load("original-audio",14,(void *)1,"vanilla",(void *)2,100)==(void *)(uintptr_t)123);
    CHECK(audio_calls==3 && last_audio_length==14 && last_binder==(void *)1 && last_work==(void *)2 && last_work_size==100 && !strcmp(last_path,"vanilla"));
    CHECK(load("other",5,NULL,NULL,NULL,0)==(void *)(uintptr_t)123 && last_audio_length==5);
    InterlockedExchange(&hooks_ready,0);
    CHECK(load("original-audio",14,NULL,NULL,NULL,0)==(void *)(uintptr_t)123 && last_audio_length==14);
    CHECK(MH_DisableHook(MH_ALL_HOOKS) == MH_OK);
    CHECK(read("app_0/images/eyes.img", bytes+1, 4) == 7);
    CHECK(MH_Uninitialize() == MH_OK);
    mvgl_free(&assets);
    puts("MVGL native x64 inline hooks: snapshot stability, size/read replacement, vanilla fallback and removal passed");
    return 0;
}
