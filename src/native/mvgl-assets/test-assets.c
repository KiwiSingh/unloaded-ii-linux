#include "assets.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    MvglAssets store = {0}; char path[MVGL_PATH_MAX];
    const char *bad[] = {"/a", "C:/a", "../a", "a/../b", "a/./b", "a//b", "a/", "", "a?b", "\\\\server\\x"};
    for (size_t i = 0; i < sizeof(bad)/sizeof(*bad); i++) assert(!mvgl_normalize(bad[i], path));
    assert(mvgl_add(&store, "app_0/images/eyes.dds", "DDS!", 4));
    assert(!mvgl_add(&store, "APP_0\\IMAGES\\EYES.DDS", "dup", 3));
    assert(mvgl_find(&store, "APP_0\\images\\eyes.img")->size == 4);
    unsigned char bytes[] = {0xaa, 0, 0, 0, 0, 0xbb};
    assert(!mvgl_read(&store, "app_0/images/eyes.img", bytes+1, 3));
    assert(bytes[1] == 0 && bytes[5] == 0xbb);
    assert(mvgl_read(&store, "app_0/images/eyes.img", bytes+1, 4));
    assert(bytes[0] == 0xaa && bytes[5] == 0xbb && !memcmp(bytes+1, "DDS!", 4));
    assert(!mvgl_read(&store, "missing", bytes+1, 4));
    assert(!mvgl_add(&store, "big", "x", MVGL_MAX_BYTES));
    const unsigned char data[] = {1,2,3,1,2,3}, pattern[] = {1,2,3};
    assert(mvgl_unique_match(data, 6, pattern, "xxx", 3) == SIZE_MAX);
    assert(mvgl_unique_match(data, 3, pattern, "xxx", 3) == 0);
    assert(mvgl_unique_match(data, 2, pattern, "xxx", 3) == SIZE_MAX);
    assert(MVGL_MAX_BYTES == 1024u * 1024u * 1024u);
    store.total = MVGL_MAX_BYTES - 4;
    assert(mvgl_add(&store, "boundary", "test", 4));
    assert(store.total == MVGL_MAX_BYTES);
    assert(!mvgl_add(&store, "overflow", "x", 1));
    store.total = MVGL_MAX_BYTES + 1ull;
    assert(!mvgl_add(&store, "underflow", "x", 1));
    mvgl_free(&store); assert(!store.count && !store.total && !store.items);
    puts("MVGL path, DDS alias, immutable copy, bounds, duplicate and ambiguous-signature tests passed");
    return 0;
}
